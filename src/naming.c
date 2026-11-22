#include "naming.h"

#include <string.h>
#include <stdio.h>

void rp_upper(const char *in, char *out, size_t cap) {
    size_t pos = 0;
    for (const char *p = in; *p && pos + 1 < cap; p++) {
        char c = *p;
        out[pos++] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
    }
    out[pos] = '\0';
}

void rp_lower(const char *in, char *out, size_t cap) {
    size_t pos = 0;
    for (const char *p = in; *p && pos + 1 < cap; p++) {
        char c = *p;
        out[pos++] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    out[pos] = '\0';
}

void rp_base_name(const char *prefix, const char *code, char *out, size_t cap) {
    char upper[64];
    size_t pos = 0;
    rp_upper(code, upper, sizeof(upper));
    for (const char *p = prefix; *p && pos + 1 < cap; p++) out[pos++] = *p;
    for (const char *p = upper; *p && pos + 1 < cap; p++) out[pos++] = *p;
    out[pos] = '\0';
}

void rp_archive_name(const char *prefix, const char *code, int lha, char *out, size_t cap) {
    char base[64];
    rp_base_name(prefix, code, base, sizeof(base));
    snprintf(out, cap, "%s.%s", base, lha ? "LHA" : "ZIP");
}
