#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "lines.h"
#include "utils.h"
#include "vector.h"

#define MIN_LINE_LENGTH       256
#define MIN_LINES_LIST_LENGTH 256

static char *get_line(FILE *stream)
{
    int c;

    cvector_vector_type(char) line = NULL;
    cvector_grow(line, MIN_LINE_LENGTH);

    do {
        c = fgetc(stream);

        if (feof(stream) || c == LINE_DELIM) {
            c = 0;
        }

        cvector_push_back(line, c);
    } while (c);

    return line;
}

static void free_line(char *line)
{
    if (line == NULL)
        return;
    cvector_free(line);
}

static int line_compare(const void *a, const void *b)
{
    char *line1 = *(char **)a, *line2 = *(char **)b;

    line1 += find_first_nonblank(line1);
    line2 += find_first_nonblank(line2);

    return strcmp(line1, line2);
}

void get_lines(char ***lines_, size_t *lines_l_, FILE *stream)
{
    char *line;

    cvector_vector_type(char *) lines = NULL;
    cvector_grow(lines, MIN_LINES_LIST_LENGTH);

    while (1) {
        line = get_line(stream);

        if (feof(stream)) {
            free_line(line);
            break;
        }

        cvector_push_back(lines, line);
    }

    *lines_ = lines;
    *lines_l_ = cvector_size(lines);
}

void free_lines(char **list, size_t list_length)
{
    for (size_t i = 0; i < list_length; i++) {
        free_line(list[i]);
    }
    cvector_free(list);
}

void sort_lines(char **lines, size_t n)
{
    qsort(lines, n, sizeof(lines[0]), line_compare);
}
