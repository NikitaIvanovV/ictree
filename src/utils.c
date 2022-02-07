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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

#ifdef DEV
FILE *debug_file = NULL;
#endif

size_t find_first_nonblank(char *string)
{
    char c;
    for (size_t i = 0; i < strlen(string); i++) {
        c = string[i];
        if (c != ' ' && c != '\t')
            return i;
    }

    return 0;
}

int size_t_compare(const void *a, const void *b)
{
    size_t a_i = *(size_t *)a, b_i = *(size_t *)b;

    return (a_i < b_i) ? -1 : (a_i > b_i);
}

char *strdup(char *str)
{

    unsigned long len = strlen(str);

    char *dup = malloc((len + 1) * sizeof(char));
    assert(dup != NULL);

    strcpy(dup, str);

    return dup;
}
