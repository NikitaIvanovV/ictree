#ifndef ERROR_H
#define ERROR_H

#define ERROR_BUF_SIZE 256

void set_error(char *);
void set_errorf(char *format, ...);
char *get_error();

#endif
