#include "utils.h"

#include <string.h>

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
