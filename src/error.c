#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "error.h"

static char error_buf[ERROR_BUF_SIZE];

void set_error(char *buf)
{
    strncpy(error_buf, buf, ERROR_BUF_SIZE);
}

void set_errorf(char *format, ...)
{
    va_list args;
    char msg[ERROR_BUF_SIZE];
    va_start(args, format);
    vsnprintf(msg, ERROR_BUF_SIZE, format, args);
    va_end(args);
    set_error(msg);
}

char *get_error()
{
    return error_buf;
}
