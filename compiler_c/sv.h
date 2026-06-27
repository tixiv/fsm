
#pragma once

#include <stddef.h>
#include <stdbool.h>

#define SV_FMT "%.*s"
#define SV_prnt(sv) (int)(sv).len, (sv).begin

typedef struct {
    const char *begin;
    size_t len;
} SV;

bool sv_starts_with(SV *sv, const char *start);
bool sv_compare_cstr(const SV *sv, const char *cstr);
bool sv_equal(const SV *sv1, const SV *sv2);
char sv_pop(SV *sv);

void sv_clone(SV *dst, const SV *src);
