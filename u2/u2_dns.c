//
//  u2_dns.c
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

#include "u2_base.h"
#include "u2_dns.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <limits.h>


static int _skip_name(const void *data, int pos, int max)
{
    const uint8_t *in = data;
    int i = pos;
    for (;;) {
        if (i >= max)
            return -1;
        uint8_t count = in[i];
        if (count == 0) {
            return i + 1;
        } else if (count < 0xc0) {
            i += 1 + count;
            if (i >= max)
                return -1;
        } else if (count >= 0xc0) {
            if (i + 2 > max)
                return -1;
            uint16_t ptr = (uint16_t)in[i] << 8 | (uint16_t)in[i + 1];
            ptr &= 0x3fff;
            i += 2;
            // check that the pointer points backward
            if (ptr >= pos)
                return -1;
            // check that name is valid
            int rv = _skip_name(data, ptr, pos);
            if (rv < 0)
                return -1;
            return i;
        } else {
            return -1;
        }
    }
}

static uint8_t _reverse_bit_order(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

static int _get_next_entry(struct u2_dns_msg_reader *reader, struct u2_dns_msg_entry *entry)
{
    int pos = reader->pos;

    int pos1 = pos;
    int rv = _skip_name(reader->data, pos, reader->size);
    if (rv < 0)
        return -1;
    pos = rv;
    int pos2 = pos;

    // parse question
    if (reader->index < reader->q_count) {
        if (pos + 4 > reader->size)
            return -1;
        pos += 4;
        struct u2_dns_msg_entry tmp = {
            .data = reader->data,
            .name_pos = pos1,
            .type_pos = pos2,
            .rdata_len = 0,
            .rdata_pos = pos
        };
        *entry = tmp;
        reader->index++;
        reader->pos = pos;
        return 0;
    }

    // parse resource record
    if (pos + 10 > reader->size)
        return -1;
    pos += 8;
    int rsize = u2_dns_msg_get_field_u16(reader->data, pos);
    pos += 2;
    int pos3 = pos;
    if (pos + rsize > reader->size)
        return -1;
    pos += rsize;
    struct u2_dns_msg_entry tmp = {
        .data = reader->data,
        .name_pos = pos1,
        .type_pos = pos2,
        .rdata_len = rsize,
        .rdata_pos = pos3
    };
    *entry = tmp;
    reader->index++;
    reader->pos = pos;
    return 0;
}

static int _move_to_entry(struct u2_dns_msg_reader *reader, int index)
{
    if (index < 0 || index > reader->q_count + reader->rr_count)
        return -1;
    if (reader->index > index) {
        // rewind
        reader->pos = 12;
        reader->index = 0;
    }
    while (reader->index < index) {
        struct u2_dns_msg_entry entry;
        int rv = _get_next_entry(reader, &entry);
        if (rv < 0)
            return -1;
    }
    return 0;
}

const char *u2_dns_msg_type_to_str(int type)
{
    switch (type) {
        case U2_DNS_RR_TYPE_A:
            return "A";
        case U2_DNS_RR_TYPE_NS:
            return "NS";
        case U2_DNS_RR_TYPE_CNAME:
            return "CNAME";
        case U2_DNS_RR_TYPE_PTR:
            return "PTR";
        case U2_DNS_RR_TYPE_TXT:
            return "TXT";
        case U2_DNS_RR_TYPE_AAAA:
            return "AAAA";
        case U2_DNS_RR_TYPE_SRV:
            return "SRV";
        case U2_DNS_RR_TYPE_OPT:
            return "OPT";
        case U2_DNS_RR_TYPE_NSEC:
            return "NSEC";
        case U2_DNS_RR_TYPE_ANY:
            return "ANY";
        default:
            return "?";
    }
}

int u2_dns_msg_name_span(const void *msg, size_t size, int pos)
{
    int end = _skip_name(msg, pos, (int)size);
    if (end < 0)
        return -1;
    return end - pos;
}

void u2_dns_name_init(char *name, size_t size)
{
    if (size < 1)
        U2_FATAL("u2_dns: name buffer too small");
    name[0] = 0;
}

/**
 * Return the entire name length, including the ending zero label.
 * The given name must be a valid dns name, without compression.
 */
int u2_dns_name_length(const void *name)
{
    const uint8_t *in = name;
    int i = 0;
    for (;;) {
        uint8_t count = in[i];
        if (count == 0)
            return i + 1;
        if (count >= 0xc0)
            U2_FATAL("u2_dns: bad name format");
        i += 1 + count;
    }
}

bool u2_dns_name_append_label(char *name, size_t size, char *label)
{
    int pos = _skip_name(name, 0, (int)size);
    if (pos < 0)
        return false;
    assert(pos > 0);
    size_t len = strlen(label);
    if (len > 63)
        return false;
    if (pos + len + 1 > size)
        return false;
    name[pos - 1] = (uint8_t)len;
    memcpy(name + pos, label, len);
    name[pos + len] = 0;
    return true;
}

bool u2_dns_name_append_compressed_name(char *name, size_t size, const void *data, int pos)
{
    int out_pos = _skip_name(name, 0, (int)size);
    if (out_pos <= 0)
        return false;
    out_pos--; // out_pos points to the ending null byte
    int8_t *out_buf = (void *)name;

    const uint8_t *in_buf = data;
    int in_pos = pos;
    for (;;) {
        uint8_t count = in_buf[in_pos];
        if (count == 0) {
            return true;
        } else if (count < 0xc0) {
            if (out_pos + count + 1 > size)
                return false;
            out_buf[out_pos] = count;
            memcpy(out_buf + out_pos + 1, in_buf + in_pos + 1, count);
            out_pos += count + 1;
            in_pos += count + 1;
            out_buf[out_pos] = 0;
        } else if (count >= 0xc0) {
            uint16_t ptr = (uint16_t)in_buf[in_pos] << 8 | (uint16_t)in_buf[in_pos + 1];
            ptr &= 0x3fff;
            // check that the pointer points backward
            if (ptr >= pos)
                return false;
            return u2_dns_name_append_compressed_name(name, size, data, ptr);
        }
    }
}

/**
 * Provided names must be valid dns names without compression.
 */
int u2_dns_name_compare(const char *name1, const char *name2)
{
    int len1 = u2_dns_name_length(name1);
    int len2 = u2_dns_name_length(name2);
    if (len1 > len2)
        return 1;
    if (len1 < len2)
        return -1;
    return memcmp(name1, name2, len1);
}

/**
 * Initialize the reader.
 * The given message is not copied into the reader. It should be kept in memory
 * as long as we use the reader and the extracted entries.
 * @return 0 = success, negative number = error
 */
int u2_dns_msg_reader_init(struct u2_dns_msg_reader *reader, const void *msg, size_t size)
{
    struct u2_dns_msg_reader tmp = {
        .data = msg,
        .size = (int)size,
        .pos = 12
    };
    *reader = tmp;

    if (size < 12 || size > INT_MAX) {
        reader->index = -1;
        return -1;
    }

    for (int j = 0; j < 4; j++) {
        int count = u2_dns_msg_get_field_u16(msg, 4 + 2 * j);
        if (j == 0)
            reader->q_count = count;
        else
            reader->rr_count += count;
    }

    return 0;
}

/**
 * Extract an entry from the message.
 * This function is very efficient as long as entries are retrieved in sequencial
 * order.
 * The returned entries are valid as long as the reader and the related input
 * message are kept in memory.
 * @return 0 = success, negative number = error
 */
int u2_dns_msg_reader_get_entry(struct u2_dns_msg_reader *reader, int index, struct u2_dns_msg_entry *entry)
{
    if (reader->index < 0)
        return -1;
    int rv = _move_to_entry(reader, index);
    if (rv < 0)
        return -1;
    return _get_next_entry(reader, entry);
}

void u2_dns_msg_builder_init(struct u2_dns_msg_builder *builder, void *data, size_t size, int id, int flags)
{
    if (size < 12)
        U2_FATAL("buffer size too small");

    builder->data = data;
    builder->max = (int)size;
    builder->size = 12;
    builder->counter_pos = 4;
    memset(data, 0, 12);
    u2_dns_msg_set_field_u16(builder->data, 0, id);
    u2_dns_msg_set_field_u16(builder->data, 2, flags);
}

void u2_dns_msg_builder_set_category(struct u2_dns_msg_builder *builder, enum u2_dns_rr_category category)
{
    int pos;
    switch (category) {
        case U2_DNS_RR_CATEGORY_QUESTION:
            pos = 4;
            break;
        case U2_DNS_RR_CATEGORY_ANSWER:
            pos = 6;
            break;
        case U2_DNS_RR_CATEGORY_AUTHORITY:
            pos = 8;
            break;
        case U2_DNS_RR_CATEGORY_ADDITIONAL:
            pos = 10;
            break;
        default:
            U2_FATAL("u2_dns: bad rr category %d\n", (int)category);
    }
    if (pos < builder->counter_pos)
        U2_FATAL("u2_dns: bad rr category order\n");
    builder->counter_pos = pos;
}

bool u2_dns_msg_builder_add_question(struct u2_dns_msg_builder *builder, const char *name, int type, bool cache_flush)
{
    if (builder->counter_pos != 4)
        u2_dns_msg_builder_set_category(builder, U2_DNS_RR_CATEGORY_QUESTION);

    int count = u2_dns_msg_get_field_u16(builder->data, 4);

    int name_len = u2_dns_name_length(name);
    if (builder->size + name_len + 4 > builder->max)
        return false;
    u2_dns_msg_set_field_u16(builder->data, 4, count + 1);
    int i = builder->size;
    memcpy(builder->data + i, name, name_len);
    i += name_len;
    u2_dns_msg_set_field_u16(builder->data, i, type);
    i += 2;
    u2_dns_msg_set_field_u16(builder->data, i, cache_flush ? 0x8001 : 0x0001);
    i += 2;
    builder->size = i;
    return true;
}

bool u2_dns_msg_builder_add_rr_name(struct u2_dns_msg_builder *builder, const char *name, int type, bool cache_flush, int ttl, const char *rname)
{
    if (builder->counter_pos < 6)
        u2_dns_msg_builder_set_category(builder, U2_DNS_RR_CATEGORY_ANSWER);

    int count = u2_dns_msg_get_field_u16(builder->data, builder->counter_pos);

    int name_len = u2_dns_name_length(name);
    int rname_len = u2_dns_name_length(rname);
    if (builder->size + name_len + 10 + rname_len > builder->max)
        return false;
    u2_dns_msg_set_field_u16(builder->data, builder->counter_pos, count + 1);
    int i = builder->size;
    memcpy(builder->data + i, name, name_len);
    i += name_len;
    u2_dns_msg_set_field_u16(builder->data, i, type);
    i += 2;
    u2_dns_msg_set_field_u16(builder->data, i, cache_flush ? 0x8001 : 0x0001);
    i += 2;
    u2_dns_msg_set_field_u32(builder->data, i, ttl);
    i += 4;
    u2_dns_msg_set_field_u16(builder->data, i, rname_len);
    i += 2;
    memcpy(builder->data + i, rname, rname_len);
    i += rname_len;
    builder->size = i;
    return true;
}

bool u2_dns_msg_builder_add_rr_srv(struct u2_dns_msg_builder *builder, const char *name, bool cache_flush, int ttl, int priority, int weight, int port, const char *host_name)
{
    if (builder->counter_pos < 6)
        u2_dns_msg_builder_set_category(builder, U2_DNS_RR_CATEGORY_ANSWER);

    int count = u2_dns_msg_get_field_u16(builder->data, builder->counter_pos);

    int name_len = u2_dns_name_length(name);
    int host_name_len = u2_dns_name_length(host_name);
    if (builder->size + name_len + 16 + host_name_len > builder->max)
        return false;
    u2_dns_msg_set_field_u16(builder->data, builder->counter_pos, count + 1);
    int i = builder->size;
    memcpy(builder->data + i, name, name_len);
    i += name_len;
    u2_dns_msg_set_field_u16(builder->data, i, U2_DNS_RR_TYPE_SRV);
    i += 2;
    u2_dns_msg_set_field_u16(builder->data, i, cache_flush ? 0x8001 : 0x0001);
    i += 2;
    u2_dns_msg_set_field_u32(builder->data, i, ttl);
    i += 4;
    u2_dns_msg_set_field_u16(builder->data, i, host_name_len + 6);
    i += 2;
    u2_dns_msg_set_field_u16(builder->data, i, priority);
    i += 2;
    u2_dns_msg_set_field_u16(builder->data, i, weight);
    i += 2;
    u2_dns_msg_set_field_u16(builder->data, i, port);
    i += 2;
    memcpy(builder->data + i, host_name, host_name_len);
    i += host_name_len;
    builder->size = i;
    return true;
}

bool u2_dns_msg_builder_add_rr_single_domain_nsec(struct u2_dns_msg_builder *builder, const char *name, bool cache_flush, int ttl, u2_dns_type_mask_t type_mask)
{
    int nbytes = 1;
    for (int i = 0; i < sizeof(type_mask); i++) {
        if ((type_mask >> (i * 8)) & 0xFF)
            nbytes = i + 1;
    }

    if (builder->counter_pos < 6)
        u2_dns_msg_builder_set_category(builder, U2_DNS_RR_CATEGORY_ANSWER);

    int count = u2_dns_msg_get_field_u16(builder->data, builder->counter_pos);

    int name_len = u2_dns_name_length(name);
    if (builder->size + name_len + 10 + name_len + 2 + nbytes > builder->max)
        return false;
    u2_dns_msg_set_field_u16(builder->data, builder->counter_pos, count + 1);
    int i = builder->size;
    memcpy(builder->data + i, name, name_len);
    i += name_len;
    u2_dns_msg_set_field_u16(builder->data, i, U2_DNS_RR_TYPE_NSEC);
    i += 2;
    u2_dns_msg_set_field_u16(builder->data, i, cache_flush ? 0x8001 : 0x0001);
    i += 2;
    u2_dns_msg_set_field_u32(builder->data, i, ttl);
    i += 4;
    u2_dns_msg_set_field_u16(builder->data, i, name_len + 2 + nbytes);
    i += 2;
    memcpy(builder->data + i, name, name_len);
    i += name_len;
    u2_dns_msg_set_field_u8(builder->data, i, 0);
    i += 1;
    u2_dns_msg_set_field_u8(builder->data, i, nbytes);
    i += 1;
    for (int j = 0; j < nbytes; j++) {
        u2_dns_msg_set_field_u8(builder->data, i, _reverse_bit_order((uint8_t)(type_mask >> (j * 8))));
        i += 1;
    }
    builder->size = i;
    return true;
}

bool u2_dns_msg_builder_add_rr_a(struct u2_dns_msg_builder *builder, const char *name, bool cache_flush, int ttl, const uint8_t *addr)
{
    if (builder->counter_pos < 6)
        u2_dns_msg_builder_set_category(builder, U2_DNS_RR_CATEGORY_ANSWER);

    int count = u2_dns_msg_get_field_u16(builder->data, builder->counter_pos);

    int name_len = u2_dns_name_length(name);
    if (builder->size + name_len + 10 + 4 > builder->max)
        return false;
    u2_dns_msg_set_field_u16(builder->data, builder->counter_pos, count + 1);
    int i = builder->size;
    memcpy(builder->data + i, name, name_len);
    i += name_len;
    u2_dns_msg_set_field_u16(builder->data, i, U2_DNS_RR_TYPE_A);
    i += 2;
    u2_dns_msg_set_field_u16(builder->data, i, cache_flush ? 0x8001 : 0x0001);
    i += 2;
    u2_dns_msg_set_field_u32(builder->data, i, ttl);
    i += 4;
    u2_dns_msg_set_field_u16(builder->data, i, 4);
    i += 2;
    memcpy(builder->data + i, addr, 4);
    i += 4;
    builder->size = i;
    return true;
}
