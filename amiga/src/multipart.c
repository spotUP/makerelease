#include "multipart.h"
#include <stdio.h>

void rp_mp_boundary(char *out, size_t cap, unsigned long seed) {
    snprintf(out, cap, "----formdata-undici-0%011lu", seed % 100000000000UL);
}

void rp_mp_init(rp_mp *m, const char *boundary) {
    memset(m, 0, sizeof *m);
    snprintf(m->boundary, sizeof m->boundary, "%s", boundary);
}

static int put_esc(strbuf *b, const char *s) {
    for (; *s; s++) {
        const char *o = *s == '\n' ? "%0A" : *s == '\r' ? "%0D" : *s == '"' ? "%22" : NULL;
        if (o ? sb_puts(b, o) : sb_putn(b, s, 1)) return -1;
    }
    return 0;
}

static int head(rp_mp *m, const char *name, const char *filename) {
    strbuf *b = &m->body;
    if (sb_puts(b, "--") || sb_puts(b, m->boundary) || sb_puts(b, "\r\nContent-Disposition: form-data; name=\"") ||
        put_esc(b, name) || sb_puts(b, "\"")) return -1;
    if (filename && (sb_puts(b, "; filename=\"") || put_esc(b, filename) || sb_puts(b, "\"\r\nContent-Type: application/octet-stream"))) return -1;
    return sb_puts(b, "\r\n\r\n");
}

int rp_mp_field(rp_mp *m, const char *name, const char *value) {
    return head(m, name, NULL) || sb_puts(&m->body, value) || sb_puts(&m->body, "\r\n");
}

int rp_mp_file(rp_mp *m, const char *name, const char *filename, const char *data, size_t len) {
    return head(m, name, filename) || sb_putn(&m->body, data, len) || sb_puts(&m->body, "\r\n");
}

int rp_mp_finish(rp_mp *m, char *ctype, size_t cap) {
    if (sb_puts(&m->body, "--") || sb_puts(&m->body, m->boundary) || sb_puts(&m->body, "--\r\n")) return -1;
    snprintf(ctype, cap, "multipart/form-data; boundary=%s", m->boundary);
    return 0;
}
