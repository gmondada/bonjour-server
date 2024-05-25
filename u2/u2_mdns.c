//
//  u2_mdns.c
//
//  Created by Gabriele Mondada on March 29, 2024.
//  Copyright © 2024 Gabriele Mondada.
//
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the “Software”),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "u2_base.h"
#include "u2_mdns.h"


static bool _add_answer(struct u2_dns_msg_builder *builder, const struct u2_dns_record *record, bool tear_down)
{
    int ttl = tear_down ? 0 : record->ttl;

    switch (record->type) {
        case U2_DNS_RR_TYPE_A:
            return u2_dns_msg_builder_add_rr_a(builder, record->domain->name, record->cache_flush, ttl, record->a.addr);
        case U2_DNS_RR_TYPE_TXT:
            return u2_dns_msg_builder_add_rr_name(builder, record->domain->name, U2_DNS_RR_TYPE_TXT, record->cache_flush, ttl, record->txt.name);
        case U2_DNS_RR_TYPE_SRV:
            return u2_dns_msg_builder_add_rr_srv(builder, record->domain->name, record->cache_flush, ttl, 0, 0, record->srv.port, record->srv.name);
        case U2_DNS_RR_TYPE_PTR:
            return u2_dns_msg_builder_add_rr_name(builder, record->domain->name, U2_DNS_RR_TYPE_PTR, record->cache_flush, ttl, record->ptr.name);
        case U2_DNS_RR_TYPE_NSEC: {
            // emit a nsec record which lists all record types available in this domain
            u2_dns_type_mask_t type_mask = 0;
            for (int r = 0; r < record->domain->record_count; r++) {
                const struct u2_dns_record *record_it = record->domain->record_list[r];
                if (record_it != record) {
                    if (record_it->type < 8 * sizeof(u2_dns_type_mask_t))
                        type_mask |= (u2_dns_type_mask_t)1 << record_it->type;
                    else
                        U2_FATAL("unsupported record type %d in NSEC\n", record->type);
                }
            }
            return u2_dns_msg_builder_add_rr_single_domain_nsec(builder, record->domain->name, record->cache_flush, ttl, type_mask);
        }
        default:
            return false;
    }
}

static inline const struct u2_mdns_response_record *_find_record(const struct u2_mdns_response_record *record_list, int record_count, const struct u2_dns_record *record)
{
    for (int i = 0; i < record_count; i++) {
        const struct u2_mdns_response_record *rr = &record_list[i];
        if (rr->record == record)
            return rr;
    }
    return NULL;
}

/**
 * This function decodes the message and fill the `proc->record_list` array.
 */
static void _decode_questions(struct u2_mdns_query_proc *proc)
{
    assert(!proc->decoding_error);

    const int record_max = U2_ARRAY_LEN(proc->record_list);
    proc->answer_record_count = 0;
    proc->additional_record_count = 0;

    for (;;) {
        if (proc->question_index >= proc->question_count)
            break;

        if (proc->answer_record_count >= record_max)
            break;

        struct u2_dns_msg_entry entry;
        int rv = u2_dns_msg_reader_get_entry(&proc->reader, proc->question_index, &entry);
        if (rv < 0) {
            proc->decoding_error = rv;
            break;
        }

        int type = u2_dns_msg_entry_get_question_type(&entry);
        int klass = u2_dns_msg_entry_get_question_class(&entry);
        if (klass != 1 && klass != 255) {
            proc->question_index++;
            continue;
        }

        char name[256];
        u2_dns_name_init(name, 256);
        bool valid_name = u2_dns_name_append_compressed_name(name, 256, entry.data, entry.name_pos);
        if (!valid_name) {
            proc->decoding_error = -1;
            break;
        }

        bool overflow = false;
        int first_record = proc->answer_record_count;
        for (int d = 0; d < proc->database->domain_count; d++) {
            const struct u2_dns_domain *domain = proc->database->domain_list[d];
            if (!u2_dns_name_compare(domain->name, name)) {
                bool found = false;
                const struct u2_dns_record *nsec_record = NULL;
                for (int r = 0; r < domain->record_count; r++) {
                    const struct u2_dns_record *record = domain->record_list[r];
                    if (record->type == type) {
                        found = true;
                        if (proc->answer_record_count >= record_max) {
                            overflow = true;
                            break;
                        }
                        proc->record_list[proc->answer_record_count].category = U2_DNS_RR_CATEGORY_ANSWER;
                        proc->record_list[proc->answer_record_count].record = record;
                        proc->answer_record_count++;
                    } else if (record->type == U2_DNS_RR_TYPE_NSEC) {
                        nsec_record = record;
                    }
                }
                if (!found && nsec_record && !overflow) {
                    /*
                     * Here we avoid redundant nsec answers. We can do that only
                     * for answers stored in the list.
                     */
                    if (!_find_record(proc->record_list, proc->answer_record_count, nsec_record)) {
                        if (proc->answer_record_count >= record_max) {
                            overflow = true;
                            break;
                        }
                        proc->record_list[proc->answer_record_count].category = U2_DNS_RR_CATEGORY_ANSWER;
                        proc->record_list[proc->answer_record_count].record = nsec_record;
                        proc->answer_record_count++;
                    }
                }
            }
        }

        if (overflow) {
            // we cannot store all records corresponding to this question
            if (first_record == 0) {
                /*
                 * This question has so many records that they do not fit in the record list.
                 * We ingore it, considering it as answered, otherwise it will be decoded again
                 * and again, with the same result, running in loop forever.
                 */
                proc->answer_record_count = 0;
            } else {
                /*
                 * This answer cannot be answered entierly. We consider it as not answered.
                 * In this way it will be decoded again later, hoping there will be more space
                 * available in the record list.
                 */
                proc->answer_record_count = first_record;
                break;
            }
        }

        proc->question_index++;
    }
}

/**
 * Removes answers that are set as already known in the request.
 * Silently ignore all decoding errors.
 */
static void _remove_known_answers(struct u2_mdns_query_proc *proc)
{
    if (proc->decoding_error)
        return;

    int qcount = u2_dns_msg_reader_get_question_count(&proc->reader);
    int acount = u2_dns_msg_reader_get_answer_rr_count(&proc->reader);

    for (int a = 0; a < acount; a++) {
        struct u2_dns_msg_entry entry;
        int rv = u2_dns_msg_reader_get_entry(&proc->reader, qcount + a, &entry);
        if (rv < 0)
            return;

        int klass = u2_dns_msg_entry_get_rr_class(&entry);
        if (klass != 1 && klass != 255)
            continue;

        int type = u2_dns_msg_entry_get_rr_type(&entry);
        switch (type) {
            case U2_DNS_RR_TYPE_PTR: {
                int span = u2_dns_msg_name_span(entry.data, entry.rdata_pos + entry.rdata_len, entry.rdata_pos);
                if (span != entry.rdata_len)
                    continue;
                break;
            }
            // TODO: manage other types
            default:
                continue;
        }

        int ttl = u2_dns_msg_entry_get_rr_ttl(&entry);

        char name[256];
        u2_dns_name_init(name, 256);
        bool valid_name = u2_dns_name_append_compressed_name(name, 256, entry.data, entry.name_pos);
        if (!valid_name)
            return;

        char ptr_name[256];
        bool ptr_name_defined = false;

        for (int r = 0; r < proc->answer_record_count; r++) {
            struct u2_mdns_response_record *record = proc->record_list + r;
            if (record->category != U2_DNS_RR_CATEGORY_ANSWER)
                continue;
            if (record->record->type != type)
                continue;
            if (u2_dns_name_compare(name, record->record->domain->name))
                continue;
            switch (type) {
                case U2_DNS_RR_TYPE_PTR: {
                    if (!ptr_name_defined) {
                        u2_dns_name_init(ptr_name, sizeof(ptr_name));
                        bool valid_name = u2_dns_name_append_compressed_name(ptr_name, sizeof(ptr_name), entry.data, entry.rdata_pos);
                        if (!valid_name)
                            return;
                        ptr_name_defined = true;
                    }
                    if (u2_dns_name_compare(ptr_name, record->record->ptr.name))
                        continue;
                    break;
                }
            }
            if (ttl < record->record->ttl / 2)
                continue;
            record->category = U2_DNS_RR_CATEGORY_NONE; // invalidate answer
            break;
        }
    }

    // remove invalidated answers

    int compact_r = 0;
    for (int r = 0; r < proc->answer_record_count; r++) {
        struct u2_mdns_response_record *record = proc->record_list + r;
        if (record->category != U2_DNS_RR_CATEGORY_NONE) {
            if (compact_r != r)
                proc->record_list[compact_r] = proc->record_list[r];
            compact_r++;
        }
    }
    proc->answer_record_count = compact_r;
    assert(!proc->additional_record_count);
}

/**
 * Fill the remaining slots in the `proc->record_list` array with non requested answers.
 * Silently ignore all decoding errors.
 */
static void _generate_additional_response_records(struct u2_mdns_query_proc *proc)
{
    if (proc->decoding_error)
        return;

    const int record_max = U2_ARRAY_LEN(proc->record_list);
    int record_index = proc->answer_record_count;

    for (int a = 0; a < proc->answer_record_count; a++) {
        if (record_index >= record_max)
            break;

        struct u2_mdns_response_record *rr = proc->record_list + a;

        if (rr->category != U2_DNS_RR_CATEGORY_ANSWER && rr->category != U2_DNS_RR_CATEGORY_ADDITIONAL)
            continue;

        const char *name = NULL;
        switch (rr->record->type) {
            case U2_DNS_RR_TYPE_PTR:
                name = rr->record->ptr.name;
                break;
            case U2_DNS_RR_TYPE_SRV:
                name = rr->record->srv.name;
                break;
            default:
                break;
        }

        if (!name)
            continue;

        for (int d = 0; d < proc->database->domain_count; d++) {
            if (record_index >= record_max)
                break;
            const struct u2_dns_domain *domain = proc->database->domain_list[d];
            if (domain->name == name) {
                for (int r = 0; r < domain->record_count; r++) {
                    if (record_index >= record_max)
                        break;
                    const struct u2_dns_record *record = domain->record_list[r];
                    /*
                     * Here we avoid redundant answers. We can do that only
                     * for answers stored in the list.
                     */
                    if (!_find_record(proc->record_list, record_index, record)) {
                        proc->record_list[record_index].category = U2_DNS_RR_CATEGORY_ADDITIONAL;
                        proc->record_list[record_index].record = record;
                        record_index++;
                    }
                }
            }
        }
    }

    proc->additional_record_count = record_index - proc->answer_record_count;
}

void u2_mdsn_query_proc_init(struct u2_mdns_query_proc *proc, const void *msg, size_t size, const struct u2_dns_database *database)
{
    memset(proc, 0, sizeof(*proc));

    proc->database = database;

    int rv = u2_dns_msg_reader_init(&proc->reader, msg, size);
    if (rv < 0) {
        proc->decoding_error = rv;
        return;
    }

    int flags = u2_dns_msg_reader_get_flags(&proc->reader);
    if (flags & U2_DNS_MSG_FLAG_QR) {
        proc->question_count = 0;
    } else {
        proc->question_count = u2_dns_msg_reader_get_question_count(&proc->reader);
    }
}

/**
 * Generate an answer message and return the real size of the message.
 * It can happen that there are several answer messages to be generated. The
 * caller has to repeat the call to this function until 0 is returned.
 * This function tries to generate messages up to the `ideal_size`.
 * If a single record is so big that `ideal_size` is not enough, the message is
 * extended up to `max_size`. If this is still not enough, the record is discarded.
 * Ideally, `ideal_size` should correspond to the MTU, or eventually to the
 * preferred DNS message size (512). `max_size` must be equal or bigger than
 * `ideal_size`. It should correspond to the maximum size of a mDNS
 * message which, including IP and UDP headers, is 9000 bytes.
 */
size_t u2_mdns_query_proc_run(struct u2_mdns_query_proc *proc, void *out_msg, size_t ideal_size, size_t max_size)
{
    for (;;) {
        bool pending_questions = !proc->decoding_error && (proc->question_index < proc->question_count);
        bool pending_records = proc->emitter.record_index < proc->answer_record_count;

        if (pending_records) {
            size_t out_size = u2_mdns_emitter_run(&proc->emitter, out_msg, ideal_size, max_size);
            if (out_size)
                return out_size;
            assert(proc->emitter.record_index >= proc->answer_record_count);
        } else if (pending_questions) {
            _decode_questions(proc);
            _remove_known_answers(proc);
            _generate_additional_response_records(proc);
            u2_mdns_emitter_init(&proc->emitter, proc->record_list, proc->answer_record_count, proc->additional_record_count, false);
        } else {
            return 0;
        }
    }
}

void u2_mdns_emitter_init(struct u2_mdns_emitter *emitter, const struct u2_mdns_response_record *record_list, int mandatory_record_count, int optional_record_count, bool tear_down)
{
    emitter->record_list = record_list;
    emitter->mandatory_record_count = mandatory_record_count;
    emitter->optional_record_count = optional_record_count;
    emitter->record_index = 0;
    emitter->tear_down = tear_down;
}

size_t u2_mdns_emitter_run(struct u2_mdns_emitter *emitter, void *out_msg, size_t ideal_size, size_t max_size)
{
    if (ideal_size > max_size)
        U2_FATAL("u2_mdsn: inconsistent output message size");

    if (emitter->record_index >= emitter->mandatory_record_count)
        return 0;

    struct u2_dns_msg_builder builder;
    u2_dns_msg_builder_init(&builder, out_msg, ideal_size, 0, U2_DNS_MSG_FLAG_QR | U2_DNS_MSG_FLAG_AA);
    enum u2_dns_rr_category category = U2_DNS_RR_CATEGORY_NONE;

    // emit mandatory records

    int first_record_index = emitter->record_index;

    for (;;) {
        if (emitter->record_index >= emitter->mandatory_record_count)
            break;
        const struct u2_mdns_response_record *rr = emitter->record_list + emitter->record_index;
        if (category != rr->category) {
            category = rr->category;
            u2_dns_msg_builder_set_category(&builder, rr->category);
        }
        bool added = _add_answer(&builder, rr->record, emitter->tear_down);
        if (!added) {
            // output msg is full
            if (emitter->record_index == first_record_index) {
                /*
                 * This record alone does not fit into an ideal message.
                 * Let's try to use the biggest allowed message.
                 */
                struct u2_dns_msg_builder builder;
                u2_dns_msg_builder_init(&builder, out_msg, max_size, 0, U2_DNS_MSG_FLAG_QR | U2_DNS_MSG_FLAG_AA);
                bool added = _add_answer(&builder, rr->record, emitter->tear_down);
                if (added) {
                    emitter->record_index++;
                    return u2_dns_msg_builder_get_size(&builder);
                }
                // record really too big - ignore it
            } else {
                // let keep the message as it is, remaining records will be sent later
                return u2_dns_msg_builder_get_size(&builder);
            }
        }
        emitter->record_index++;
    }

    // emit optinal records

    assert(emitter->record_index >= emitter->mandatory_record_count);

    for (;;) {
        if (emitter->record_index >= emitter->mandatory_record_count + emitter->optional_record_count)
            break;
        const struct u2_mdns_response_record *rr = emitter->record_list + emitter->record_index;
        if (category != rr->category) {
            category = rr->category;
            u2_dns_msg_builder_set_category(&builder, rr->category);
        }
        bool added = _add_answer(&builder, rr->record, emitter->tear_down);
        if (!added)
            break;
        emitter->record_index++;
    }

    return u2_dns_msg_builder_get_size(&builder);
}
