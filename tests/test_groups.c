#include "groups.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

#define CHECK_STR(a, b) do { \
    if (strcmp((a), (b)) != 0) { printf("FAIL %d: got [%s] expect [%s]\n", __LINE__, (a), (b)); failures++; } \
} while (0)

#define CHECK_SUB(a, b) do { \
    if (strstr((a), (b)) == NULL) { printf("FAIL %d: [%s] lacks [%s]\n", __LINE__, (a), (b)); failures++; } \
} while (0)

static char g_dir[256];

static void write_file(const char *rel, const char *content) {
    char path[512];
    FILE *f;
    snprintf(path, sizeof(path), "%s/%s", g_dir, rel);
    f = fopen(path, "wb");
    if (!f) {
        printf("FAIL %d: cannot write %s\n", __LINE__, path);
        failures++;
        return;
    }
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}

static const char *UPROUGH_JSON =
    "{\"label\":\"Up Rough\",\"url\":\"https://uprough.net\",\"filePrefix\":\"UP-\","
    "\"codeLength\":5,\"renderer\":\"uprough\",\"backend\":\"releases-doorserver\","
    "\"server\":{\"baseUrl\":\"https://releases.uprough.net/\",\"writeKey\":\"s3cret\"},"
    "\"members\":[{\"nickname\":\"spot\",\"role\":\"code\"}]}";

static void setup(void) {
    char *t = getenv("TMPDIR");
    snprintf(g_dir, sizeof(g_dir), "%s/rp-groups-XXXXXX", t ? t : "/tmp");
    if (!mkdtemp(g_dir)) {
        printf("FAIL: mkdtemp\n");
        exit(1);
    }
}

static void make_group(const char *id) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", g_dir, id);
    mkdir(path, 0755);
}

static void test_load_uprough(void) {
    rp_group g;
    char err[256];
    make_group("uprough");
    write_file("uprough/profile.json", UPROUGH_JSON);
    CHECK(rp_load_group(g_dir, "uprough", &g, err, sizeof(err)) == 0);
    CHECK_STR(g.label, "Up Rough");
    CHECK_STR(g.file_prefix, "UP-");
    CHECK(g.code_length == 5);
    CHECK(g.renderer == R_UPROUGH);
    CHECK(g.backend_doorserver == 1);
    CHECK_STR(g.server_base, "https://releases.uprough.net");
    CHECK_STR(g.server_key, "s3cret");
    CHECK(g.nmembers == 1);
    CHECK_STR(g.members[0].nickname, "spot");
}

static void test_load_placeholder(void) {
    rp_group g;
    char err[256];
    make_group("demo");
    write_file("demo/profile.json",
               "{\"label\":\"Demo\",\"url\":\"https://demo.net\",\"renderer\":\"placeholder\",\"filePrefix\":\"DM-\"}");
    CHECK(rp_load_group(g_dir, "demo", &g, err, sizeof(err)) == 0);
    CHECK(g.renderer == R_PLACEHOLDER);
    CHECK(g.code_length == 5);
    CHECK(g.backend_doorserver == 0);
    CHECK(g.nmembers == 0);
}

static void test_rejects(void) {
    rp_group g;
    char err[256];
    make_group("badjson");
    write_file("badjson/profile.json", "{ nope");
    CHECK(rp_load_group(g_dir, "badjson", &g, err, sizeof(err)) != 0);
    make_group("nolabel");
    write_file("nolabel/profile.json", "{\"url\":\"https://x.net\",\"renderer\":\"placeholder\"}");
    CHECK(rp_load_group(g_dir, "nolabel", &g, err, sizeof(err)) != 0);
    CHECK_SUB(err, "nolabel/profile.json: \"label\" is missing");
    make_group("badrenderer");
    write_file("badrenderer/profile.json", "{\"label\":\"x\",\"url\":\"https://x.net\",\"renderer\":\"fancy\"}");
    CHECK(rp_load_group(g_dir, "badrenderer", &g, err, sizeof(err)) != 0);
    make_group("doorplace");
    write_file("doorplace/profile.json",
               "{\"label\":\"x\",\"url\":\"https://x.net\",\"renderer\":\"placeholder\",\"backend\":\"releases-doorserver\"}");
    CHECK(rp_load_group(g_dir, "doorplace", &g, err, sizeof(err)) != 0);
    make_group("longnick");
    write_file("longnick/profile.json",
               "{\"label\":\"x\",\"url\":\"https://x.net\",\"renderer\":\"placeholder\","
               "\"members\":[{\"nickname\":\"this_nickname_is_way_too_long_for_amiga\",\"role\":\"code\"}]}");
    CHECK(rp_load_group(g_dir, "longnick", &g, err, sizeof(err)) != 0);
    CHECK(rp_load_group(g_dir, "../evil", &g, err, sizeof(err)) != 0);
}

static void test_list_and_templates(void) {
    char ids[8][RP_ID_MAX + 1];
    char err[256];
    char *diz = NULL;
    int n = rp_list_groups(g_dir, ids, 8, err, sizeof(err));
    CHECK(n == 7);
    CHECK_STR(ids[0], "badjson");
    CHECK_STR(ids[6], "uprough");
    write_file("demo/file_id.diz", "hi\r\n");
    CHECK(rp_read_template("no/such/dir", 1, &diz, err, sizeof(err)) == 0);
    CHECK(diz == NULL);
    {
        char path[512];
        snprintf(path, sizeof(path), "%s/demo/file_id.diz", g_dir);
        CHECK(rp_read_template(path, 0, &diz, err, sizeof(err)) == 0);
        CHECK_STR(diz, "hi\n");
        free(diz);
    }
    CHECK(rp_list_groups("/nonexistent-dir-xyz", ids, 8, err, sizeof(err)) == -1);
}

int main(void) {
    setup();
    test_load_uprough();
    test_load_placeholder();
    test_rejects();
    test_list_and_templates();
    if (failures == 0) puts("groups: all passed");
    return failures != 0;
}
