//
//  u2_mdns.c
//
//  Created by Gabriele Mondada on March 29, 2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#include <stdio.h>
#include "u2_base.h"
#include "u2_mdns.h"


struct u2_dns_response_record {
    enum u2_dns_rr_category category;
    const struct u2_dns_record *record;
};


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
                        printf("warning: unsupported record type %d in NSEC\n", record->type);
                }
            }
            return u2_dns_msg_builder_add_rr_single_domain_nsec(builder, record->domain->name, record->cache_flush, ttl, type_mask);
        }
        default:
            return false;
    }
}

static inline const struct u2_dns_response_record *_find_record(const struct u2_dns_response_record *record_list, int record_count, const struct u2_dns_record *record)
{
    for (int i = 0; i < record_count; i++) {
        const struct u2_dns_response_record *rr = &record_list[i];
        if (rr->record == record)
            return rr;
    }
    return NULL;
}

/**
 * This function processes all questions and add the corresponding answers in the given list, starting at the given index.
 * It then returns the total number of records present in the list. If the list is too short to store all aswers, the returned
 * value is an estimation of the total number of records we would have if `record_max` was infinite. This estimation
 * is never below the real number and can be used to allocate a buffer able to contains all records.
 */
static int _decode_questions(void *msg_in, size_t size_in, const struct u2_dns_database *database, struct u2_dns_response_record *record_list, int record_count, int record_max)
{
    struct u2_dns_msg_reader reader;
    int rv = u2_dns_msg_reader_init(&reader, msg_in, size_in);
    if (rv < 0)
        return 0;

    int flags = u2_dns_msg_reader_get_flags(&reader);
    if (flags & U2_DNS_MSG_FLAG_QR)
        return 0;

    struct u2_dns_msg_entry entries[32];
    int entry_count = u2_dns_msg_reader_get_entry_count(&reader);
    if (entry_count > U2_ARRAY_LEN(entries))
        entry_count = U2_ARRAY_LEN(entries);
    for (int i = 0; i < entry_count; i++) {
        rv = u2_dns_msg_reader_get_entry(&reader, i, entries + i);
        if (rv < 0)
            return 0;
    }

    int qcount = u2_dns_msg_reader_get_question_count(&reader);
    if (qcount > entry_count)
        qcount = entry_count; // TODO: should we generate a single message with the truncated flag or several messages?

    for (int q = 0; q < qcount; q++) {
        const struct u2_dns_msg_entry *entry = entries + q;

        int type = u2_dns_msg_entry_get_question_type(entry);
        int klass = u2_dns_msg_entry_get_question_class(entry);
        if (klass != 1 && klass != 255)
            continue;

        char name[256];
        u2_dns_name_init(name, 256);
        u2_dns_name_append_compressed_name(name, 256, msg_in, entry->name_pos);

        for (int d = 0; d < database->domain_count; d++) {
            const struct u2_dns_domain *domain = database->domain_list[d];
            if (!u2_dns_name_compare(domain->name, name)) {
                bool found = false;
                const struct u2_dns_record *nsec_record = NULL;
                for (int r = 0; r < domain->record_count; r++) {
                    const struct u2_dns_record *record = domain->record_list[r];
                    if (record->type == type) {
                        if (record_count < record_max) {
                            record_list[record_count].category = U2_DNS_RR_CATEGORY_ANSWER;
                            record_list[record_count].record = record;
                        }
                        record_count++;
                        found = true;
                    } else if (record->type == U2_DNS_RR_TYPE_NSEC) {
                        nsec_record = record;
                    }
                }
                if (!found && nsec_record) {
                    /*
                     * Here we avoid redundant answers. We can do that only
                     * for answers stored in the list. When the list is full
                     * record_count could be overestimated.
                     */
                    if (!_find_record(record_list, U2_MIN(record_count, record_max), nsec_record)) {
                        if (record_count < record_max) {
                            record_list[record_count].category = U2_DNS_RR_CATEGORY_ANSWER;
                            record_list[record_count].record = nsec_record;
                        }
                        record_count++;
                    }
                }
            }
        }
    }

    // remove known answers

    if (!record_count)
        return 0;

    int acount = u2_dns_msg_reader_get_answer_rr_count(&reader);
    if (acount > entry_count - qcount)
        acount = entry_count - qcount;

    for (int a = qcount; a < qcount + acount; a++) {
        const struct u2_dns_msg_entry *entry = entries + a;

        int klass = u2_dns_msg_entry_get_rr_class(entry);
        if (klass != 1 && klass != 255)
            continue;

        int type = u2_dns_msg_entry_get_rr_type(entry);
        switch (type) {
            case U2_DNS_RR_TYPE_PTR: {
                int span = u2_dns_msg_name_span(msg_in, size_in, entry->rdata_pos);
                if (span != entry->rdata_len)
                    continue;
                break;
            }
            // TODO: manage other types
            default:
                continue;
        }

        int ttl = u2_dns_msg_entry_get_rr_ttl(entry);

        char name[256];
        u2_dns_name_init(name, 256);
        u2_dns_name_append_compressed_name(name, 256, msg_in, entry->name_pos);

        char buf[256];
        bool buf_in_use = false;

        for (int r = 0; r < record_count; r++) {
            struct u2_dns_response_record *record = record_list + r;
            if (record->category != U2_DNS_RR_CATEGORY_ANSWER)
                continue;
            if (record->record->type != type)
                continue;
            if (u2_dns_name_compare(name, record->record->domain->name))
                continue;
            switch (type) {
                case U2_DNS_RR_TYPE_PTR: {
                    if (!buf_in_use) {
                        u2_dns_name_init(buf, sizeof(buf));
                        u2_dns_name_append_compressed_name(buf, sizeof(buf), msg_in, entry->rdata_pos);
                        buf_in_use = true;
                    }
                    if (u2_dns_name_compare(buf, record->record->ptr.name))
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
    // TODO

    return record_count;
}

static int _generate_additional_response_records(const struct u2_dns_database *database, struct u2_dns_response_record *record_list, int record_count, int record_max)
{
    for (int i = 0; i < record_count; i++) {
        struct u2_dns_response_record *rr = &record_list[i];

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

        for (int d = 0; d < database->domain_count; d++) {
            const struct u2_dns_domain *domain = database->domain_list[d];
            if (domain->name == name) {
                for (int r = 0; r < domain->record_count; r++) {
                    const struct u2_dns_record *record = domain->record_list[r];
                    /*
                     * Here we avoid redundant answers. We can do that only
                     * for answers stored in the list. When the list is full
                     * record_count could be overestimated.
                     */
                    if (!_find_record(record_list, U2_MIN(record_count, record_max), record)) {
                        if (record_count < record_max) {
                            record_list[record_count].category = U2_DNS_RR_CATEGORY_ADDITIONAL;
                            record_list[record_count].record = record;
                        }
                        record_count++;
                    }
                }
            }
        }
    }
    return record_count;
}

static size_t _emit_response(void *out_msg, size_t out_msg_max, struct u2_dns_response_record *record_list, int record_count)
{
    struct u2_dns_msg_builder builder;
    u2_dns_msg_builder_init(&builder, out_msg, out_msg_max, 0, U2_DNS_MSG_FLAG_QR | U2_DNS_MSG_FLAG_AA);

    for (int i = 0; i < record_count; i++) {
        struct u2_dns_response_record *rr = &record_list[i];
        if (rr->category == U2_DNS_RR_CATEGORY_ANSWER) {
            _add_answer(&builder, rr->record, false);
        }
    }

    u2_dns_msg_builder_set_category(&builder, U2_DNS_RR_CATEGORY_ADDITIONAL);

    for (int i = 0; i < record_count; i++) {
        struct u2_dns_response_record *rr = &record_list[i];
        if (rr->category == U2_DNS_RR_CATEGORY_ADDITIONAL) {
            _add_answer(&builder, rr->record, false);
        }
    }

    int size_out = builder.size;
    if (size_out <= 12)
        return 0;
    return size_out;
}

size_t u2_mdns_process_query(const struct u2_dns_database *database, void *msg_in, size_t size_in, void *msg_out, size_t max_out)
{

    struct u2_dns_response_record response_record_list[32];
    int response_record_count = _decode_questions(msg_in, size_in, database, response_record_list, 0, U2_ARRAY_LEN(response_record_list));

    response_record_count = _generate_additional_response_records(database, response_record_list, response_record_count, U2_ARRAY_LEN(response_record_list));

    if (response_record_count > U2_ARRAY_LEN(response_record_list))
        response_record_count = U2_ARRAY_LEN(response_record_list);

    return _emit_response(msg_out, max_out, response_record_list, response_record_count);
}

size_t u2_mdns_generate_unsolicited_announcement(const struct u2_dns_record **record_list, int record_count, bool tear_down, void *msg_out, size_t max_out)
{
    struct u2_dns_msg_builder builder;
    u2_dns_msg_builder_init(&builder, msg_out, max_out, 0, U2_DNS_MSG_FLAG_QR | U2_DNS_MSG_FLAG_AA);

    for (int i = 0; i < record_count; i++) {
        const struct u2_dns_record *r = record_list[i];
        _add_answer(&builder, r, tear_down);
    }

    int size_out = builder.size;
    if (size_out <= 12)
        return 0;
    return size_out;
}
