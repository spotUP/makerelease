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

void rp_suggest_code(const char *name, int length, char *out, size_t cap) {
    char up[128], letters[16], first[16] = "";
    int nl = 0, nd = 0, end, room, i;
    const char *digits;
    rp_upper(name, up, sizeof(up));
    end = (int)strlen(up);
    while (end > 0 && up[end - 1] == ' ') end--;
    digits = up + end;
    while (digits > up && digits[-1] >= '0' && digits[-1] <= '9') digits--;
    nd = (int)(up + end - digits);
    if (nd > length) { digits += nd - length; nd = length; }
    room = length - nd;
    if (room > (int)sizeof(letters) - 1) room = (int)sizeof(letters) - 1;
    for (i = 0; i < (int)(digits - up) && i < end; i++) {
        int in_word = up[i] >= 'A' && up[i] <= 'Z';
        if (!in_word) continue;
        if (i > 0 && up[i - 1] >= 'A' && up[i - 1] <= 'Z') continue;
        if (nl < room) letters[nl++] = up[i];
        if (first[0] == '\0') {
            int k = 0;
            while (up[i + k] >= 'A' && up[i + k] <= 'Z' && k < (int)sizeof(first) - 1) { first[k] = up[i + k]; k++; }
            first[k] = '\0';
        }
    }
    for (i = 1; nl < room && first[i] != '\0'; i++) letters[nl++] = first[i];
    letters[nl] = '\0';
    snprintf(out, cap, "%s%.*s", letters, nd, digits);
}
