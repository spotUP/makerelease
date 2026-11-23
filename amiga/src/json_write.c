#include "json_write.h"
#include <stdio.h>

int jw_str(strbuf *b, const char *s) {
    if (sb_puts(b, "\"")) return -1;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        char esc[8];
        const char *o = NULL;
        switch (c) {
        case '"': o = "\\\""; break;
        case '\\': o = "\\\\"; break;
        case '\b': o = "\\b"; break;
        case '\f': o = "\\f"; break;
        case '\n': o = "\\n"; break;
        case '\r': o = "\\r"; break;
        case '\t': o = "\\t"; break;
        default:
            if (c < 0x20) { snprintf(esc, sizeof esc, "\\u%04x", c); o = esc; }
        }
        if (o ? sb_puts(b, o) : sb_putn(b, s, 1)) return -1;
    }
    return sb_puts(b, "\"");
}

int jw_key(strbuf *b, const char *key, int first) {
    if (!first && sb_puts(b, ",")) return -1;
    if (jw_str(b, key)) return -1;
    return sb_puts(b, ":");
}
