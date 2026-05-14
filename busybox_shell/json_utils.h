#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <stdio.h>

void json_print_string(FILE *out, const char *value);
void json_print_escaped_char(FILE *out, int ch);

#endif
