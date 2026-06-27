
#include "sv.h"
#include <string.h>

bool sv_starts_with(SV *sv, const char *start) {
    return memcmp(sv->begin, start, strlen(start)) == 0;
}

bool sv_compare_cstr(const SV *sv, const char *cstr) {
    return sv->len == strlen(cstr) && memcmp(sv->begin, cstr, sv->len) == 0;
}

bool sv_equal(const SV *sv1, const SV *sv2) {
    return sv1->len == sv2->len && memcmp(sv1->begin, sv2->begin, sv1->len) == 0;
}

char sv_pop(SV *sv)
{
    if (sv->len == 0) return 0;
    
    char c = *sv->begin++;
    sv->len--;
    return c;
}

void sv_clone(SV *dst, const SV *src) {
    dst->begin = src->begin;
    dst->len = src->len;
}
