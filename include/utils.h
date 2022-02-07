/*
 * Copyright 2022 Nikita Ivanov
 *
 * This file is part of ictree
 *
 * ictree is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * ictree is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ictree. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

#include "error.h"

#define DIR_DELIM   '/'
#define DIR_DELIM_S ((char[2]){ DIR_DELIM, 0 })

#define LENGTH(a) (sizeof(a) / sizeof(a[0]))

#define MAX(a, b) ((a > b) ? a : b)
#define MIN(a, b) ((a < b) ? a : b)

#define FORMATTED_STRING(msg, format)              \
    do {                                           \
        va_list args;                              \
        va_start(args, format);                    \
        vsnprintf(msg, sizeof(msg), format, args); \
        va_end(args);                              \
    } while (0);

#ifdef DEV
extern FILE *debug_file;
#define DEBUG_FILE "debug"
#define DEBUG(...)                        \
    do {                                  \
        fprintf(debug_file, __VA_ARGS__); \
        fflush(debug_file);               \
    } while (0)
#endif

char *strdup(char *str);
int size_t_compare(const void *a, const void *b);
size_t find_first_nonblank(char *string);

#endif
