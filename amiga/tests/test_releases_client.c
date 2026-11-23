#include "releases_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK_STR(a, b) do { \
    if (strcmp((a), (b)) != 0) { printf("FAIL %d: got [%s] expect [%s]\n", __LINE__, (a), (b)); failures++; } \
} while (0)
#define CHECK_INT(a, b) do { \
    long va_ = (long)(a), vb_ = (long)(b); \
    if (va_ != vb_) { printf("FAIL %d: got %ld expect %ld\n", __LINE__, va_, vb_); failures++; } \
} while (0)

/* Fake transport: records the request, answers with the canned reply. */
typedef struct {
    int rc, status;
    const char *reply;
    const char *error;
    char method[16], url[300], key[64], ctype[64], body[256];
    int has_key, has_ctype;
    size_t nheaders, body_len;
} fake;

static int fake_http(void *ctx, const rp_http_req *r, rp_http_resp *resp) {
    fake *f = ctx;
    size_t i;
    snprintf(f->method, sizeof f->method, "%s", r->method);
    snprintf(f->url, sizeof f->url, "%s", r->url);
    f->nheaders = r->nheaders;
    f->has_key = f->has_ctype = 0;
    for (i = 0; i < r->nheaders; i++) {
        if (!strcmp(r->headers[i].name, "X-Release-Key")) { f->has_key = 1; snprintf(f->key, sizeof f->key, "%s", r->headers[i].value); }
        if (!strcmp(r->headers[i].name, "Content-Type")) { f->has_ctype = 1; snprintf(f->ctype, sizeof f->ctype, "%s", r->headers[i].value); }
    }
    f->body_len = r->body_len;
    snprintf(f->body, sizeof f->body, "%s", r->body ? r->body : "");
    if (f->rc) { snprintf(resp->error, sizeof resp->error, "%s", f->error); return f->rc; }
    resp->status = f->status;
    resp->body = f->reply ? strdup(f->reply) : NULL;
    return 0;
}

static fake F;
static rp_rel_cfg cfg = { "https://releases.uprough.net", "sekrit", fake_http, &F };

static void setup(int status, const char *reply) {
    memset(&F, 0, sizeof F);
    F.status = status; F.reply = reply;
}

int main(void) {
    jnode *j;
    rp_rel_err e;
    int ex = -1;

    /* GET seed: division encoded, no key header, body parsed */
    setup(200, "{\"nextReleaseNumber\":7}");
    CHECK_INT(rp_rel_seed(&cfg, "up rough", &j, &e), 0);
    CHECK_STR(F.method, "GET");
    CHECK_STR(F.url, "https://releases.uprough.net/api/releases/seed?division=up%20rough");
    CHECK_INT(F.nheaders, 0);
    CHECK_INT(json_get(j, "nextReleaseNumber")->num, 7);
    json_free(j);

    setup(200, "{\"history\":[]}");
    CHECK_INT(rp_rel_manifest(&cfg, "x", &j, &e), 0);
    CHECK_STR(F.url, "https://releases.uprough.net/api/releases/manifest?division=x");
    json_free(j);

    setup(200, "{\"checked\":false}");
    CHECK_INT(rp_rel_numbering_check(&cfg, &j, &e), 0);
    CHECK_STR(F.url, "https://releases.uprough.net/api/releases/numbering-check");
    json_free(j);

    /* exists */
    setup(200, "{\"exists\":true}");
    CHECK_INT(rp_rel_exists(&cfg, "A/B", &ex, &e), 0);
    CHECK_INT(ex, 1);
    CHECK_STR(F.url, "https://releases.uprough.net/api/releases/A%2FB/exists");
    setup(200, "{\"exists\":false}");
    CHECK_INT(rp_rel_exists(&cfg, "ABC", &ex, &e), 0);
    CHECK_INT(ex, 0);

    /* GET error: message with server text, then fallbacks (not JSON, no error key) */
    setup(500, "{\"error\":\"boom\"}");
    CHECK_INT(rp_rel_seed(&cfg, "x", &j, &e), -1);
    CHECK_STR(e.message, "releases.uprough.net returned 500: boom");
    setup(502, "<html>");
    CHECK_INT(rp_rel_seed(&cfg, "x", &j, &e), -1);
    CHECK_STR(e.message, "releases.uprough.net returned 502: unknown error");

    /* transport failure */
    setup(0, NULL);
    F.rc = 1; F.error = "connection refused";
    CHECK_INT(rp_rel_seed(&cfg, "x", &j, &e), -1);
    CHECK_STR(e.message, "could not reach releases.uprough.net: connection refused");
    CHECK_INT(e.status, 0);

    /* POST create */
    setup(200, "{\"releaseNumber\":8,\"revision\":3,\"history\":[]}");
    CHECK_INT(rp_rel_create(&cfg, "{\"code\":\"ABC\"}", &j, &e), 0);
    CHECK_STR(F.method, "POST");
    CHECK_STR(F.url, "https://releases.uprough.net/api/releases");
    CHECK_STR(F.key, "sekrit");
    CHECK_STR(F.ctype, "application/json");
    CHECK_STR(F.body, "{\"code\":\"ABC\"}");
    CHECK_INT(json_get(j, "releaseNumber")->num, 8);
    json_free(j);
    setup(401, "{\"error\":\"bad key\"}");
    CHECK_INT(rp_rel_create(&cfg, "{}", &j, &e), -1);
    CHECK_STR(e.message, "bad key");
    CHECK_INT(e.status, 401);
    setup(503, "");
    CHECK_INT(rp_rel_create(&cfg, "{}", &j, &e), -1);
    CHECK_STR(e.message, "releases.uprough.net returned 503");

    /* POST publish: multipart passthrough, 409 reason + wordpress url */
    setup(200, "{\"ok\":true}");
    CHECK_INT(rp_rel_publish(&cfg, "ab c", "multipart/form-data; boundary=zz", "BODY", 4, &j, &e), 0);
    CHECK_STR(F.method, "POST");
    CHECK_STR(F.url, "https://releases.uprough.net/api/releases/ab%20c/publish");
    CHECK_STR(F.key, "sekrit");
    CHECK_STR(F.ctype, "multipart/form-data; boundary=zz");
    CHECK_INT(F.body_len, 4);
    json_free(j);
    setup(409, "{\"error\":\"already published\",\"reason\":\"already-published\",\"wordpressPostUrl\":\"https://w/p\"}");
    CHECK_INT(rp_rel_publish(&cfg, "ABC", "multipart/form-data", "x", 1, &j, &e), -1);
    CHECK_STR(e.message, "already published");
    CHECK_INT(e.status, 409);
    CHECK_STR(e.reason, "already-published");
    CHECK_STR(e.wordpress_url, "https://w/p");
    setup(500, "{\"error\":\"x\",\"wordpressPostUrl\":\"https://w/p\"}");
    rp_rel_publish(&cfg, "ABC", "m", "x", 1, &j, &e);
    CHECK_STR(e.wordpress_url, "");

    /* DELETE */
    setup(200, "{\"deleted\":true,\"code\":\"ABC\"}");
    CHECK_INT(rp_rel_delete(&cfg, "ABC", &j, &e), 0);
    CHECK_STR(F.method, "DELETE");
    CHECK_STR(F.url, "https://releases.uprough.net/api/releases/ABC");
    CHECK_STR(F.key, "sekrit");
    CHECK_INT(F.has_ctype, 0);
    json_free(j);
    setup(404, "{\"error\":\"no such release\"}");
    CHECK_INT(rp_rel_delete(&cfg, "ABC", &j, &e), -1);
    CHECK_STR(e.message, "no such release");
    CHECK_INT(e.status, 404);

    if (failures == 0) puts("releases_client: all passed");
    return failures != 0;
}
