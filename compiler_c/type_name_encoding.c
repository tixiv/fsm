
#include "type_name_encoding.h"
#include "string_builder.h"
#include <stdlib.h>

#include <stdio.h>

SV encode_function_name(SV name, Type* type) {
    char buf[1024];
    SB sb;
    sb_init(&sb, malloc(1024), 1024);

    sb_printf(&sb, "generic_%.*s_%s", SV_prnt(name), get_type_name_encoded_r(buf, type));

    // printf("%s\n", sb.buffer);

    return (SV) {sb.buffer, sb.len};
}
