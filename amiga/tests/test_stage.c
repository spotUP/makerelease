#include "archive.h"

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

static void setup(void) {
    char *t = getenv("TMPDIR");
    snprintf(g_dir, sizeof(g_dir), "%s/rp-stage-XXXXXX", t ? t : "/tmp");
    if (!mkdtemp(g_dir)) {
        printf("FAIL: mkdtemp\n");
        exit(1);
    }
}

static void test_cmd(void) {
    char *c = rp_archive_cmd("lha", "RAM:UP-X.LHA", "UP-X");
    CHECK_STR(c, "\"lha\" -r a \"RAM:UP-X.LHA\" \"UP-X\"");
    free(c);
    c = rp_archive_cmd("zip", "RAM:UP-X.ZIP", "UP-X");
    CHECK_STR(c, "\"zip\" -r \"RAM:UP-X.ZIP\" \"UP-X\"");
    free(c);
}

static void test_files(void) {
    char err[256], p[512], q[512];
    FILE *f;
    snprintf(p, sizeof(p), "%s/a/b/c", g_dir);
    CHECK(rp_make_dirs(p, err, sizeof(err)) == 0);
    CHECK(rp_make_dirs(p, err, sizeof(err)) == 0);
    snprintf(p, sizeof(p), "%s/src/sub", g_dir);
    CHECK(rp_make_dirs(p, err, sizeof(err)) == 0);
    snprintf(p, sizeof(p), "%s/src/top.txt", g_dir);
    CHECK(rp_write_file(p, "top", 3, err, sizeof(err)) == 0);
    snprintf(p, sizeof(p), "%s/src/sub/nested.txt", g_dir);
    CHECK(rp_write_file(p, "nested", 6, err, sizeof(err)) == 0);
    snprintf(p, sizeof(p), "%s/src", g_dir);
    snprintf(q, sizeof(q), "%s/dst", g_dir);
    CHECK(rp_copy_tree(p, q, err, sizeof(err)) == 0);
    snprintf(p, sizeof(p), "%s/dst/sub/nested.txt", g_dir);
    f = fopen(p, "rb");
    CHECK(f != NULL);
    if (f) fclose(f);
    snprintf(p, sizeof(p), "%s/dst2", g_dir);
    CHECK(rp_move_dir(q, p, err, sizeof(err)) == 0);
    snprintf(q, sizeof(q), "%s/dst", g_dir);
    CHECK(access(q, F_OK) != 0);
    CHECK(rp_remove_tree(p, err, sizeof(err)) == 0);
    CHECK(rp_remove_tree(p, err, sizeof(err)) == 0);
    snprintf(p, sizeof(p), "%s/nope/nested.txt", g_dir);
    CHECK(rp_write_file(p, "x", 1, err, sizeof(err)) != 0);
}

/* dst parent is missing so rename() fails and the copy fallback runs; an
 * unreadable file makes the copy fail partway. */
static void test_move_fail(void) {
    char err[256], p[512], q[512];
    snprintf(p, sizeof(p), "%s/mf/ok.txt", g_dir);
    snprintf(q, sizeof(q), "%s/mf", g_dir);
    CHECK(rp_make_dirs(q, err, sizeof(err)) == 0);
    CHECK(rp_write_file(p, "ok", 2, err, sizeof(err)) == 0);
    snprintf(p, sizeof(p), "%s/mf/bad.txt", g_dir);
    CHECK(rp_write_file(p, "bad", 3, err, sizeof(err)) == 0);
    CHECK(chmod(p, 0) == 0);
    snprintf(p, sizeof(p), "%s/gone/dst", g_dir);
    CHECK(rp_move_dir(q, p, err, sizeof(err)) != 0);
    CHECK(access(p, F_OK) != 0);
    snprintf(p, sizeof(p), "%s/mf/ok.txt", g_dir);
    CHECK(access(p, F_OK) == 0);
    snprintf(p, sizeof(p), "%s/mf/bad.txt", g_dir);
    CHECK(access(p, F_OK) == 0);
    chmod(p, 0600);
}

static int has_text(const char *path, const char *want) {
    char buf[64] = "";
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    return strcmp(buf, want) == 0;
}

/* A picked path may be a file or a drawer; both must copy. */
static void test_copy_file_or_drawer(void) {
    char err[256], p[512], q[512];
    snprintf(p, sizeof(p), "%s/pick/TP-TESTR/inner", g_dir);
    CHECK(rp_make_dirs(p, err, sizeof(err)) == 0);
    snprintf(p, sizeof(p), "%s/pick/solo.txt", g_dir);
    CHECK(rp_write_file(p, "solo", 4, err, sizeof(err)) == 0);
    snprintf(p, sizeof(p), "%s/pick/TP-TESTR/top.txt", g_dir);
    CHECK(rp_write_file(p, "top", 3, err, sizeof(err)) == 0);
    snprintf(p, sizeof(p), "%s/pick/TP-TESTR/inner/deep.txt", g_dir);
    CHECK(rp_write_file(p, "deep", 4, err, sizeof(err)) == 0);
    snprintf(q, sizeof(q), "%s/rel", g_dir);
    CHECK(rp_make_dirs(q, err, sizeof(err)) == 0);

    /* single file */
    snprintf(p, sizeof(p), "%s/pick/solo.txt", g_dir);
    snprintf(q, sizeof(q), "%s/rel/solo.txt", g_dir);
    CHECK(rp_copy_tree(p, q, err, sizeof(err)) == 0);
    CHECK(has_text(q, "solo"));

    /* drawer named like the release: its entries (loose file and nested drawer) land in the release drawer */
    snprintf(p, sizeof(p), "%s/pick/TP-TESTR/top.txt", g_dir);
    snprintf(q, sizeof(q), "%s/rel/top.txt", g_dir);
    CHECK(rp_copy_tree(p, q, err, sizeof(err)) == 0);
    CHECK(has_text(q, "top"));
    snprintf(p, sizeof(p), "%s/pick/TP-TESTR/inner", g_dir);
    snprintf(q, sizeof(q), "%s/rel/inner", g_dir);
    CHECK(rp_copy_tree(p, q, err, sizeof(err)) == 0);
    snprintf(q, sizeof(q), "%s/rel/inner/deep.txt", g_dir);
    CHECK(has_text(q, "deep"));

    /* whole drawer */
    snprintf(p, sizeof(p), "%s/pick/TP-TESTR", g_dir);
    snprintf(q, sizeof(q), "%s/rel/TP-TESTR", g_dir);
    CHECK(rp_copy_tree(p, q, err, sizeof(err)) == 0);
    snprintf(q, sizeof(q), "%s/rel/TP-TESTR/inner/deep.txt", g_dir);
    CHECK(has_text(q, "deep"));

    /* a missing source still fails with the read error */
    snprintf(p, sizeof(p), "%s/pick/missing.txt", g_dir);
    snprintf(q, sizeof(q), "%s/rel/missing.txt", g_dir);
    CHECK(rp_copy_tree(p, q, err, sizeof(err)) != 0);
    CHECK(strstr(err, "could not be read") != NULL);
}

int main(void) {
    setup();
    test_cmd();
    test_files();
    test_move_fail();
    test_copy_file_or_drawer();
    if (failures == 0) puts("stage: all passed");
    return failures != 0;
}
