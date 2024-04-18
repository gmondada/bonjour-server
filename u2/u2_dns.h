//
//  u2_dns.h
//
//  Created by Gabriele Mondada on March 29, 2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#ifndef _U2_DNS_H_
#define _U2_DNS_H_

#include <stdint.h>
#include <stdbool.h>
#include "u2_def.h"

U2_C_BEGIN


/*** types ***/

enum u2_dns_msg_flas {
    U2_DNS_MSG_FLAG_QR = 0x8000,
    U2_DNS_MSG_FLAG_AA = 0x0400,
};

enum u2_dns_rr_type {
    U2_DNS_RR_TYPE_A     = 1,
    U2_DNS_RR_TYPE_NS    = 2,
    U2_DNS_RR_TYPE_CNAME = 5,
    U2_DNS_RR_TYPE_PTR   = 12,
    U2_DNS_RR_TYPE_TXT   = 16,
    U2_DNS_RR_TYPE_AAAA  = 28,
    U2_DNS_RR_TYPE_SRV   = 33,
    U2_DNS_RR_TYPE_OPT   = 41,
    U2_DNS_RR_TYPE_NSEC  = 47,
    U2_DNS_RR_TYPE_ANY   = 255,
};

enum u2_dns_rr_category {
    U2_DNS_RR_CATEGORY_NONE = 0,
    U2_DNS_RR_CATEGORY_QUESTION,
    U2_DNS_RR_CATEGORY_ANSWER,
    U2_DNS_RR_CATEGORY_AUTHORITY,
    U2_DNS_RR_CATEGORY_ADDITIONAL,
};

typedef uint64_t u2_dns_type_mask_t;

struct u2_dns_domain;

struct u2_dns_record {
    const struct u2_dns_domain *domain;
    enum u2_dns_rr_type type;
    int ttl;
    bool cache_flush;
    union {
        struct {
            int port;
            const char *name;
        } srv;
        struct {
            const char *name;
        } ptr;
        struct {
            const char *name;
        } txt;
        struct {
            uint8_t addr[4];
        } a;
        struct {
            uint8_t addr[16];
        } aaaa;
        struct {
            // content is implicit
            int __placeholder;
        } nsec;
    };
};

struct u2_dns_domain {
    const char *name;
    const struct u2_dns_record *const *record_list;
    int record_count;
};

struct u2_dns_database {
    const struct u2_dns_domain *const *domain_list;
    int domain_count;
};

struct u2_dns_msg_entry {
    uint16_t name_pos;
    uint16_t type_pos;
    uint16_t rdata_pos;
    uint16_t rdata_len;
};

struct u2_dns_msg_builder {
    uint8_t *data;
    int size;
    int max;
    int counter_pos;
};


/*** prototypes ***/

const char *u2_dns_msg_type_to_str(int type);
int u2_dns_msg_name_span(const void *msg, size_t size, int pos);
int u2_dns_msg_decompose(const void *msg, size_t size, struct u2_dns_msg_entry *entries, int max);

bool u2_dns_name_init(char *name, size_t size);
int u2_dns_name_length(const void *name);
bool u2_dns_name_append_label(char *name, size_t size, char *label);
bool u2_dns_name_append_compressed_name(char *name, size_t size, const void *data, int pos);
int u2_dns_name_compare(const char *name1, const char *name2);

void u2_dns_msg_builder_init(struct u2_dns_msg_builder *builder, void *data, size_t size, int id, int flags);
void u2_dns_msg_builder_set_category(struct u2_dns_msg_builder *builder, enum u2_dns_rr_category category);
bool u2_dns_msg_builder_add_question(struct u2_dns_msg_builder *builder, const char *name, int type, bool cache_flush);
bool u2_dns_msg_builder_add_rr_name(struct u2_dns_msg_builder *builder, const char *name, int type, bool cache_flush, int ttl, const char *rname);
bool u2_dns_msg_builder_add_rr_srv(struct u2_dns_msg_builder *builder, const char *name, bool cache_flush, int ttl, int priority, int weight, int port, const char *host_name);
bool u2_dns_msg_builder_add_rr_single_domain_nsec(struct u2_dns_msg_builder *builder, const char *name, bool cache_flush, int ttl, u2_dns_type_mask_t type_mask);
bool u2_dns_msg_builder_add_rr_a(struct u2_dns_msg_builder *builder, const char *name, bool cache_flush, int ttl, const uint8_t *addr);


/*** inline functions ***/

static inline uint8_t u2_dns_msg_get_field_u8(const void *data, int pos)
{
    const uint8_t *p = (const uint8_t *)data;
    return p[pos];
}

static inline void u2_dns_msg_set_field_u8(void *data, int pos, uint8_t value)
{
    uint8_t *p = (uint8_t *)data;
    p[pos] = value;
}

static inline uint16_t u2_dns_msg_get_field_u16(const void *data, int pos)
{
    const uint8_t *p = (const uint8_t *)data;
    p += pos;
    return (uint16_t)p[0] << 8 | (uint16_t)p[1];
}

static inline void u2_dns_msg_set_field_u16(void *data, int pos, uint16_t value)
{
    uint8_t *p = (uint8_t *)data;
    p += pos;
    p[0] = value >> 8;
    p[1] = value;
}

static inline uint32_t u2_dns_msg_get_field_u32(const void *data, int pos)
{
    const uint8_t *p = (const uint8_t *)data;
    p += pos;
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | (uint32_t)p[3];
}

static inline void u2_dns_msg_set_field_u32(void *data, int pos, uint32_t value)
{
    uint8_t *p = (uint8_t *)data;
    p += pos;
    p[0] = value >> 24;
    p[1] = value >> 16;
    p[2] = value >> 8;
    p[3] = value;
}

static inline int u2_dns_msg_get_id(const void *data)
{
    return u2_dns_msg_get_field_u16(data, 0);
}

static inline int u2_dns_msg_get_flags(const void *data)
{
    return u2_dns_msg_get_field_u16(data, 2);
}

static inline int u2_dns_msg_get_question_count(const void *data)
{
    return u2_dns_msg_get_field_u16(data, 4);
}

static inline int u2_dns_msg_get_answer_rr_count(const void *data)
{
    return u2_dns_msg_get_field_u16(data, 6);
}

static inline int u2_dns_msg_get_authority_rr_count(const void *data)
{
    return u2_dns_msg_get_field_u16(data, 8);
}

static inline int u2_dns_msg_get_additional_rr_count(const void *data)
{
    return u2_dns_msg_get_field_u16(data, 10);
}

static inline int u2_dns_msg_get_question_type(const void *data, const struct u2_dns_msg_entry *entry)
{
    return u2_dns_msg_get_field_u16(data, entry->type_pos);
}

static inline bool u2_dns_msg_get_question_unicast_response(const void *data, const struct u2_dns_msg_entry *entry)
{
    return (u2_dns_msg_get_field_u8(data, entry->type_pos + 2) & 0x80) != 0;
}

static inline int u2_dns_msg_get_question_class(const void *data, const struct u2_dns_msg_entry *entry)
{
    return u2_dns_msg_get_field_u16(data, entry->type_pos + 2) & 0x7fff;
}

static inline int u2_dns_msg_get_rr_type(const void *data, const struct u2_dns_msg_entry *entry)
{
    return u2_dns_msg_get_field_u16(data, entry->type_pos);
}

static inline bool u2_dns_msg_get_rr_cache_flush(const void *data, const struct u2_dns_msg_entry *entry)
{
    return (u2_dns_msg_get_field_u8(data, entry->type_pos + 2) & 0x80) != 0;
}

static inline int u2_dns_msg_get_rr_class(const void *data, const struct u2_dns_msg_entry *entry)
{
    return u2_dns_msg_get_field_u16(data, entry->type_pos + 2) & 0x7fff;
}

static inline int u2_dns_msg_get_rr_ttl(const void *data, const struct u2_dns_msg_entry *entry)
{
    return u2_dns_msg_get_field_u32(data, entry->type_pos + 4);
}


U2_C_END
#endif
