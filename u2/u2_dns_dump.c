//
//  u2_dns_dump.c
//
//  Created by Gabriele Mondada on March 29, 2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#include "u2_dns_dump.h"
#include <stdio.h>


static const char *_prefix(int indent)
{
    static const char buf[] = "                                ";
    static const int len = sizeof(buf) - 1;
    int n = indent * 2;
    if (n > len)
        n = len;
    return buf + len - n;
}

/**
 * Data should be validated by u2_dns_msg_decompose() or u2_dns_msg_name_span() before invoking this function.
 */
void u2_dns_msg_dump_name(const void *data, int pos)
{
    const uint8_t *in = data;
    int i = pos;
    for (;;) {
        uint8_t count = in[i];
        if (count == 0)
            return;
        if (i != pos)
            putchar('.');
        if (count < 0xc0) {
            i++;
            for (int j = 0; j < count; j++) {
                uint8_t c = in[i];
                i++;
                if (c == '\0') {
                    printf("\\0");
                } else if (c == '.') {
                    printf("\\.");
                } else if (c == '\t') {
                    printf("\\t");
                } else if (c == '\n') {
                    printf("\\n");
                } else if (c == '\r') {
                    printf("\\r");
                } else if (c == '\\') {
                    printf("\\\\");
                } else if (c < 32) {
                    printf("\\x%02X", (int)c);
                } else {
                    putchar(c);
                }
            }
        } else if (count >= 0xc0) {
            uint16_t ptr = (uint16_t)in[i] << 8 | (uint16_t)in[i + 1];
            ptr &= 0x3fff;
            if (ptr >= pos) {
                putchar('?');
                return;
            }
            u2_dns_msg_dump_name(data, ptr);
            return;
        } else {
            putchar('?');
        }
    }
}

void u2_dns_msg_dump_question(const struct u2_dns_msg_entry *entry, int indent)
{
    const char *prefix = _prefix(indent);
    printf("%sname:  ", prefix);
    u2_dns_msg_dump_name(entry->data, entry->name_pos);
    printf("\n");
    int type = u2_dns_msg_entry_get_question_type(entry);
    printf("%stype:  %d (%s)\n", prefix, type, u2_dns_msg_type_to_str(type));
    int klass = u2_dns_msg_entry_get_question_class(entry);
    printf("%sclass: %d\n", prefix, klass);
    bool unicast_response = u2_dns_msg_entry_get_question_unicast_response(entry);
    printf("%sunicast_response: %d\n", prefix, (int)unicast_response);
}

void u2_dns_msg_dump_rr(const struct u2_dns_msg_entry *entry, int indent)
{
    const char *prefix = _prefix(indent);
    printf("%sname: ", prefix);
    u2_dns_msg_dump_name(entry->data, entry->name_pos);
    printf("\n");
    int type = u2_dns_msg_entry_get_rr_type(entry);
    printf("%stype: %d (%s)\n", prefix, type, u2_dns_msg_type_to_str(type));
    int klass = u2_dns_msg_entry_get_rr_class(entry);
    bool cache_flush = u2_dns_msg_entry_get_rr_cache_flush(entry);
    int ttl = u2_dns_msg_entry_get_rr_ttl(entry);
    const uint8_t *rdata = entry->data + entry->rdata_pos;
    int rsize = entry->rdata_len;
    int rpos = entry->rdata_pos;
    switch (type) {
        case U2_DNS_RR_TYPE_PTR:
            printf("%sclass:       %d\n", prefix, klass);
            printf("%scache_flush: %d\n", prefix, (int)cache_flush);
            printf("%sttl:         %d\n", prefix, ttl);
            printf("%srdata:       ", prefix);
            int span = u2_dns_msg_name_span(entry->data, rpos + rsize, rpos);
            if (span != rsize)
                printf("<format error>");
            else
                u2_dns_msg_dump_name(entry->data, rpos);
            printf("\n");
            break;
        case U2_DNS_RR_TYPE_A:
            printf("%sclass:       %d\n", prefix, klass);
            printf("%scache_flush: %d\n", prefix, (int)cache_flush);
            printf("%sttl:         %d\n", prefix, ttl);
            if (rsize == 4)
                printf("%saddr:        %d.%d.%d.%d\n", prefix, (int)rdata[0], (int)rdata[1], (int)rdata[2], (int)rdata[3]);
            else
                printf("%saddr:        error\n", prefix);
            break;
        case U2_DNS_RR_TYPE_AAAA:
            printf("%sclass:       %d\n", prefix, klass);
            printf("%scache_flush: %d\n", prefix, (int)cache_flush);
            printf("%sttl:         %d\n", prefix, ttl);
            if (rsize == 16) {
                printf("%saddr:        ", prefix);
                for (int i = 0; i < 16; i++) {
                    if (i > 0 && (i % 2) == 0)
                        printf(":");
                    printf("%02x", (int)rdata[i]);
                }
                printf("\n");
            } else {
                printf("%saddr:        error\n", prefix);
            }
            break;
        case U2_DNS_RR_TYPE_NSEC: {
            printf("%sclass:       %d\n", prefix, klass);
            printf("%scache_flush: %d\n", prefix, (int)cache_flush);
            printf("%sttl:         %d\n", prefix, ttl);
            printf("%sname:        ", prefix);
            int span = u2_dns_msg_name_span(entry->data, rpos + rsize, rpos);
            if (span < 0)
                printf("<format error>");
            else
                u2_dns_msg_dump_name(entry->data, rpos);
            printf("\n");
            printf("%stypes:       ", prefix);
            int p = span;
            bool first = true;
            for (;;) {
                if (p >= rsize)
                    break;
                if (p + 2 > rsize) {
                    printf("<format error>");
                    break;
                }
                int type = (int)rdata[p] << 8;
                p++;
                int count = rdata[p];
                p++;
                if (p + count > rsize || count >= 32) {
                    printf("<format error>");
                    break;
                }
                for (int i = 0; i < count; i++) {
                    uint8_t mask = rdata[p];
                    for (int j = 0; j < 8; j++) {
                        if (mask & 0x80) {
                            if (first)
                                first = false;
                            else
                                printf(", ");
                            const char *str = u2_dns_msg_type_to_str(type);
                            printf("%d (%s)", type, str);
                        }
                        mask <<= 1;
                        type++;
                    }
                    p++;
                }
            }
            printf("\n");
            break;
        }
        case U2_DNS_RR_TYPE_SRV:
            printf("%sclass:       %d\n", prefix, klass);
            printf("%scache_flush: %d\n", prefix, (int)cache_flush);
            printf("%sttl:         %d\n", prefix, ttl);
            if (entry->rdata_len < 7) {
                printf("%ssrv:         <format error>\n", prefix);
            } else {
                int priority = u2_dns_msg_get_field_u16(rdata, 0);
                int weight = u2_dns_msg_get_field_u16(rdata, 2);
                int port = u2_dns_msg_get_field_u16(rdata, 4);
                printf("%spriority:    %d\n", prefix, priority);
                printf("%sweight:      %d\n", prefix, weight);
                printf("%sport:        %d\n", prefix, port);
                printf("%starget:      ", prefix);
                int span = u2_dns_msg_name_span(entry->data, rpos + rsize, rpos + 6);
                if (span + 6 != rsize)
                    printf("<format error>");
                else
                    u2_dns_msg_dump_name(entry->data, rpos + 6);
                printf("\n");
            }
            break;
        case U2_DNS_RR_TYPE_OPT:
            printf("%sudp payload: %d\n", prefix, klass | (cache_flush ? 0x8000 : 0));
            printf("%sflags:       0x%08x\n", prefix, ttl);
            printf("%srdata:      ", prefix);
            for (int i = 0; i < rsize;) {
                if (i + 4 > rsize) {
                    printf(" <format error>");
                    break;
                }
                int code = u2_dns_msg_get_field_u16(entry->data, rpos + i);
                int size = u2_dns_msg_get_field_u16(entry->data, rpos + i + 2);
                if (i + 4 + size > rsize) {
                    printf(" <format error>");
                    break;
                }
                printf(" 0x%04x=(%d)", code, size);
                i += 4 + size;
            }
            printf("\n");
            break;
        default:
            printf("%sclass:       %d\n", prefix, klass);
            printf("%scache_flush: %d\n", prefix, (int)cache_flush);
            printf("%sttl:         %d\n", prefix, ttl);
            printf("%srdata:       ", prefix);
            for (int i = 0; i < rsize; i++) {
                uint8_t c = rdata[i];
                if (c == '\0') {
                    printf("\\0");
                } else if (c == '\t') {
                    printf("\\t");
                } else if (c == '\n') {
                    printf("\\n");
                } else if (c == '\r') {
                    printf("\\r");
                } else if (c == '\\') {
                    printf("\\\\");
                } else if (c < 32 || c >= 128) {
                    printf("\\x%02X", (int)c);
                } else {
                    putchar(c);
                }
            }
            printf("\n");
            break;
    }
}

void u2_dns_msg_dump(const void *data, size_t size, int indent)
{
    const char *prefix = _prefix(indent);
    struct u2_dns_msg_reader reader;
    int rv = u2_dns_msg_reader_init(&reader, data, size);
    if (rv < 0) {
        printf("%serror: %d\n", prefix, rv);
        return;
    }
    int id = u2_dns_msg_reader_get_id(&reader);
    printf("%sid: %d\n", prefix, id);
    int flags = u2_dns_msg_reader_get_flags(&reader);
    printf("%sflags: 0x%04x", prefix, flags);
    if (flags & U2_DNS_MSG_FLAG_QR)
        printf(" response");
    else
        printf(" query");
    if (flags & U2_DNS_MSG_FLAG_AA)
        printf(" AA");
    printf("\n");

    struct u2_dns_msg_entry entry;
    int index = 0;
    int count = u2_dns_msg_reader_get_question_count(&reader);
    for (int i = 0; i < count; i++) {
        printf("%squestion %d:\n", prefix, i);
        int rv = u2_dns_msg_reader_get_entry(&reader, index, &entry);
        if (rv < 0) {
            printf("%serror: %d\n", _prefix(indent + 1), rv);
            return;
        }
        u2_dns_msg_dump_question(&entry, indent + 1);
        index++;
    }
    count = u2_dns_msg_reader_get_answer_rr_count(&reader);
    for (int i = 0; i < count; i++) {
        printf("%sanswer_rr %d:\n", prefix, i);
        int rv = u2_dns_msg_reader_get_entry(&reader, index, &entry);
        if (rv < 0) {
            printf("%serror: %d\n", _prefix(indent + 1), rv);
            return;
        }
        u2_dns_msg_dump_rr(&entry, indent + 1);
        index++;
    }
    count = u2_dns_msg_reader_get_authority_rr_count(&reader);
    for (int i = 0; i < count; i++) {
        printf("%sauthority_rr %d:\n", prefix, i);
        int rv = u2_dns_msg_reader_get_entry(&reader, index, &entry);
        if (rv < 0) {
            printf("%serror: %d\n", _prefix(indent + 1), rv);
            return;
        }
        u2_dns_msg_dump_rr(&entry, indent + 1);
        index++;
    }
    count = u2_dns_msg_reader_get_additional_rr_count(&reader);
    for (int i = 0; i < count; i++) {
        printf("%sadditional_rr %d:\n", prefix, i);
        int rv = u2_dns_msg_reader_get_entry(&reader, index, &entry);
        if (rv < 0) {
            printf("%serror: %d\n", _prefix(indent + 1), rv);
            return;
        }
        u2_dns_msg_dump_rr(&entry, indent + 1);
        index++;
    }
}

void u2_dns_database_dump(const struct u2_dns_database *database, int indent)
{
    for (int i = 0; i < database->domain_count; i++) {
        printf("%sdomain %d:\n", _prefix(indent), i);
        const struct u2_dns_domain *domain = database->domain_list[i];
        printf("%sname: ", _prefix(indent + 1));
        u2_dns_msg_dump_name(domain->name, 0);
        printf("\n");
        for (int j = 0; j < domain->record_count; j++) {
            printf("%srecord %d:\n", _prefix(indent + 1), j);
            const struct u2_dns_record *record = domain->record_list[j];
            const char *type_name = u2_dns_msg_type_to_str(record->type);
            const char *prefix = _prefix(indent + 2);
            printf("%stype:        %s\n", prefix, type_name);
            printf("%scache_flush: %d\n", prefix, (int)record->cache_flush);
            printf("%sttl:         %d\n", prefix, record->ttl);
            switch (record->type) {
                case U2_DNS_RR_TYPE_PTR:
                    printf("%srdata:       ", prefix);
                    u2_dns_msg_dump_name(record->ptr.name, 0);
                    printf("\n");
                    break;
                case U2_DNS_RR_TYPE_A:
                    printf("%saddr:        %d.%d.%d.%d\n", prefix, (int)record->a.addr[0], (int)record->a.addr[1], (int)record->a.addr[2], (int)record->a.addr[3]);
                    break;
                case U2_DNS_RR_TYPE_AAAA:
                    printf("%saddr:        ", prefix);
                    for (int i = 0; i < 16; i++) {
                        if (i > 0 && (i % 2) == 0)
                            printf(":");
                        printf("%02x", (int)record->aaaa.addr[i]);
                    }
                    printf("\n");
                    break;
                case U2_DNS_RR_TYPE_NSEC: {
                    break;
                }
                case U2_DNS_RR_TYPE_SRV:
                    printf("%sport:        %d\n", prefix, record->srv.port);
                    printf("%starget:      ", prefix);
                    u2_dns_msg_dump_name(record->srv.name, 0);
                    printf("\n");
                    break;
                default:
                    break;
            }
        }
    }
}
