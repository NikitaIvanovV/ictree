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

#ifndef LINES_H
#define LINES_H

#include <stdio.h>
#include <stdlib.h>

#define LINE_DELIM '\n'

typedef struct Lines {
    char **lines;
    size_t lines_l;
} Lines;

Lines get_lines(FILE *stream, char separator);
void free_lines(Lines *lines);
void sort_lines(Lines lines);

#endif
