
#pragma once

#include <stddef.h>

typedef struct {
    char *buffer;
    size_t len;
    size_t capacity;
} SB;

void sb_init(SB *sb, char *buffer, size_t buffer_size_bytes);
void sb_reset(SB *sb);
void sb_printf(SB *sb, const char *fmt, ...);
