#ifndef LINES_H
#define LINES_H

#include <stdio.h>
#include <stdlib.h>

#define LINE_DELIM '\n'

void free_lines(char **list, size_t list_length);
void get_lines(char ***lines, size_t *lines_l, FILE *stream);
void sort_lines(char **lines, size_t n);

#endif
