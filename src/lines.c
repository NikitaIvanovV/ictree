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

#define MIN_LINES_VECTOR_LEN 256

static char *get_line(FILE *stream)
{
    int c;

    cvector_vector_type(char) s = NULL;

    do {
        c = fgetc(stream);

        if (feof(stream) || c == LINE_DELIM) {
            c = 0;
        }

        cvector_push_back(s, c);
    } while (c);

    return s;
}

Lines get_lines(FILE *stream, char separator)
{
    char *s;

    Lines l = { .lines = NULL, .lines_l = 0 };

    /* lines vector contains pointers to every line */
    cvector_grow(l.lines, MIN_LINES_VECTOR_LEN);

    while (1) {
        s = get_line(stream);

        /* Append / char if not already there */
        if (s[cvector_size(s) - 2] != separator) {
            cvector_pop_back(s); /* Remove '\0' */
            cvector_push_back(s, separator);
            cvector_push_back(s, 0);
        }

        if (feof(stream)) {
            cvector_free(s);
            break;
        }

        cvector_push_back(l.lines, s);
    }

    l.lines_l = cvector_size(l.lines);
    return l;
}

void free_lines(Lines *lines)
{
    if (lines->lines != NULL) {
        for (size_t i = 0; i < cvector_size(lines->lines); i++) {
            cvector_free(lines->lines[i]);
        }
        cvector_free(lines->lines);
        lines->lines = NULL;
        lines->lines_l = 0;
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
