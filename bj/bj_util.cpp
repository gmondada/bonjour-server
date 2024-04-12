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

} // namespace
