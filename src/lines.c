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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "lines.h"
#include "utils.h"
#include "vector.h"

#define MIN_LINE_LENGTH      256
#define MIN_LINES_VECTOR_LEN 256

static char *get_line(cvector_vector_type(char) string, FILE *stream)
{
    int c;
    unsigned long p = cvector_size(string);

    do {
        c = fgetc(stream);

        if (feof(stream) || c == LINE_DELIM) {
            c = 0;
        }

        cvector_push_back(string, c);
    } while (c);

    return string + p;
}

Lines get_lines(FILE *stream)
{
    char *p;

    Lines l = { .string = NULL, .lines = NULL, .lines_l = 0 };

    /* string vector contains all lines */
    cvector_grow(l.string, MIN_LINE_LENGTH * MIN_LINES_VECTOR_LEN);

    /* lines vector contains pointers to every line */
    cvector_grow(l.lines, MIN_LINES_VECTOR_LEN);

    while (1) {
        p = get_line(l.string, stream);

        if (feof(stream))
            break;

        cvector_push_back(l.lines, p);
    }

    l.lines_l = cvector_size(l.lines);
    return l;
}

void free_lines(Lines *lines)
{
    if (lines->string != NULL) {
        cvector_free(lines->string);
        lines->string = NULL;
    }
    if (lines->lines != NULL) {
        cvector_free(lines->lines);
        lines->lines = NULL;
    }
}

static int line_compare(const void *a, const void *b)
{
    char *line1 = *(char **)a, *line2 = *(char **)b;

    line1 += find_first_nonblank(line1);
    line2 += find_first_nonblank(line2);

    return strcmp(line1, line2);
}

void sort_lines(Lines lines)
{
    qsort(lines.lines, lines.lines_l, sizeof(lines.lines[0]), line_compare);
}
