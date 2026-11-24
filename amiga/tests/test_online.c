#include "online.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %d: %s\n", __LINE__, #c); failures++; } } while (0)
#define CHECK_STR(a, b) do { if (strcmp((a), (b)) != 0) { printf("FAIL %d: got [%s] expect [%s]\n", __LINE__, (a), (b)); failures++; } } while (0)

/* Scripted transport: the reply is chosen by method, calls are logged. */
static char log_[2048];
static char create_body[1024];
static int create_key_seen;
static int reply_status[4];
static const char *reply_body[4];
static int ncalls;

static int fake_http(void *ctx, const rp_http_req *r, rp_http_resp *resp) {
    size_t i;
    int k = ncalls++ < 3 ? ncalls - 1 : 3;
    (void)ctx;
    snprintf(log_ + strlen(log_), sizeof log_ - strlen(log_), "%s %s;", r->method, r->url);
    if (!strcmp(r->method, "POST") && strstr(r->url, "/api/releases") && !strstr(r->url, "/publish")) {
        snprintf(create_body, sizeof create_body, "%.*s", (int)r->body_len, r->body);
        for (i = 0; i < r->nheaders; i++) if (!strcmp(r->headers[i].name, "X-Release-Key") && !strcmp(r->headers[i].value, "sekrit")) create_key_seen = 1;
    }
    resp->status = reply_status[k];
    resp->body = strdup(reply_body[k]);
    return 0;
}

static void reset(void) { memset(log_, 0, sizeof log_); ncalls = 0; create_key_seen = 0; create_body[0] = 0; }

static void writef(const char *p, const char *s) { FILE *f = fopen(p, "wb"); fputs(s, f); fclose(f); }

int main(void) {
    char out[64], msg[512];
    rp_group g;
    rp_online_publish_args a;

    CHECK(rp_latin1_to_utf8("a\xE9\xFF", out, sizeof out) == 0);
    CHECK_STR(out, "a\xC3\xA9\xC3\xBF");
    CHECK(rp_latin1_to_utf8("\xE9\xE9", out, 4) == -1);

    memset(&g, 0, sizeof g);
    strcpy(g.id, "uprough"); strcpy(g.label, "Up Rough");
    g.renderer = R_UPROUGH; g.backend_doorserver = 1;
    strcpy(g.server_base, "https://rel.test"); strcpy(g.server_key, "sekrit");
    strcpy(g.members[0].nickname, "R\xE9my"); strcpy(g.members[0].role, "Code");
    g.nmembers = 1;

    writef("online_t.lha", "ARCHIVE"); writef("online_t.png", "PNG"); writef("online_t.diz", "\n  line  \n");
    memset(&a, 0, sizeof a);
    a.group = &g; a.code = "ABC"; a.release_name = "Caf\xE9"; a.platform_tag = "A500"; a.description = "d";
    a.format = "LHA"; a.diz_path = "online_t.diz"; a.archive_path = "online_t.lha"; a.image_path = "online_t.png";
    a.seed = 1; a.http = fake_http;

    {
        rp_credit cr[1] = {{ "Code", "R\xE9my" }};
        a.credits = cr; a.n_credits = 1; a.creators = "R\xE9my";
        reply_status[0] = 201; reply_body[0] = "{\"releaseNumber\":5}";
        reply_status[1] = 200; reply_body[1] = "{\"wordpressPostUrl\":\"https://wp/p\",\"archiveUrl\":\"https://a/x.lha\"}";
        reset();
        CHECK(rp_online_publish(&a, msg, sizeof msg) == 0);
        CHECK(strstr(create_body, "\"credits\":[{\"role\":\"Code\",\"name\":\"R\xC3\xA9my\"}]") != NULL);
        CHECK(strstr(create_body, "\"entryCreators\":\"R\xC3\xA9my\"") != NULL);
        a.credits = NULL; a.n_credits = 0; a.creators = NULL;
    }
    reply_status[0] = 201; reply_body[0] = "{\"releaseNumber\":5}";
    reply_status[1] = 200; reply_body[1] = "{\"wordpressPostUrl\":\"https://wp/p\",\"archiveUrl\":\"https://a/x.lha\"}";
    reset();
    CHECK(rp_online_publish(&a, msg, sizeof msg) == 0);
    CHECK_STR(msg, "Published: https://wp/p (archive: https://a/x.lha)");
    CHECK(strstr(log_, "POST https://rel.test/api/releases;POST https://rel.test/api/releases/ABC/publish;") != NULL);
    CHECK(create_key_seen);
    CHECK(strstr(create_body, "Caf\xC3\xA9") != NULL);
    CHECK(strstr(create_body, "R\xC3\xA9my") != NULL);
    CHECK(strstr(create_body, "\"division\":\"uprough\"") != NULL);

    /* create refused: publish never sent */
    reply_status[0] = 409; reply_body[0] = "{\"error\":\"code taken\"}";
    reset();
    CHECK(rp_online_publish(&a, msg, sizeof msg) == -1);
    CHECK_STR(msg, "Not published - code taken");
    CHECK(strstr(log_, "/publish") == NULL);

    /* no cover image, no key */
    a.image_path = "";
    CHECK(rp_online_publish(&a, msg, sizeof msg) == -1);
    CHECK(strstr(msg, "cover image") != NULL);
    a.image_path = "online_t.png"; g.server_key[0] = 0;
    CHECK(rp_online_publish(&a, msg, sizeof msg) == -1);
    CHECK(strstr(msg, "write key") != NULL);
    strcpy(g.server_key, "sekrit");

    /* revoke */
    reply_status[0] = 200; reply_body[0] = "{\"deleted\":true,\"releaseNumber\":5,\"wasDraft\":false,\"wordpressPostUrl\":\"https://wp/p\",\"wordpressPostError\":\"boom\",\"archiveRemoved\":false}";
    reset();
    CHECK(rp_online_revoke(&g, "ABC", fake_http, NULL, msg, sizeof msg) == 0);
    CHECK_STR(msg, "Revoked release 5. Its WordPress post could NOT be trashed - remove it by hand: https://wp/p Its archive could not be removed - check the server logs.");
    CHECK(strstr(log_, "DELETE https://rel.test/api/releases/ABC;") != NULL);
    reply_body[0] = "{\"deleted\":true,\"releaseNumber\":6,\"wasDraft\":true}";
    reset();
    CHECK(rp_online_revoke(&g, "ABC", fake_http, NULL, msg, sizeof msg) == 0);
    CHECK(strstr(msg, "never published") != NULL);
    CHECK(rp_online_revoke(&g, "", fake_http, NULL, msg, sizeof msg) == -1);

    /* door repository: /X only */
    CHECK(rp_online_door(&g, "online_t.lha", 1, fake_http, NULL, msg, sizeof msg) == -1);
    g.renderer = R_XINNOVATIONS;
    reply_status[0] = 201; reply_body[0] = "{\"message\":\"Thanks\",\"id\":3}";
    reset();
    CHECK(rp_online_door(&g, "online_t.lha", 1, fake_http, NULL, msg, sizeof msg) == 0);
    CHECK_STR(msg, "Door repository: Thanks");

    remove("online_t.lha"); remove("online_t.png"); remove("online_t.diz");
    if (failures == 0) puts("online: all passed");
    return failures != 0;
}
