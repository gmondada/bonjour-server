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
            for (int j=0; j<count; j++) {
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

void u2_dns_msg_dump_question(const void *data, const struct u2_dns_msg_entry *entry, int indent)
{
    const char *prefix = _prefix(indent);
    printf("%sname:  ", prefix);
    u2_dns_msg_dump_name(data, entry->name_pos);
    printf("\n");
    int type = u2_dns_msg_get_question_type(data, entry);
    printf("%stype:  %d (%s)\n", prefix, type, u2_dns_msg_type_to_str(type));
    int klass = u2_dns_msg_get_question_class(data, entry);
    printf("%sclass: %d\n", prefix, klass);
    bool unicast_response = u2_dns_msg_get_question_unicast_response(data, entry);
    printf("%sunicast_response: %d\n", prefix, (int)unicast_response);
}

void u2_dns_msg_dump_rr(const void *data, const struct u2_dns_msg_entry *entry, int indent)
{
    const char *prefix = _prefix(indent);
    printf("%sname: ", prefix);
    u2_dns_msg_dump_name(data, entry->name_pos);
    printf("\n");
    int type = u2_dns_msg_get_rr_type(data, entry);
    printf("%stype: %d (%s)\n", prefix, type, u2_dns_msg_type_to_str(type));
    int klass = u2_dns_msg_get_rr_class(data, entry);
    bool cache_flush = u2_dns_msg_get_rr_cache_flush(data, entry);
    int ttl = u2_dns_msg_get_rr_ttl(data, entry);
    uint8_t *rdata = ((uint8_t *)data) + entry->rdata_pos;
    int rsize = entry->rdata_len;
    int rpos = entry->rdata_pos;
    switch (type) {
        case U2_DNS_RR_TYPE_PTR:
            printf("%sclass:       %d\n", prefix, klass);
            printf("%scache_flush: %d\n", prefix, (int)cache_flush);
            printf("%sttl:         %d\n", prefix, ttl);
            printf("%srdata:       ", prefix);
            int span = u2_dns_msg_name_span(data, rpos + rsize, rpos);
            if (span != rsize)
                printf("<format error>");
            else
                u2_dns_msg_dump_name(data, rpos);
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
            int span = u2_dns_msg_name_span(data, rpos + rsize, rpos);
            if (span < 0)
                printf("<format error>");
            else
                u2_dns_msg_dump_name(data, rpos);
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
                for (int i=0; i<count; i++) {
                    uint8_t mask = rdata[p];
                    for (int j=0; j<8; j++) {
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
                int span = u2_dns_msg_name_span(data, rpos + rsize, rpos + 6);
                if (span + 6 != rsize)
                    printf("<format error>");
                else
                    u2_dns_msg_dump_name(data, rpos + 6);
                printf("\n");
            }
            break;
        case U2_DNS_RR_TYPE_OPT:
            printf("%sudp payload: %d\n", prefix, klass | (cache_flush ? 0x8000 : 0));
            printf("%sflags:       0x%08x\n", prefix, ttl);
            printf("%srdata:      ", prefix);
            for (int i=0; i<rsize;) {
                if (i + 4 > rsize) {
                    printf(" <format error>");
                    break;
                }
                int code = u2_dns_msg_get_field_u16(data, rpos + i);
                int size = u2_dns_msg_get_field_u16(data, rpos + i + 2);
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
            for (int i=0; i<rsize; i++) {
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
    struct u2_dns_msg_entry entries[32];
    int rv = u2_dns_msg_decompose(data, size, entries, 32);
    if (rv >= 0 && rv <= 32) {
        uint16_t id = u2_dns_msg_get_id(data);
        printf("%sid: %d\n", prefix, id);
        uint16_t flags = u2_dns_msg_get_flags(data);
        printf("%sflags: 0x%04x", prefix, flags);
        if (flags & U2_DNS_MSG_FLAG_QR)
            printf(" response");
        else
            printf(" query");
        if (flags & U2_DNS_MSG_FLAG_AA)
            printf(" AA");
        printf("\n");

        int entry_index = 0;
        int count = u2_dns_msg_get_question_count(data);
        for (int i=0; i<count; i++) {
            printf("%squestion %d:\n", prefix, i);
            u2_dns_msg_dump_question(data, entries + entry_index, indent + 1);
            entry_index++;
        }
        count = u2_dns_msg_get_answer_rr_count(data);
        for (int i=0; i<count; i++) {
            printf("%sanswer_rr %d:\n", prefix, i);
            u2_dns_msg_dump_rr(data, entries + entry_index, indent + 1);
            entry_index++;
        }
        count = u2_dns_msg_get_authority_rr_count(data);
        for (int i=0; i<count; i++) {
            printf("%sauthority_rr %d:\n", prefix, i);
            u2_dns_msg_dump_rr(data, entries + entry_index, indent + 1);
            entry_index++;
        }
        count = u2_dns_msg_get_additional_rr_count(data);
        for (int i=0; i<count; i++) {
            printf("%sadditional_rr %d:\n", prefix, i);
            u2_dns_msg_dump_rr(data, entries + entry_index, indent + 1);
            entry_index++;
        }
    } else {
        printf("%sdecoding error\n", prefix);
    }
}
