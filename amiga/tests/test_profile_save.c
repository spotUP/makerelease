#include "profile_save.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "stage.h"

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

#define CHECK_STR(a, b) do { \
    if (strcmp((a), (b)) != 0) { printf("FAIL %d: got [%s] expect [%s]\n", __LINE__, (a), (b)); failures++; } \
} while (0)

static char g_dir[256];
static char err[512];

/* Generated with the desktop serializeProfile (JSON.stringify(body, null, 2) + "\n"). */
static const char *FULL_JSON =
    "{\n  \"label\": \"Up Rough\",\n  \"url\": \"https://uprough.net\",\n  \"filePrefix\": \"UP-\",\n"
    "  \"codeLength\": 5,\n  \"renderer\": \"uprough\",\n  \"backend\": \"releases-doorserver\",\n"
    "  \"server\": {\n    \"baseUrl\": \"https://releases.uprough.net\",\n    \"writeKey\": \"s3\\\"cret\"\n  },\n"
    "  \"members\": [\n    {\n      \"nickname\": \"spot\",\n      \"role\": \"code\"\n    },\n"
    "    {\n      \"nickname\": \"two\",\n      \"role\": \"gfx\"\n    }\n  ]\n}\n";
static const char *DEMO_JSON =
    "{\n  \"label\": \"Demo\",\n  \"url\": \"x\",\n  \"filePrefix\": \"\",\n  \"codeLength\": 5,\n"
    "  \"renderer\": \"placeholder\",\n  \"backend\": \"none\",\n  \"members\": []\n}\n";

static void setup(void) {
    char *t = getenv("TMPDIR");
    snprintf(g_dir, sizeof(g_dir), "%s/rp-psave-XXXXXX", t ? t : "/tmp");
    if (!mkdtemp(g_dir)) {
        printf("FAIL: mkdtemp\n");
        exit(1);
    }
}

static void path_of(char *out, size_t cap, const char *rel) { snprintf(out, cap, "%s/%s", g_dir, rel); }

static int exists(const char *rel) {
    char p[512];
    struct stat st;
    path_of(p, sizeof p, rel);
    return stat(p, &st) == 0;
}

static char *slurp(const char *rel) {
    char p[512], *buf;
    FILE *f;
    long n;
    path_of(p, sizeof p, rel);
    f = fopen(p, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc((size_t)n + 1);
    buf[fread(buf, 1, (size_t)n, f)] = '\0';
    fclose(f);
    return buf;
}

static void fill_full(rp_group *g, const char *id) {
    memset(g, 0, sizeof *g);
    strcpy(g->id, id);
    strcpy(g->label, "Up Rough");
    strcpy(g->url, "https://uprough.net");
    strcpy(g->file_prefix, "UP-");
    g->code_length = 5;
    g->renderer = R_UPROUGH;
    g->backend_doorserver = 1;
    strcpy(g->server_base, "https://releases.uprough.net");
    strcpy(g->server_key, "s3\"cret");
    strcpy(g->members[0].nickname, "spot");
    strcpy(g->members[0].role, "code");
    strcpy(g->members[1].nickname, "two");
    strcpy(g->members[1].role, "gfx");
    g->nmembers = 2;
}

static void fill_demo(rp_group *g, const char *id) {
    memset(g, 0, sizeof *g);
    strcpy(g->id, id);
    strcpy(g->label, "Demo");
    strcpy(g->url, "x");
    g->code_length = 5;
    g->renderer = R_PLACEHOLDER;
}

static void test_serialize_and_roundtrip(void) {
    rp_group g, back;
    char out[4096];
    CHECK(rp_group_save(g_dir, (fill_demo(&g, "demo"), &g), 1, NULL, NULL, 0, err, sizeof err) == 0);
    fill_full(&g, "uprough");
    CHECK(rp_group_serialize(&g, out, sizeof out, err, sizeof err) == 0);
    CHECK_STR(out, FULL_JSON);
    fill_demo(&g, "demo");
    CHECK(rp_group_serialize(&g, out, sizeof out, err, sizeof err) == 0);
    CHECK_STR(out, DEMO_JSON);
    CHECK(rp_group_serialize(&g, out, 20, err, sizeof err) == -1);

    fill_full(&g, "uprough");
    CHECK(rp_group_save(g_dir, &g, 1, "diz text\n", "nfo \xe9\n", 0, err, sizeof err) == 0);
    CHECK(rp_load_group(g_dir, "uprough", &back, err, sizeof err) == 0);
    CHECK_STR(back.label, "Up Rough");
    CHECK_STR(back.server_key, "s3\"cret");
    CHECK(back.nmembers == 2 && back.backend_doorserver && back.renderer == R_UPROUGH);
    {
        char *t = slurp("uprough/profile.json");
        CHECK(t && strcmp(t, FULL_JSON) == 0);
        free(t);
        t = slurp("uprough/release.nfo");
        CHECK(t && strcmp(t, "nfo \xe9\n") == 0);
        free(t);
        t = slurp("uprough/file_id.diz");
        CHECK(t && strcmp(t, "diz text\n") == 0);
        free(t);
    }
    CHECK(!exists("uprough/profile.json.tmp"));
}

static void test_refusals(void) {
    rp_group g;
    fill_full(&g, "Bad_Id");
    CHECK(rp_group_save(g_dir, &g, 1, NULL, NULL, 0, err, sizeof err) == -1);
    CHECK(strstr(err, "must use only a-z, 0-9 and \"-\"") != NULL);
    CHECK(!exists("Bad_Id"));

    fill_full(&g, "uprough");
    CHECK(rp_group_save(g_dir, &g, 1, NULL, NULL, 0, err, sizeof err) == -1);
    CHECK_STR(err, "A group with id uprough already exists");
    fill_full(&g, "ghost");
    CHECK(rp_group_save(g_dir, &g, 0, NULL, NULL, 0, err, sizeof err) == -1);
    CHECK_STR(err, "No group with id ghost exists");
    CHECK(!exists("ghost"));
}

static void test_validation_leaves_folder_untouched(void) {
    rp_group g;
    char *before, *after;

    /* Edit: placeholder renderer with a doorserver backend is refused by the parser. */
    before = slurp("uprough/profile.json");
    fill_full(&g, "uprough");
    g.renderer = R_PLACEHOLDER;
    strcpy(g.label, "Changed");
    CHECK(rp_group_save(g_dir, &g, 0, "NEW DIZ", NULL, 1, err, sizeof err) == -1);
    CHECK(strstr(err, "\"backend\" can be \"releases-doorserver\" only with renderer") != NULL);
    after = slurp("uprough/profile.json");
    CHECK_STR(before, after);
    free(before);
    free(after);
    after = slurp("uprough/file_id.diz");
    CHECK_STR(after, "diz text\n");
    free(after);
    CHECK(exists("uprough/release.nfo"));

    /* New: a refused save creates no drawer. */
    fill_demo(&g, "fresh");
    g.label[0] = '\0';
    CHECK(rp_group_save(g_dir, &g, 1, "d", "n", 0, err, sizeof err) == -1);
    CHECK(strstr(err, "\"label\" must not be empty") != NULL);
    CHECK(!exists("fresh"));
    fill_demo(&g, "fresh");
    strcpy(g.file_prefix, "a b");
    CHECK(rp_group_save(g_dir, &g, 1, NULL, NULL, 0, err, sizeof err) == -1);
    CHECK(!exists("fresh"));
    fill_demo(&g, "fresh");
    g.code_length = 40;
    CHECK(rp_group_save(g_dir, &g, 1, NULL, NULL, 0, err, sizeof err) == -1);
    CHECK(strstr(err, "\"codeLength\" must be a whole number from 1 to 32") != NULL);
    CHECK(!exists("fresh"));
}

static void test_overlong_field(void) {
    rp_group g;
    char out[4096];
    fill_demo(&g, "demo");
    memset(g.label, 'a', sizeof g.label); /* no terminator: a caller that copied too much */
    CHECK(rp_group_serialize(&g, out, sizeof out, err, sizeof err) == -1);
    CHECK_STR(err, "\"label\" is too long (max 63)");
    CHECK(rp_group_save(g_dir, &g, 0, NULL, NULL, 0, err, sizeof err) == -1);
    fill_demo(&g, "demo");
    memset(g.members[0].role, 'b', sizeof g.members[0].role);
    g.nmembers = 1;
    CHECK(rp_group_serialize(&g, out, sizeof out, err, sizeof err) == -1);
    CHECK_STR(err, "\"members[0].role\" is too long (max 31)");
}

static void test_edit_template_removal(void) {
    rp_group g;
    fill_full(&g, "uprough");
    strcpy(g.label, "Up Rough 2");
    CHECK(rp_group_save(g_dir, &g, 0, NULL, NULL, 1, err, sizeof err) == 0);
    CHECK(!exists("uprough/release.nfo"));
    CHECK(exists("uprough/file_id.diz"));
    {
        rp_group back;
        CHECK(rp_load_group(g_dir, "uprough", &back, err, sizeof err) == 0);
        CHECK_STR(back.label, "Up Rough 2");
    }
}

static void test_duplicate_and_delete(void) {
    char *t;
    CHECK(rp_group_duplicate(g_dir, "uprough", "uprough-copy", err, sizeof err) == 0);
    CHECK(exists("uprough-copy/profile.json"));
    t = slurp("uprough-copy/file_id.diz");
    CHECK(t && strcmp(t, "diz text\n") == 0);
    free(t);
    CHECK(rp_group_duplicate(g_dir, "uprough", "uprough-copy", err, sizeof err) == -1);
    CHECK_STR(err, "A group with id uprough-copy already exists");
    CHECK(rp_group_duplicate(g_dir, "nope", "x", err, sizeof err) == -1);
    CHECK(rp_group_duplicate(g_dir, "uprough", "Bad Id", err, sizeof err) == -1);

    CHECK(rp_group_delete(g_dir, "Bad Id", err, sizeof err) == -1);
    CHECK(rp_group_delete(g_dir, "ghost", err, sizeof err) == -1);
    CHECK_STR(err, "No group with id ghost exists");
    CHECK(rp_group_delete(g_dir, "uprough-copy", err, sizeof err) == 0);
    CHECK(!exists("uprough-copy"));
    CHECK(rp_group_delete(g_dir, "demo", err, sizeof err) == 0);
    CHECK(rp_group_delete(g_dir, "uprough", err, sizeof err) == -1);
    CHECK_STR(err, "The last group cannot be deleted");
    CHECK(exists("uprough/profile.json"));
}

int main(void) {
    setup();
    test_serialize_and_roundtrip();
    test_refusals();
    test_validation_leaves_folder_untouched();
    test_overlong_field();
    test_edit_template_removal();
    test_duplicate_and_delete();
    {
        char ignore[64];
        rp_remove_tree(g_dir, ignore, sizeof ignore);
    }
    if (failures == 0) puts("profile_save: all passed");
    return failures != 0;
}
