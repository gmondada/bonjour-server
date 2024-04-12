//
//  u2_dns.c
//
//  Created by Gabriele Mondada on March 29, 2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#include "u2_dns.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>


static int _skip_name(const void *data, int pos, int max)
{
    const uint8_t *in = data;
    int i = pos;
    for (;;) {
        if (i >= max)
            return -1;
        uint8_t count = in[i];
        if (count == 0) {
            i += 1;
            if (i > max)
                return -1;
            return i;
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

/**
 * Check the structure of the given dns message and extract the list of entries. Return the number of entries
 * present in the message, whatever `max` is. This allows the caller to verify if the provider array is big enough.
 * In case of error, return a negative error code.
 * This function must be called first, before any other u2_dns_msg_xxx() function.
 */
int u2_dns_msg_decompose(const void *msg, size_t size, struct u2_dns_msg_entry *entries, int max)
{
    int count;
    int pos = 12;
    int entry_index = 0;

    if (size < 12)
        return -1;

    for (int j=0; j<4; j++) {
        count = u2_dns_msg_get_field_u16(msg, 4 + 2 * j);
        for (int i=0; i<count; i++) {
            int pos1 = pos;
            int rv = _skip_name(msg, pos, (int)size);
            if (rv < 0)
                return -1;
            pos = rv;
            int pos2 = pos;
            if (j == 0) {
                // question
                if (pos + 4 > size)
                    return -1;
                pos += 4;
                if (entry_index < max) {
                    entries[entry_index].name_pos = pos1;
                    entries[entry_index].type_pos = pos2;
                    entries[entry_index].rdata_len = 0;
                    entries[entry_index].rdata_pos = pos;
                }
            } else {
                // resource record
                if (pos + 10 > size)
                    return -1;
                pos += 8;
                int rsize = u2_dns_msg_get_field_u16(msg, pos);
                pos += 2;
                int pos3 = pos;
                if (pos + rsize > size)
                    return -1;
                pos += rsize;
                if (entry_index < max) {
                    entries[entry_index].name_pos = pos1;
                    entries[entry_index].type_pos = pos2;
                    entries[entry_index].rdata_len = rsize;
                    entries[entry_index].rdata_pos = pos3;
                }
            }
            entry_index++;
        }
    }

    return entry_index;
}

bool u2_dns_name_init(char *name, size_t size)
{
    if (size < 1)
        return false;
    *name = 0;
    return true;
}

/**
 * Return the entire name length, including the ending zero label. Name compression is not allowed.
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
            abort();
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
    if (pos  + len + 1 > size)
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
        } else {
            return false;
        }
    }
}

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

void u2_dns_msg_builder_init(struct u2_dns_msg_builder *builder, void *data, size_t size, int id, int flags)
{
    if (size < 12)
        abort();

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
            abort();
    }
    if (pos < builder->counter_pos) {
        abort();
    }
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
    for (int i=0; i<sizeof(type_mask); i++) {
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
    for (int j=0; j<nbytes; j++) {
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
