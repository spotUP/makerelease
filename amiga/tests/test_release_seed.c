#include "release_seed.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %d: %s\n", __LINE__, #c); failures++; } } while (0)

static rp_seed_info info;

static jnode *parse(const char *t) {
    char err[64];
    jnode *j = json_parse(t, err, sizeof err);
    if (!j) printf("parse error: %s\n", err);
    return j;
}

typedef struct { int status; const char *reply; char url[200]; } fake;
static int fake_http(void *ctx, const rp_http_req *r, rp_http_resp *resp) {
    fake *f = ctx;
    snprintf(f->url, sizeof f->url, "%s", r->url);
    if (!f->reply) { snprintf(resp->error, sizeof resp->error, "no connection"); return 1; }
    resp->status = f->status;
    resp->body = strdup(f->reply);
    return 0;
}

int main(void) {
    jnode *seed, *num;
    char msg[200];
    fake f = {200, "{\"exists\":true}", ""};
    rp_rel_cfg cfg = {"https://releases.uprough.net", "k", fake_http, &f};

    CHECK(rp_release_category_index("Demo") == 0 && rp_release_category_index("Other") == 10);
    CHECK(rp_release_category_index("") == -1 && rp_release_category_index("demo") == -1);
    CHECK(rp_release_categories[RP_RELEASE_CATEGORY_COUNT] == NULL && rp_release_categories[10] != NULL);

    seed = parse("{\"nextReleaseNumber\":218,\"currentDizText\":\"DIZ\","
                 "\"members\":[{\"nickname\":\"Spot\",\"role\":\"coder\"},{\"nickname\":\"Hoff\",\"role\":\"music\"},"
                 "{\"nickname\":\"Spot\",\"role\":\"Coder, gfx\"}],"
                 "\"boards\":[{\"name\":\"B1\",\"sysop\":\"S\",\"hq\":\"yes\",\"status\":\"up\"},{\"name\":\"B2\"}],"
                 "\"drafts\":[{\"code\":\"OLD\",\"releaseNumber\":217,\"createdAt\":100}]}");
    num = parse("{\"checked\":true,\"ok\":false,\"summary\":\"Catalogue ends at 216.\"}");
    CHECK(rp_seed_parse(seed, num, 1000000, "up", 1, &info) == 0);
    CHECK(info.next_number == 218 && !strcmp(info.number_text, "218") && !strcmp(info.current_diz, "DIZ"));
    CHECK(info.nmembers == 2 && !strcmp(info.members[0].nickname, "Spot") && !strcmp(info.members[0].role, "coder, gfx"));
    CHECK(info.nboards == 2 && !strcmp(info.boards[0].hq, "yes") && !strcmp(info.boards[1].name, "B2") && !strcmp(info.boards[1].sysop, ""));
    CHECK(!strcmp(info.numbering_warning, "Catalogue ends at 216. Do not build until this is reconciled - the release number goes into the NFO."));
    CHECK(strstr(info.stale_warning, "OLD (release 217, built 11 days ago)"));
    CHECK(info.config_warning[0] == 0);
    CHECK(!strncmp(info.warnings, "Catalogue ends", 14) && strstr(info.warnings, "never revoked") && strchr(info.warnings, ' '));
    json_free(num);

    /* numbering fine, unchecked, or failed: no warning; no key: config warning only */
    num = parse("{\"checked\":true,\"ok\":true,\"summary\":\"fine\"}");
    CHECK(rp_seed_parse(seed, num, 50, "x", 0, &info) == 0);
    CHECK(info.numbering_warning[0] == 0 && info.stale_warning[0] == 0);
    CHECK(strstr(info.config_warning, "The \"x\" profile has no server write key") && !strcmp(info.warnings, info.config_warning));
    json_free(num);
    num = parse("{\"checked\":false}");
    CHECK(rp_seed_parse(seed, num, 50, "x", 1, &info) == 0 && info.warnings[0] == 0);
    json_free(num);
    CHECK(rp_seed_parse(seed, NULL, 50, "x", 1, &info) == 0 && info.warnings[0] == 0);
    json_free(seed);

    seed = parse("{}");
    CHECK(rp_seed_parse(seed, NULL, 50, "x", 1, &info) == 0 && info.next_number == -1 && info.number_text[0] == 0 && info.nmembers == 0);
    json_free(seed);
    seed = parse("[]");
    CHECK(rp_seed_parse(seed, NULL, 50, "x", 1, &info) == -1);
    json_free(seed);

    /* code check */
    CHECK(rp_code_check(&cfg, "HYP", "up-hyp", msg, sizeof msg) == 1);
    CHECK(!strcmp(msg, "up-hyp already exists - building will fail unless you change the code."));
    CHECK(strstr(f.url, "/HYP"));
    f.reply = "{\"exists\":false}";
    CHECK(rp_code_check(&cfg, "NEW", "up-new", msg, sizeof msg) == 0 && msg[0] == 0);
    f.reply = NULL;
    CHECK(rp_code_check(&cfg, "NEW", "up-new", msg, sizeof msg) == -1 && msg[0] != 0);

    /* number preview */
    CHECK(rp_release_number_preview("218", "up-", "hypoc", 5, 1, msg, sizeof msg) == 0 && !strcmp(msg, "release 218 up-hypoc.lha"));
    CHECK(rp_release_number_preview("", "up-", "", 5, 0, msg, sizeof msg) == 0 && !strcmp(msg, "release ? up-xxxxx.zip"));
    CHECK(rp_release_number_preview("1", "up-", "ab", 5, 1, msg, 8) == -1);

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("test_release_seed ok\n");
    return 0;
}
