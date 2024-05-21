//
//  u2_base.h
//
//  Created by Gabriele Mondada on 02.04.2024.
//  Copyright © 2024 Gabriele Mondada. All rights reserved.
//

#ifndef _U2_BASE_H_
#define _U2_BASE_H_

#include <stdlib.h>
#include "u2_def.h"

#define U2_MIN(x, y) (((x) > (y)) ? (y) : (x))
#define U2_MAX(x, y) (((x) < (y)) ? (y) : (x))
#define U2_ARRAY_LEN(x) (sizeof(x) / sizeof(*(x)))

#define U2_FATAL(...) do { \
    fprintf(stderr, "%s(%d): ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\n"); \
    abort(); \
} while (0)

#endif
