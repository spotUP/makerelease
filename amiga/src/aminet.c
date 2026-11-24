#include "aminet.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "strbuf.h"
#include "wrap.h"

int rp_aminet_names(const char *base, char *archive, char *readme, size_t cap)
{
    size_t n = strlen(base), stem = n, i;
    const char *dot = strrchr(base, '.');
    if (n + 1 > cap) return -1;
    for (i = 0; i < n; i++) archive[i] = (char)tolower((unsigned char)base[i]);
    archive[n] = 0;
    if (dot && dot != base) stem = (size_t)(dot - base);   /* regex /\.[^.]+$/ needs a char after the dot */
    if (dot && dot[1] == 0) stem = n;
    if (stem + 8 > cap) return -1;
    memcpy(readme, archive, stem);
    strcpy(readme + stem, ".readme");
    return 0;
}

char *rp_aminet_normalize_readme(const char *in)
{
    strbuf b = {0};
    const unsigned char *p = (const unsigned char *)in;
    static const struct { unsigned char c; const char *s; } fold[] = {
        {0xE4, "ae"}, {0xF6, "oe"}, {0xE5, "aa"}, {0xFC, "ue"}, {0xE9, "e"}, {0xE8, "e"}, {0xEA, "e"},
        {0xC4, "Ae"}, {0xD6, "Oe"}, {0xC5, "Aa"}, {0xDC, "Ue"}, {0xC9, "E"},
        {0xF1, "n"}, {0xE7, "c"}, {0xF8, "oe"}, {0xD8, "Oe"}, {0xE6, "ae"}, {0xC6, "Ae"},
    };
    if (sb_putn(&b, "", 0)) return NULL;
    for (; *p; p++) {
        int ok = 0;
        if (*p == '\r') { if (p[1] == '\n') p++; ok = !sb_putn(&b, "\n", 1); }
        else if (*p < 0x80) ok = !sb_putn(&b, (const char *)p, 1);
        else {
            size_t i; const char *s = "?";
            for (i = 0; i < sizeof fold / sizeof fold[0]; i++) if (fold[i].c == *p) s = fold[i].s;
            ok = !sb_putn(&b, s, strlen(s));
        }
        if (!ok) { free(b.data); return NULL; }
    }
    if (b.len == 0 || b.data[b.len - 1] != '\n') if (sb_putn(&b, "\n", 1)) { free(b.data); return NULL; }
    return b.data;
}

static void add(char *msg, size_t cap, int *n, const char *fmt, const char *a, int x, int y)
{
    size_t l = strlen(msg);
    char one[200];
    snprintf(one, sizeof one, fmt, a, x, y);
    if (*n) snprintf(msg + l, cap - l, "; %s", one); else snprintf(msg, cap, "%s", one);
    (*n)++;
}

/* Header value of "Field:" among the lines before the first blank one; "" when absent. */
static int header_value(const char *text, const char *field, char *out, size_t cap)
{
    size_t fl = strlen(field);
    const char *p = text;
    out[0] = 0;
    while (*p && *p != '\n') {
        const char *e = strchr(p, '\n');
        size_t ll = e ? (size_t)(e - p) : strlen(p);
        if (ll > fl && !strncmp(p, field, fl) && p[fl] == ':') {
            const char *v = p + fl + 1, *end = p + ll;
            size_t n;
            while (v < end && isspace((unsigned char)*v)) v++;
            while (end > v && isspace((unsigned char)end[-1])) end--;
            n = (size_t)(end - v);
            if (n >= cap) n = cap - 1;
            memcpy(out, v, n); out[n] = 0;
            return 1;
        }
        if (!e) break;
        p = e + 1;
    }
    return 0;
}

static int plain_ok(const char *s, int slash_ok)   /* [a-z0-9]+ (or a/b when slash_ok) */
{
    int n = 0, sl = 0;
    for (; *s; s++) {
        if (*s == '/' && slash_ok && n && !sl && s[1]) { sl = 1; continue; }
        if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9'))) return 0;
        n++;
    }
    return n > 0 && (!slash_ok || sl);
}

/* /^[^\s@()]+@[^\s@()]+\.[^\s@()]+ \(.+\)$/ */
static int uploader_ok(const char *s)
{
    const char *at, *sp, *p;
    size_t n = strlen(s);
    at = strchr(s, '@');
    if (!at || at == s) return 0;
    for (p = s; p < at; p++) if (isspace((unsigned char)*p) || *p == '(' || *p == ')') return 0;
    sp = strstr(at, " (");
    if (!sp || sp - at < 4 || n < 2 || s[n - 1] != ')' || sp + 3 > s + n - 1) return 0;
    for (p = at + 1; p < sp; p++) if (isspace((unsigned char)*p) || *p == '@' || *p == '(' || *p == ')') return 0;
    {   /* a dot inside the domain with characters on both sides */
        const char *d = memchr(at + 1, '.', (size_t)(sp - at - 1));
        if (!d || d == at + 1) return 0;
        for (p = d + 1; p < sp; p++) { (void)p; return 1; }
    }
    return 0;
}

int rp_aminet_problems(const char *archive, const char *readme, const char *text, char *msg, size_t cap)
{
    const char *names[2]; int n = 0, i, line = 1;
    char v[160];
    const char *p;
    msg[0] = 0;
    names[0] = archive; names[1] = readme;
    for (i = 0; i < 2; i++) {
        const char *s;
        int bad = 0;
        if (strlen(names[i]) > RP_AMINET_FILENAME_MAX)
            add(msg, cap, &n, "\"%s\" is %d characters; Aminet allows 30", names[i], (int)strlen(names[i]), 0);
        for (s = names[i]; *s; s++) if (!isalnum((unsigned char)*s) && !strchr("._-", *s)) bad = 1;
        if (bad || !*names[i])
            add(msg, cap, &n, "\"%s\" may only contain letters, digits, dots, underscores and hyphens", names[i], 0, 0);
    }
    if (strchr(text, '\r')) add(msg, cap, &n, "the readme must use LF line ends, not CR LF%s", "", 0, 0);
    if (strncmp(text, "Short:", 6)) add(msg, cap, &n, "the first line must be \"Short:\"%s", "", 0, 0);
    header_value(text, "Short", v, sizeof v);
    if (!v[0]) add(msg, cap, &n, "Short: is empty%s", "", 0, 0);
    else if (strlen(v) > RP_AMINET_SHORT_MAX) add(msg, cap, &n, "%sShort: is %d characters; Aminet allows 40", "", (int)strlen(v), 0);
    header_value(text, "Uploader", v, sizeof v);
    if (!uploader_ok(v)) add(msg, cap, &n, "Uploader: needs an email address and a name, as \"you@example.com (Your Name)\"%s", "", 0, 0);
    header_value(text, "Type", v, sizeof v);
    if (!plain_ok(v, 1) || strchr(v, '/') != strrchr(v, '/'))
        add(msg, cap, &n, "Type: must be an Aminet directory such as \"demo/aga\"%s", "", 0, 0);
    header_value(text, "Architecture", v, sizeof v);
    if (!v[0]) add(msg, cap, &n, "Architecture: is empty%s", "", 0, 0);
    for (p = text; *p; line++) {
        const char *e = strchr(p, '\n');
        size_t ll = e ? (size_t)(e - p) : strlen(p);
        if (ll > RP_AMINET_LINE_MAX) add(msg, cap, &n, "%sline %d is %d characters; Aminet allows 78", "", line, (int)ll);
        if (!e) break;
        p = e + 1;
    }
    return n;
}

/* ---- FTP (RFC 959) over rp_tcp_ops ---- */
typedef struct {
    const rp_tcp_ops *ops; void *ctx; int h;
    char buf[1024]; size_t len;   /* unread bytes from the control connection */
    char line[256];               /* last reply line */
} ftp;

/* Reads one reply (multi-line aware); returns its 3-digit code, or -1. */
static int ftp_reply(ftp *f)
{
    int code = -1, first = -1;
    for (;;) {
        char *nl;
        while (!(nl = memchr(f->buf, '\n', f->len))) {
            int k;
            if (f->len == sizeof f->buf) return -1;
            k = f->ops->recv(f->ctx, f->h, f->buf + f->len, sizeof f->buf - f->len);
            if (k <= 0) return -1;
            f->len += (size_t)k;
        }
        {
            size_t ll = (size_t)(nl - f->buf), keep = ll < sizeof f->line - 1 ? ll : sizeof f->line - 1;
            memcpy(f->line, f->buf, keep); f->line[keep] = 0;
            if (keep && f->line[keep - 1] == '\r') f->line[keep - 1] = 0;
            memmove(f->buf, nl + 1, f->len - ll - 1); f->len -= ll + 1;
        }
        if (isdigit((unsigned char)f->line[0]) && isdigit((unsigned char)f->line[1]) && isdigit((unsigned char)f->line[2])) {
            code = atoi(f->line);
            if (first < 0) first = code;
            if (f->line[3] == ' ' && code == first) return code;   /* a "123-" opener ends at "123 " */
        }
    }
}

static int ftp_cmd(ftp *f, const char *fmt, const char *arg)
{
    char c[300];
    int n = snprintf(c, sizeof c, fmt, arg ? arg : "");
    if (n <= 0 || (size_t)n >= sizeof c - 2) return -1;
    memcpy(c + n, "\r\n", 3);
    if (f->ops->send(f->ctx, f->h, c, (size_t)n + 2)) return -1;
    return ftp_reply(f);
}

/* "227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)" -> host text and port. */
static int parse_pasv(const char *l, char *host, size_t hcap, int *port)
{
    int v[6], i = 0;
    const char *p = strchr(l, '(');
    if (!p) return -1;
    for (p++; i < 6; i++) {
        char *e;
        long x = strtol(p, &e, 10);
        if (e == p || x < 0 || x > 255) return -1;
        v[i] = (int)x; p = e;
        if (i < 5) { if (*p != ',') return -1; p++; }
    }
    snprintf(host, hcap, "%d.%d.%d.%d", v[0], v[1], v[2], v[3]);
    *port = v[4] * 256 + v[5];
    return 0;
}

static int ftp_put(ftp *f, const char *name, const char *data, size_t len, char *err, size_t cap)
{
    char host[32], oerr[96];
    int port, d, c;
    if ((c = ftp_cmd(f, "PASV%s", "")) != 227 || parse_pasv(f->line, host, sizeof host, &port)) {
        snprintf(err, cap, "passive mode refused (%s)", f->line); return -1;
    }
    if ((d = f->ops->open(f->ctx, host, port, oerr, sizeof oerr)) < 0) { snprintf(err, cap, "data connection: %s", oerr); return -1; }
    c = ftp_cmd(f, "STOR %s", name);
    if (c != 150 && c != 125) { f->ops->close(f->ctx, d); snprintf(err, cap, "%s", f->line); return -1; }
    if (len && f->ops->send(f->ctx, d, data, len)) { f->ops->close(f->ctx, d); snprintf(err, cap, "send failed"); return -1; }
    f->ops->close(f->ctx, d);   /* closing the data connection ends the transfer */
    if (ftp_reply(f) != 226) { snprintf(err, cap, "%s", f->line); return -1; }
    return 0;
}

int rp_aminet_upload(const rp_tcp_ops *ops, void *ctx, const char *base, const char *email,
                     const char *readme_text, const char *data, size_t len, char *msg, size_t cap)
{
    char an[64], rn[64], probs[512], err[160], oerr[96], pass[160];
    const char *at;
    ftp f;
    int c;
    if (rp_aminet_names(base, an, rn, sizeof an)) { snprintf(msg, cap, "not uploaded to Aminet: file name too long"); return -1; }
    if (rp_aminet_problems(an, rn, readme_text, probs, sizeof probs)) { snprintf(msg, cap, "not uploaded to Aminet: %s", probs); return -1; }
    at = strchr(email, '@');
    if (!at || strlen(email) >= sizeof pass) { snprintf(msg, cap, "not uploaded to Aminet: the uploader email is missing"); return -1; }
    snprintf(pass, sizeof pass, "%s", email);

    memset(&f, 0, sizeof f);
    f.ops = ops; f.ctx = ctx;
    if ((f.h = ops->open(ctx, RP_AMINET_HOST, 21, oerr, sizeof oerr)) < 0) { snprintf(msg, cap, "Aminet upload of %s failed: %s", an, oerr); return -1; }
    err[0] = 0;
    if (ftp_reply(&f) != 220) snprintf(err, sizeof err, "no greeting");
    else if ((c = ftp_cmd(&f, "USER %s", "anonymous")) != 331 && c != 230) snprintf(err, sizeof err, "%s", f.line);
    else if (c == 331 && ftp_cmd(&f, "PASS %s", pass) != 230) snprintf(err, sizeof err, "login refused (%s)", f.line);
    else if (ftp_cmd(&f, "TYPE I%s", "") != 200) snprintf(err, sizeof err, "%s", f.line);
    else if (ftp_cmd(&f, "CWD %s", RP_AMINET_DIR) != 250) snprintf(err, sizeof err, "%s", f.line);
    else if (ftp_put(&f, an, data, len, err, sizeof err) == 0) {
        /* archive first, readme second: Aminet pairs the readme with the file of the same base name */
        if (ftp_put(&f, rn, readme_text, strlen(readme_text), err, sizeof err) == 0) {
            ftp_cmd(&f, "QUIT%s", "");
            ops->close(ctx, f.h);
            snprintf(msg, cap, "uploaded %s and %s to Aminet (it appears once a moderator has looked at it)", an, rn);
            return 0;
        }
        snprintf(msg, cap, "Aminet upload of %s failed: %s", rn, err);
        ops->close(ctx, f.h);
        return -1;
    }
    snprintf(msg, cap, "Aminet upload of %s failed: %s", an, err);
    ops->close(ctx, f.h);
    return -1;
}

/* ---- renderAminetReadme ---- */

typedef struct { char **v; size_t n, cap; int oom; } lvec;

static void lv_add(lvec *l, const char *s, size_t len)
{
    char *d;
    if (l->oom) return;
    if (l->n == l->cap) {
        char **p = realloc(l->v, (l->cap ? l->cap * 2 : 16) * sizeof *p);
        if (!p) { l->oom = 1; return; }
        l->v = p; l->cap = l->cap ? l->cap * 2 : 16;
    }
    d = malloc(len + 1);
    if (!d) { l->oom = 1; return; }
    memcpy(d, s, len); d[len] = 0;
    l->v[l->n++] = d;
}

static void lv_free(lvec *l) { size_t i; for (i = 0; i < l->n; i++) free(l->v[i]); free(l->v); }

static void lv_wrap(lvec *l, const char *text)
{
    rp_lines w = rp_wrap_words(text, RP_AMINET_LINE_MAX, RP_AMINET_LINE_MAX);
    size_t i;
    for (i = 0; i < w.n; i++) lv_add(l, w.lines[i], strlen(w.lines[i]));
    rp_free_lines(&w);
}

/* wrapPreformatted: own line breaks kept, only lines over 78 columns re-wrapped. */
static void lv_preformatted(lvec *l, const char *text)
{
    const char *p = text;
    for (;;) {
        const char *e = p;
        size_t n;
        while (*e && *e != '\n' && *e != '\r') e++;
        n = (size_t)(e - p);
        if (n > RP_AMINET_LINE_MAX) {
            char *tmp = malloc(n + 1);
            if (!tmp) { l->oom = 1; return; }
            memcpy(tmp, p, n); tmp[n] = 0;
            lv_wrap(l, tmp);
            free(tmp);
        } else {
            while (n && (p[n - 1] == ' ' || p[n - 1] == '\t')) n--;
            lv_add(l, p, n);
        }
        if (!*e) break;
        if (*e == '\r' && e[1] == '\n') e++;
        p = e + 1;
    }
}

static const char *trim(const char *s, size_t *len)
{
    const char *e;
    while (isspace((unsigned char)*s)) s++;
    e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    *len = (size_t)(e - s);
    return s;
}

static void put_trim(strbuf *b, const char *s) { size_t n; s = trim(s, &n); sb_putn(b, s, n); }

static void header(strbuf *b, const char *label, const char *value)
{
    size_t n = strlen(label) + 1;
    sb_puts(b, label); sb_puts(b, ":");
    for (; n < 14; n++) sb_puts(b, " ");
    put_trim(b, value);
    sb_puts(b, "\n");
}

static void edges(lvec *l, size_t *first, size_t *end)
{
    while (*first < *end && !l->v[*first][0]) (*first)++;
    while (*end > *first && !l->v[*end - 1][0]) (*end)--;
}

char *rp_aminet_render_readme(const rp_aminet_readme_in *in)
{
    strbuf b = {0};
    lvec notes = {0}, body = {0};
    size_t i, nf, ne, bf, be, dl;
    const char *desc = trim(in->description, &dl);
    size_t cl;
    const char *cr = trim(in->entry_creators, &cl);

    lv_preformatted(&notes, in->release_notes);
    nf = 0; ne = notes.n; edges(&notes, &nf, &ne);
    if (nf < ne) {
        size_t n; const char *t = trim(notes.v[nf], &n);
        if (dl && n == dl && !memcmp(t, desc, n)) { nf++; edges(&notes, &nf, &ne); }
    }
    lv_wrap(&body, in->description);
    lv_add(&body, "", 0);
    for (i = nf; i < ne; i++) lv_add(&body, notes.v[i], strlen(notes.v[i]));
    {
        int any = 0;
        for (i = 0; i < in->n_credits; i++) {
            size_t rl, nl; const char *r = trim(in->credits[i].role, &rl), *nm = trim(in->credits[i].name, &nl);
            char *line;
            if (!rl && !nl) continue;
            if (!any) { lv_add(&body, "", 0); lv_add(&body, "Credits:", 8); any = 1; }
            line = malloc(rl + nl + 5);
            if (!line) { body.oom = 1; break; }
            sprintf(line, "  %.*s: %.*s", (int)rl, r, (int)nl, nm);
            lv_preformatted(&body, line);
            free(line);
        }
    }
    lv_add(&body, "", 0);
    {
        char *sig = malloc(strlen(in->group_label) + strlen(in->group_url) + 32);
        if (!sig) body.oom = 1;
        else { sprintf(sig, "Released by %s - %s", in->group_label, in->group_url); lv_add(&body, sig, strlen(sig)); free(sig); }
    }
    if (notes.oom || body.oom) { lv_free(&notes); lv_free(&body); return NULL; }

    header(&b, "Short", in->short_text);
    sb_puts(&b, "Uploader:     "); put_trim(&b, in->uploader_email); sb_puts(&b, " ("); put_trim(&b, in->uploader_name); sb_puts(&b, ")\n");
    sb_puts(&b, "Author:       "); sb_puts(&b, in->group_label);
    if (cl) { sb_puts(&b, " ("); sb_putn(&b, cr, cl); sb_puts(&b, ")"); }
    sb_puts(&b, "\n");
    header(&b, "Type", in->type);
    header(&b, "Architecture", in->architecture);
    header(&b, "Distribution", "Aminet");
    sb_puts(&b, "\n");
    bf = 0; be = body.n; edges(&body, &bf, &be);
    for (i = bf; i < be; i++) { sb_puts(&b, body.v[i]); sb_puts(&b, "\n"); }
    lv_free(&notes); lv_free(&body);
    return b.data;
}

int rp_aminet_prefs_load(const char *path, rp_aminet_prefs *p) {
    FILE *f = fopen(path, "rb");
    char *dst[4], line[RP_AMINET_PREF_MAX + 8];
    int i;
    if (!f) return -1;
    dst[0] = p->directory; dst[1] = p->architecture; dst[2] = p->uploader; dst[3] = p->email;
    memset(p, 0, sizeof *p);
    for (i = 0; i < 4 && fgets(line, sizeof line, f); i++) {
        size_t n = strcspn(line, "\r\n");
        if (n >= RP_AMINET_PREF_MAX) n = RP_AMINET_PREF_MAX - 1;
        memcpy(dst[i], line, n);
        dst[i][n] = '\0';
    }
    fclose(f);
    return 0;
}

int rp_aminet_prefs_save(const char *path, const rp_aminet_prefs *p) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "%s\n%s\n%s\n%s\n", p->directory, p->architecture, p->uploader, p->email);
    return fclose(f) == 0 ? 0 : -1;
}
