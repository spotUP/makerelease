/* JSON writer, publish body, multipart, door repo client, cfg filler.
 * Expected strings come from the TS: JSON.stringify(toPublishBody(form)) and
 * undici FormData serialised through Request (node 24), boundary seed 14997584896. */
#include "releases_client.h"
#include "door_repo_client.h"
#include "multipart.h"

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

typedef struct {
    int rc, status;
    const char *reply, *error;
    char *body; size_t body_len;
    char method[16], url[200], ctype[128], key[64];
} fake;

static int fake_http(void *ctx, const rp_http_req *r, rp_http_resp *resp) {
    fake *f = ctx;
    size_t i;
    free(f->body);
    f->body = malloc(r->body_len + 1);
    memcpy(f->body, r->body, r->body_len);
    f->body[r->body_len] = '\0';
    f->body_len = r->body_len;
    snprintf(f->method, sizeof f->method, "%s", r->method);
    snprintf(f->url, sizeof f->url, "%s", r->url);
    f->ctype[0] = f->key[0] = '\0';
    for (i = 0; i < r->nheaders; i++) {
        if (!strcmp(r->headers[i].name, "Content-Type")) snprintf(f->ctype, sizeof f->ctype, "%s", r->headers[i].value);
        if (!strcmp(r->headers[i].name, "X-Release-Key")) snprintf(f->key, sizeof f->key, "%s", r->headers[i].value);
    }
    if (f->rc) { snprintf(resp->error, sizeof resp->error, "%s", f->error); return f->rc; }
    resp->status = f->status;
    resp->body = f->reply ? strdup(f->reply) : NULL;
    return 0;
}

#define BND "----formdata-undici-014997584896"
#define CT "multipart/form-data; boundary=" BND
#define HDR(n, fn) "--" BND "\r\nContent-Disposition: form-data; name=\"" n "\"" fn

int main(void) {
    static fake F;
    char *s;
    char b[80];
    rp_rel_cfg cfg;
    rp_rel_err e;
    jnode *j;

    /* trimDizBanner */
    s = rp_trim_diz("\n  ART  \n\n b\t\n\n"); CHECK_STR(s, "  ART\n\n b"); free(s);
    s = rp_trim_diz("\n \n"); CHECK_STR(s, ""); free(s);

    /* toPublishBody, all keys */
    {
        rp_rel_board bo[] = {{"B", "S", "Y", "ok"}};
        rp_rel_member me[] = {{"a", "coder"}, {"b", "gfx"}};
        rp_rel_credit cr[] = {{"r", "n"}};
        rp_rel_embed em[] = {{"youtube", "c", "http://y"}, {"image", "i", NULL}};
        rp_rel_form f = { "UR-007", "LHA", "Say \"hi\"\\ there", "OS3", "line1\nline2\ttab\x01", "n",
            "\n  ART  \n\n b\t\n\n", "T", "C", "me", "up-rough", bo, 1, me, 2, "d\r\n", cr, 1, em, 2 };
        s = rp_rel_publish_body(&f);
        CHECK_STR(s, "{\"code\":\"UR-007\",\"format\":\"LHA\",\"releaseName\":\"Say \\\"hi\\\"\\\\ there\",\"platformTag\":\"OS3\","
            "\"releaseNotes\":\"line1\\nline2\\ttab\\u0001\",\"news\":\"n\",\"dizText\":\"  ART\\n\\n b\",\"entryTitle\":\"T\","
            "\"entryCategory\":\"C\",\"entryCreators\":\"me\",\"division\":\"up-rough\","
            "\"boards\":[{\"name\":\"B\",\"sysop\":\"S\",\"hq\":\"Y\",\"status\":\"ok\"}],"
            "\"members\":[{\"nickname\":\"a\",\"role\":\"coder\"},{\"nickname\":\"b\",\"role\":\"gfx\"}],\"description\":\"d\\r\\n\","
            "\"credits\":[{\"role\":\"r\",\"name\":\"n\"}],"
            "\"mediaEmbeds\":[{\"type\":\"youtube\",\"caption\":\"c\",\"youtubeUrl\":\"http://y\"},{\"type\":\"image\",\"caption\":\"i\"}]}");
        free(s);
        /* optional keys omitted, empty arrays; UTF-8 passes through */
        f.division = NULL; f.boards = NULL; f.n_members = f.n_credits = f.n_embeds = 0; f.release_name = "caf\xc3\xa9";
        s = rp_rel_publish_body(&f);
        CHECK_STR(s, "{\"code\":\"UR-007\",\"format\":\"LHA\",\"releaseName\":\"caf\xc3\xa9\",\"platformTag\":\"OS3\","
            "\"releaseNotes\":\"line1\\nline2\\ttab\\u0001\",\"news\":\"n\",\"dizText\":\"  ART\\n\\n b\",\"entryTitle\":\"T\","
            "\"entryCategory\":\"C\",\"entryCreators\":\"me\",\"members\":[],\"description\":\"d\\r\\n\",\"credits\":[],\"mediaEmbeds\":[]}");
        /* and it goes out through rp_rel_create with the JSON headers */
        memset(&F, 0, sizeof F); F.status = 200; F.reply = "{\"code\":\"UR-007\"}";
        cfg.base_url = "https://releases.uprough.net"; cfg.write_key = "k"; cfg.http = fake_http; cfg.http_ctx = &F;
        CHECK_INT(rp_rel_create(&cfg, s, &j, &e), 0);
        CHECK_STR(F.body, s); CHECK_STR(F.ctype, "application/json"); CHECK_STR(F.key, "k");
        json_free(j); free(s);
    }

    /* multipart: publishToWeb parts, byte-exact vs undici */
    {
        rp_rel_file ar = { "a\"b.lha", "AAA\r\nB", 6 }, im = { "p.png", "IMG", 3 }, em = { "e1.png", "E1", 2 };
        rp_mp_boundary(b, sizeof b, 14997584896UL); CHECK_STR(b, BND);
        memset(&F, 0, sizeof F); F.status = 200; F.reply = "{\"ok\":true}";
        CHECK_INT(rp_rel_publish_files(&cfg, "UR-007", 14997584896UL, &ar, &im, &em, 1, &j, &e), 0);
        json_free(j);
        CHECK_STR(F.url, "https://releases.uprough.net/api/releases/UR-007/publish");
        CHECK_STR(F.ctype, CT); CHECK_STR(F.key, "k");
        CHECK_STR(F.body,
            HDR("archive", "; filename=\"a%22b.lha\"") "\r\nContent-Type: application/octet-stream\r\n\r\nAAA\r\nB\r\n"
            HDR("image", "; filename=\"p.png\"") "\r\nContent-Type: application/octet-stream\r\n\r\nIMG\r\n"
            HDR("embedImage", "; filename=\"e1.png\"") "\r\nContent-Type: application/octet-stream\r\n\r\nE1\r\n"
            "--" BND "--\r\n");
    }
    {   /* text field, as undici writes it */
        rp_mp mp; char ct[128];
        rp_mp_init(&mp, BND); rp_mp_field(&mp, "note", "hello"); rp_mp_finish(&mp, ct, sizeof ct);
        CHECK_STR(mp.body.data, HDR("note", "") "\r\n\r\nhello\r\n--" BND "--\r\n");
        free(mp.body.data);
    }

    /* door repo submit */
    memset(&F, 0, sizeof F); F.status = 200; F.reply = "{\"message\":\"queued\",\"id\":42}";
    {
        rp_door_result r;
        CHECK_INT(rp_door_submit(fake_http, &F, NULL, "door.lha", "DATA", 4, 14997584896UL, &r), 0);
        CHECK_STR(F.method, "POST");
        CHECK_STR(F.url, "https://doors.uprough.net/api/door-repo/submissions");
        CHECK_STR(F.ctype, CT); CHECK_STR(F.key, "");
        CHECK_STR(F.body, HDR("file", "; filename=\"door.lha\"") "\r\nContent-Type: application/octet-stream\r\n\r\nDATA\r\n--" BND "--\r\n");
        CHECK_INT(r.ok, 1); CHECK_STR(r.message, "queued"); CHECK_INT(r.id, 42);

        F.reply = "{}";
        CHECK_INT(rp_door_submit(fake_http, &F, NULL, "d.lha", "x", 1, 1, &r), 0);
        CHECK_STR(r.message, "submitted to the door repository"); CHECK_INT(r.id, -1);

        F.status = 409; F.reply = "{\"error\":\"already in the queue\"}";
        CHECK_INT(rp_door_submit(fake_http, &F, NULL, "d.lha", "x", 1, 1, &r), -1);
        CHECK_INT(r.ok, 0); CHECK_STR(r.message, "already in the queue");

        F.status = 500; F.reply = NULL;
        CHECK_INT(rp_door_submit(fake_http, &F, "http://local/x", "d.lha", "x", 1, 1, &r), -1);
        CHECK_STR(r.message, "the door repository returned 500"); CHECK_STR(F.url, "http://local/x");

        F.rc = 1; F.error = "no route";
        CHECK_INT(rp_door_submit(fake_http, &F, NULL, "d.lha", "x", 1, 1, &r), -1);
        CHECK_STR(r.message, "could not reach the door repository: no route");
    }

    /* clientConfigFor: env wins when non-empty, no server block fails */
    CHECK_INT(rp_rel_cfg_for(&cfg, NULL, NULL, NULL, NULL, fake_http, &F), -1);
    CHECK_INT(rp_rel_cfg_for(&cfg, "https://p", "pk", NULL, "", fake_http, &F), 0);
    CHECK_STR(cfg.base_url, "https://p"); CHECK_STR(cfg.write_key, "pk");
    CHECK_INT(rp_rel_cfg_for(&cfg, "https://p", "pk", "http://localhost:3000", "ek", fake_http, &F), 0);
    CHECK_STR(cfg.base_url, "http://localhost:3000"); CHECK_STR(cfg.write_key, "ek");
    CHECK_INT(rp_rel_cfg_for(&cfg, "https://p", "", NULL, NULL, fake_http, &F), 0);
    CHECK_STR(cfg.write_key, "");

    free(F.body);
    if (failures == 0) puts("online_writers: all passed");
    return failures != 0;
}
