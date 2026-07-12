
#include "string_builder.h"
#include <stdio.h>
#include <stdarg.h>

void sb_init(SB *sb, char *buffer, size_t buffer_size_bytes)  {
    sb->buffer = buffer;
    sb->len = 0;
    sb->capacity = buffer_size_bytes;
}

void sb_reset(SB *sb) {
    sb->len = 0;
}

void sb_printf(SB *sb, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    vsnprintf(&sb->buffer[sb->len], sb->capacity, fmt, args);
    while (sb->len < sb->capacity - 1 && sb->buffer[sb->len] != 0) sb->len ++;

    va_end(args);
}
