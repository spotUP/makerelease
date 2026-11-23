#include "releases_client.h"
#include "json_write.h"
#include "multipart.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define HOST "releases.uprough.net"

int rp_url_encode(const char *in, char *out, size_t cap) {
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    for (; *in; in++) {
        unsigned char ch = (unsigned char)*in;
        int plain = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
                    (ch && strchr("-_.!~*'()", ch));
        if (o + (plain ? 1 : 3) + 1 > cap) return -1;
        if (plain) out[o++] = (char)ch;
        else { out[o++] = '%'; out[o++] = hex[ch >> 4]; out[o++] = hex[ch & 15]; }
    }
    out[o] = '\0';
    return 0;
}

static void fail(rp_rel_err *e, int status, const char *fmt, const char *a, int n) {
    memset(e, 0, sizeof *e);
    e->status = status;
    snprintf(e->message, sizeof e->message, fmt, a ? a : "", n);
}

static const char *jstr(const jnode *o, const char *key) {
    const jnode *v = json_get(o, key);
    return (v && v->type == J_STR) ? v->str : NULL;
}

enum { M_GET, M_WRITE, M_PUBLISH };

/* One round trip. M_GET: "returned N: <error|unknown error>", no status on the
 * error. M_WRITE: "<error|returned N>" with status. M_PUBLISH: also reason and
 * (409 only) wordpress_url. Unparsable body counts as {}. */
static int call(const rp_rel_cfg *c, const char *method, const char *path, const char *ctype,
                const char *body, size_t body_len, int mode, jnode **out, rp_rel_err *err) {
    char url[600];
    rp_http_header hdrs[2];
    size_t nh = 0;
    rp_http_req req;
    rp_http_resp resp;
    jnode *j;
    char perr[64];
    const char *msg;

    memset(&resp, 0, sizeof resp);
    if (out) *out = NULL;
    if (snprintf(url, sizeof url, "%s%s", c->base_url, path) >= (int)sizeof url) {
        fail(err, 0, "could not reach " HOST ": url too long%s", NULL, 0);
        return -1;
    }
    if (mode != M_GET) {
        hdrs[nh].name = "X-Release-Key"; hdrs[nh].value = c->write_key; nh++;
    }
    if (ctype) { hdrs[nh].name = "Content-Type"; hdrs[nh].value = ctype; nh++; }
    req.method = method; req.url = url; req.headers = nh ? hdrs : NULL; req.nheaders = nh;
    req.body = body; req.body_len = body_len;

    if (c->http(c->http_ctx, &req, &resp) != 0) {
        free(resp.body);
        fail(err, 0, "could not reach " HOST ": %s%.0d", resp.error, 0);
        return -1;
    }
    j = resp.body ? json_parse(resp.body, perr, sizeof perr) : NULL;
    free(resp.body);
    if (resp.status < 200 || resp.status > 299) {
        msg = j ? jstr(j, "error") : NULL;
        if (mode == M_GET) {
            char e2[200];
            snprintf(e2, sizeof e2, "%s", msg ? msg : "unknown error");
            memset(err, 0, sizeof *err);
            snprintf(err->message, sizeof err->message, HOST " returned %d: %s", resp.status, e2);
        } else {
            memset(err, 0, sizeof *err);
            err->status = resp.status;
            if (msg) snprintf(err->message, sizeof err->message, "%s", msg);
            else snprintf(err->message, sizeof err->message, HOST " returned %d", resp.status);
            if (mode == M_PUBLISH && j) {
                const char *r = jstr(j, "reason"), *w = jstr(j, "wordpressPostUrl");
                if (r) snprintf(err->reason, sizeof err->reason, "%s", r);
                if (w && resp.status == 409) snprintf(err->wordpress_url, sizeof err->wordpress_url, "%s", w);
            }
        }
        json_free(j);
        return -1;
    }
    if (out) *out = j; else json_free(j);
    return 0;
}

static int get_division(const rp_rel_cfg *c, const char *what, const char *division, jnode **out, rp_rel_err *err) {
    char enc[128], path[200];
    rp_url_encode(division, enc, sizeof enc);
    snprintf(path, sizeof path, "/api/releases/%s?division=%s", what, enc);
    return call(c, "GET", path, NULL, NULL, 0, M_GET, out, err);
}

int rp_rel_seed(const rp_rel_cfg *c, const char *d, jnode **out, rp_rel_err *err) {
    return get_division(c, "seed", d, out, err);
}
int rp_rel_manifest(const rp_rel_cfg *c, const char *d, jnode **out, rp_rel_err *err) {
    return get_division(c, "manifest", d, out, err);
}
int rp_rel_numbering_check(const rp_rel_cfg *c, jnode **out, rp_rel_err *err) {
    return call(c, "GET", "/api/releases/numbering-check", NULL, NULL, 0, M_GET, out, err);
}

static int code_path(const char *code, const char *suffix, char *path, size_t cap) {
    char enc[128];
    if (rp_url_encode(code, enc, sizeof enc) != 0) return -1;
    snprintf(path, cap, "/api/releases/%s%s", enc, suffix);
    return 0;
}

int rp_rel_exists(const rp_rel_cfg *c, const char *code, int *exists, rp_rel_err *err) {
    char path[200];
    jnode *j;
    const jnode *e;
    if (code_path(code, "/exists", path, sizeof path)) { fail(err, 0, "invalid code%s", NULL, 0); return -1; }
    if (call(c, "GET", path, NULL, NULL, 0, M_GET, &j, err) != 0) return -1;
    e = json_get(j, "exists");
    *exists = (e && e->type == J_BOOL && e->num) ? 1 : 0;
    json_free(j);
    return 0;
}

int rp_rel_create(const rp_rel_cfg *c, const char *json_body, jnode **out, rp_rel_err *err) {
    return call(c, "POST", "/api/releases", "application/json", json_body, strlen(json_body), M_WRITE, out, err);
}

int rp_rel_publish(const rp_rel_cfg *c, const char *code, const char *content_type,
                   const char *body, size_t body_len, jnode **out, rp_rel_err *err) {
    char path[200];
    if (code_path(code, "/publish", path, sizeof path)) { fail(err, 0, "invalid code%s", NULL, 0); return -1; }
    return call(c, "POST", path, content_type, body, body_len, M_PUBLISH, out, err);
}

int rp_rel_delete(const rp_rel_cfg *c, const char *code, jnode **out, rp_rel_err *err) {
    char path[200];
    if (code_path(code, "", path, sizeof path)) { fail(err, 0, "invalid code%s", NULL, 0); return -1; }
    return call(c, "DELETE", path, NULL, NULL, 0, M_WRITE, out, err);
}

int rp_rel_cfg_for(rp_rel_cfg *out, const char *profile_base, const char *profile_key,
                   const char *env_base, const char *env_key, rp_http_fn http, void *http_ctx) {
    if (!profile_base) return -1;
    out->base_url = (env_base && *env_base) ? env_base : profile_base;
    out->write_key = (env_key && *env_key) ? env_key : (profile_key ? profile_key : "");
    out->http = http;
    out->http_ctx = http_ctx;
    return 0;
}

char *rp_trim_diz(const char *text) {
    size_t start = 0, end, o = 0;
    char *out = malloc(strlen(text) + 1);
    const char *p = text;
    if (!out) return NULL;
    /* every line right-trimmed of blanks/tabs, rejoined with \n */
    for (;;) {
        const char *nl = strchr(p, '\n');
        size_t ln = nl ? (size_t)(nl - p) : strlen(p);
        while (ln && (p[ln - 1] == ' ' || p[ln - 1] == '\t')) ln--;
        memcpy(out + o, p, ln); o += ln;
        if (!nl) break;
        out[o++] = '\n';
        p = nl + 1;
    }
    end = o;
    while (start < end && out[start] == '\n') start++;
    while (end > start && out[end - 1] == '\n') end--;
    memmove(out, out + start, end - start);
    out[end - start] = '\0';
    return out;
}

#define P(s) sb_puts(&b, s)
#define S(k, v) do { if (jw_key(&b, k, first) || jw_str(&b, v)) goto oom; first = 0; } while (0)
#define K(k) do { if (jw_key(&b, k, first)) goto oom; first = 0; } while (0)
#define PAIR(k, v, n) (jw_key(&b, k, n) || jw_str(&b, v))

char *rp_rel_publish_body(const rp_rel_form *f) {
    strbuf b = {0};
    char *diz = rp_trim_diz(f->diz_banner);
    size_t i;
    int first = 1;
    if (!diz) return NULL;
    if (P("{")) goto oom;
    S("code", f->code); S("format", f->format); S("releaseName", f->release_name);
    S("platformTag", f->platform_tag); S("releaseNotes", f->release_notes); S("news", f->news);
    S("dizText", diz); S("entryTitle", f->entry_title); S("entryCategory", f->entry_category);
    S("entryCreators", f->entry_creators);
    if (f->division) S("division", f->division);
    if (f->boards) {
        K("boards");
        if (P("[")) goto oom;
        for (i = 0; i < f->n_boards; i++) {
            const rp_rel_board *x = &f->boards[i];
            if ((i && P(",")) || P("{") || PAIR("name", x->name, 1) || PAIR("sysop", x->sysop, 0) ||
                PAIR("hq", x->hq, 0) || PAIR("status", x->status, 0) || P("}")) goto oom;
        }
        if (P("]")) goto oom;
    }
    K("members");
    if (P("[")) goto oom;
    for (i = 0; i < f->n_members; i++)
        if ((i && P(",")) || P("{") || PAIR("nickname", f->members[i].nickname, 1) ||
            PAIR("role", f->members[i].role, 0) || P("}")) goto oom;
    if (P("]")) goto oom;
    S("description", f->description);
    K("credits");
    if (P("[")) goto oom;
    for (i = 0; i < f->n_credits; i++)
        if ((i && P(",")) || P("{") || PAIR("role", f->credits[i].role, 1) ||
            PAIR("name", f->credits[i].name, 0) || P("}")) goto oom;
    if (P("]")) goto oom;
    K("mediaEmbeds");
    if (P("[")) goto oom;
    for (i = 0; i < f->n_embeds; i++) {
        const rp_rel_embed *e = &f->embeds[i];
        if ((i && P(",")) || P("{") || PAIR("type", e->type, 1) || PAIR("caption", e->caption, 0) ||
            (!strcmp(e->type, "youtube") && PAIR("youtubeUrl", e->youtube_url, 0)) || P("}")) goto oom;
    }
    if (P("]") || P("}")) goto oom;
    free(diz);
    return b.data;
oom:
    free(diz); free(b.data);
    return NULL;
}

int rp_rel_publish_files(const rp_rel_cfg *c, const char *code, unsigned long boundary_seed,
                         const rp_rel_file *archive, const rp_rel_file *image,
                         const rp_rel_file *embed_images, size_t n_embeds,
                         jnode **out, rp_rel_err *err) {
    rp_mp mp;
    char boundary[72], ctype[128];
    size_t i;
    int rc;
    rp_mp_boundary(boundary, sizeof boundary, boundary_seed);
    rp_mp_init(&mp, boundary);
    rc = rp_mp_file(&mp, "archive", archive->filename, archive->data, archive->len) ||
         rp_mp_file(&mp, "image", image->filename, image->data, image->len);
    /* embedImage parts are matched by position on the server */
    for (i = 0; !rc && i < n_embeds; i++)
        rc = rp_mp_file(&mp, "embedImage", embed_images[i].filename, embed_images[i].data, embed_images[i].len);
    if (rc || rp_mp_finish(&mp, ctype, sizeof ctype)) {
        free(mp.body.data);
        fail(err, 0, "could not reach " HOST ": out of memory%s", NULL, 0);
        return -1;
    }
    rc = rp_rel_publish(c, code, ctype, mp.body.data, mp.body.len, out, err);
    free(mp.body.data);
    return rc;
}
