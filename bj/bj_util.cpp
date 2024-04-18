//
//  bj_util.cpp
//
//  Created by Gabriele Mondada on 29.03.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#include "bj_util.h"
#include <stdio.h>

namespace bj_util
{

void dump_data(std::span<unsigned char> data)
{
    size_t size = data.size();
    unsigned char *buf = data.data();
    for (int y = 0; y < (size + 15) / 16; y++) {
        for (int x = 0; x < 16; x++) {
            int off = x + y * 16;
            if (off < size)
                printf("%02x ", buf[off] & 0xff);
            else
                printf("   ");
            if (x == 7)
                printf(" ");
        }
        for (int x = 0; x < 16; x++) {
            int off = x + y * 16;
            if (off < size) {
                unsigned char c = buf[off];
                if (c < 32 || c >= 128)
                    c = '.';
                printf("%c", c);
            } else
                printf(" ");
        }
        printf("\n");
    }
}

/**
 * Convert a domain name from a classic dot-separated string into
 * the format used by DNS.
 */
std::string dns_name(std::string_view name)
{
    std::string dns_name;
    size_t length = name.size();
    if (length > 0 && name[length - 1] == '.')
        length--;
    size_t p1 = 0;
    for (size_t p2 = 0; p2 <= length; p2++) {
        if (p2 == length || name[p2] == '.') {
            size_t component_length = p2 - p1;
            if (component_length > 63)
                throw std::invalid_argument("DNS name component too long");
            dns_name += (char)component_length;
            dns_name += name.substr(p1, p2 - p1);
            p1 = p2 + 1;
        }
    }
    dns_name += (char)0;
    if (dns_name.size() > 255)
        throw std::invalid_argument("DNS name too long");
    return dns_name;
}

} // namespace
