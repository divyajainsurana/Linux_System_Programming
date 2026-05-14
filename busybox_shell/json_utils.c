#include <stdio.h>

#include "json_utils.h"

void json_print_string(FILE *out, const char *value)
{
    const unsigned char *cursor = (const unsigned char *)value;

    fputc('"', out);
    while (*cursor != '\0') {
        json_print_escaped_char(out, *cursor);
        cursor++;
    }
    fputc('"', out);
}

void json_print_escaped_char(FILE *out, int ch)
{
    switch (ch) {
    case '"':
        fputs("\\\"", out);
        break;
    case '\\':
        fputs("\\\\", out);
        break;
    case '\b':
        fputs("\\b", out);
        break;
    case '\f':
        fputs("\\f", out);
        break;
    case '\n':
        fputs("\\n", out);
        break;
    case '\r':
        fputs("\\r", out);
        break;
    case '\t':
        fputs("\\t", out);
        break;
    default:
        if ((unsigned char)ch < 0x20) {
            fprintf(out, "\\u%04x", (unsigned char)ch);
        } else {
            fputc(ch, out);
        }
        break;
    }
}
