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
static char publish_body[2048];
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
    if (!strcmp(r->method, "POST") && strstr(r->url, "/publish"))
        snprintf(publish_body, sizeof publish_body, "%.*s", (int)r->body_len, r->body);
    resp->status = reply_status[k];
    resp->body = strdup(reply_body[k]);
    return 0;
}

static void reset(void) { memset(log_, 0, sizeof log_); ncalls = 0; create_key_seen = 0; create_body[0] = 0; publish_body[0] = 0; }

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
    a.seed = 1; a.http = fake_http; a.category = "Demo";

    {
        rp_credit cr[1] = {{ "Code", "R\xE9my" }};
        a.credits = cr; a.n_credits = 1; a.creators = "R\xE9my"; a.news = "line one\nline two";
        reply_status[0] = 201; reply_body[0] = "{\"releaseNumber\":5}";
        reply_status[1] = 200; reply_body[1] = "{\"wordpressPostUrl\":\"https://wp/p\",\"archiveUrl\":\"https://a/x.lha\"}";
        reset();
        CHECK(rp_online_publish(&a, msg, sizeof msg) == 0);
        CHECK(strstr(create_body, "\"credits\":[{\"role\":\"Code\",\"name\":\"R\xC3\xA9my\"}]") != NULL);
        CHECK(strstr(create_body, "\"entryCreators\":\"R\xC3\xA9my\"") != NULL);
        CHECK(strstr(create_body, "\"news\":\"line one\\nline two\"") != NULL);
        a.credits = NULL; a.n_credits = 0; a.creators = NULL; a.news = NULL;
    }
    /* Roster tab rows: members replace the group's, boards and embeds reach the body, the image embed is uploaded */
    {
        rp_member mem[1];
        rp_board_row brd[1];
        rp_embed_row emb[2];
        memset(mem, 0, sizeof mem); memset(brd, 0, sizeof brd); memset(emb, 0, sizeof emb);
        strcpy(mem[0].nickname, "Zed"); strcpy(mem[0].role, "Music");
        strcpy(brd[0].name, "Caf\xE9 BBS"); strcpy(brd[0].sysop, "Sys"); strcpy(brd[0].hq, "Oslo"); strcpy(brd[0].status, "Online");
        strcpy(emb[0].caption, "Intro"); strcpy(emb[0].url, "https://y/t");
        emb[1].is_image = 1; strcpy(emb[1].caption, "Shot"); strcpy(emb[1].path, "online_t_e.png");
        writef("online_t_e.png", "EMBPNG");
        a.members = mem; a.n_members = 1; a.boards = brd; a.n_boards = 1; a.embeds = emb; a.n_embeds = 2;
        reply_status[0] = 201; reply_body[0] = "{\"releaseNumber\":5}";
        reply_status[1] = 200; reply_body[1] = "{\"wordpressPostUrl\":\"https://wp/p\",\"archiveUrl\":\"https://a/x.lha\"}";
        reset();
        CHECK(rp_online_publish(&a, msg, sizeof msg) == 0);
        CHECK(strstr(create_body, "\"members\":[{\"nickname\":\"Zed\",\"role\":\"Music\"}]") != NULL);
        CHECK(strstr(create_body, "R\xC3\xA9my") == NULL);
        CHECK(strstr(create_body, "\"boards\":[{\"name\":\"Caf\xC3\xA9 BBS\"") != NULL);
        CHECK(strstr(create_body, "{\"type\":\"youtube\",\"caption\":\"Intro\",\"youtubeUrl\":\"https://y/t\"}") != NULL);
        CHECK(strstr(create_body, "{\"type\":\"image\",\"caption\":\"Shot\"}") != NULL);
        CHECK(strstr(publish_body, "name=\"embedImage\"") != NULL);
        CHECK(strstr(publish_body, "filename=\"online_t_e.png\"") != NULL);
        CHECK(strstr(publish_body, "EMBPNG") != NULL);
        /* no boards: the key is left out; an unreadable embed image stops before any request */
        a.n_boards = 0;
        reset();
        CHECK(rp_online_publish(&a, msg, sizeof msg) == 0);
        CHECK(strstr(create_body, "\"boards\"") == NULL);
        strcpy(emb[1].path, "online_t_missing.png");
        reset();
        CHECK(rp_online_publish(&a, msg, sizeof msg) == -1);
        CHECK(strstr(msg, "online_t_missing.png") != NULL);
        CHECK(log_[0] == 0);
        remove("online_t_e.png");
        a.members = NULL; a.n_members = 0; a.boards = NULL; a.embeds = NULL; a.n_embeds = 0;
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

    CHECK(strstr(create_body, "\"entryCategory\":\"Demo\"") != NULL);
    /* no category: refused before any request */
    a.category = "";
    reset();
    CHECK(rp_online_publish(&a, msg, sizeof msg) == -1);
    CHECK_STR(msg, "Not published - choose a category first");
    CHECK(log_[0] == 0);
    a.category = "Demo";

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

    /* seed: number comes through, numbering-check failure is only a missing warning */
    {
        static rp_seed_info info;
        jnode *tree = NULL;
        reply_status[0] = 200; reply_body[0] = "{\"nextReleaseNumber\":218,\"currentDizText\":\"DIZ\"}";
        reply_status[1] = 500; reply_body[1] = "{}";
        reset();
        CHECK(rp_online_seed(&g, fake_http, NULL, 1000, &info, &tree, msg, sizeof msg) == 0);
        CHECK_STR(info.number_text, "218");
        CHECK(strstr(log_, "GET https://rel.test/api/releases/seed") != NULL && tree != NULL);
        json_free(tree);
        reply_status[0] = 500; reply_body[0] = "{\"error\":\"down\"}";
        reset();
        CHECK(rp_online_seed(&g, fake_http, NULL, 1000, &info, &tree, msg, sizeof msg) == -1 && msg[0] != 0);
        /* code check: taken, free, and a group with no server */
        reply_status[0] = 200; reply_body[0] = "{\"exists\":true}";
        reset();
        CHECK(rp_online_code_check(&g, "ABC", fake_http, NULL, msg, sizeof msg) == 1);
        CHECK(strstr(msg, "already exists") != NULL);
        reply_body[0] = "{\"exists\":false}";
        reset();
        CHECK(rp_online_code_check(&g, "ABC", fake_http, NULL, msg, sizeof msg) == 0 && msg[0] == 0);
        g.backend_doorserver = 0;
        reset();
        CHECK(rp_online_code_check(&g, "ABC", fake_http, NULL, msg, sizeof msg) == 0 && log_[0] == 0);
        g.backend_doorserver = 1;
    }

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

    /* /X release list: this release (formatted number, final DIZ line), then the manifest history */
    {
        rp_x_entry *xl = NULL;
        size_t nx = 9;
        const char *diz = "  uP rOUGH /X iNNOVATiONS pRESENTS:\n|  My Door at Board  |\n";
        reply_status[0] = 200;
        reply_body[0] = "{\"history\":[{\"releaseNumber\":\"oo6\",\"dizText\":\"x PRESENTS:\\n| Old Door |\"},{\"dizText\":3}]}";
        reset();
        CHECK(rp_online_release_list(&g, fake_http, NULL, 7, diz, &xl, &nx, msg, sizeof msg) == 0);
        CHECK(strstr(log_, "GET https://rel.test/api/releases/manifest?division=xinnovations;") != NULL);
        CHECK(nx == 3);
        if (nx == 3) {
            CHECK_STR(xl[0].number, "oo7");
            CHECK(strstr(xl[0].art, "My Door at Board") != NULL);
            CHECK_STR(xl[1].number, "oo6");
            CHECK(strstr(xl[1].art, "Old Door") != NULL);
            CHECK_STR(xl[2].number, "");
        }
        rp_online_release_list_free(xl, nx);
        reply_status[0] = 500; reply_body[0] = "{\"error\":\"boom\"}";
        reset();
        CHECK(rp_online_release_list(&g, fake_http, NULL, 7, diz, &xl, &nx, msg, sizeof msg) == -1);
        CHECK(xl == NULL && nx == 0 && strstr(msg, "boom") != NULL);
        /* Up Rough has no release list and sends nothing */
        g.renderer = R_UPROUGH;
        reset();
        CHECK(rp_online_release_list(&g, fake_http, NULL, 7, diz, &xl, &nx, msg, sizeof msg) == 0);
        CHECK(nx == 0 && ncalls == 0);
    }

    /* Up Rough NFO history: this release, then the seed's */
    {
        char jerr[64];
        jnode *seed = json_parse("{\"history\":[{\"releaseNumber\":\"216\",\"filenames\":\"up-old.lha\",\"title\":\"Old\"}]}", jerr, sizeof jerr);
        rp_history self = { "217", "up-new.lha", "New", "Demo", "Zed" }, h[4];
        CHECK(rp_seed_history(seed, &self, h, 4) == 2);
        CHECK_STR(h[0].number, "217");
        CHECK_STR(h[1].number, "216");
        CHECK_STR(h[1].filenames, "up-old.lha");
        CHECK_STR(h[1].category, "");
        CHECK(rp_seed_history(seed, &self, h, 1) == 1);
        CHECK(rp_seed_history(NULL, &self, h, 4) == 1);
        json_free(seed);
    }

    remove("online_t.lha"); remove("online_t.png"); remove("online_t.diz");
    if (failures == 0) puts("online: all passed");
    return failures != 0;
}
