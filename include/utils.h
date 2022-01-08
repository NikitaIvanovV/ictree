#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

#include "error.h"

#define MAX(a, b) ((a > b) ? a : b)
#define MIN(a, b) ((a < b) ? a : b)

size_t find_first_nonblank(char *string);

#endif
