
#include "sv.h"
#include <string.h>

bool sv_starts_with(const SV *sv, const char *start) {
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

int sv_find_cstr(SV sv, const char *cstr) {
    for (int i = 0; sv.len; i++) {
        if (sv_starts_with(&sv, cstr)) {
            return i;
        }
        sv_pop(&sv);
    }
    return -1;
}
