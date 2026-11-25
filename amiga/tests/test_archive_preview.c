#include "archive_preview.h"

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

static char g_dir[256];

static void put(const char *rel) {
    char p[512], d[512], err[256], *s;
    snprintf(p, sizeof(p), "%s/%s", g_dir, rel);
    snprintf(d, sizeof(d), "%s", p);
    s = strrchr(d, '/');
    *s = '\0';
    CHECK(rp_make_dirs(d, err, sizeof(err)) == 0);
    CHECK(rp_write_file(p, "x", 1, err, sizeof(err)) == 0);
}

static int has(const rp_preview *r, const char *e) {
    size_t i;
    for (i = 0; i < r->nentries; i++)
        if (strcmp(r->entries[i], e) == 0) return 1;
    return 0;
}

int main(void) {
    char *t = getenv("TMPDIR"), err[256], a[512], b[512], c[512], want[512];
    const char *paths[1];
    rp_preview r;
    snprintf(g_dir, sizeof(g_dir), "%s/rp-prev-XXXXXX", t ? t : "/tmp");
    if (!mkdtemp(g_dir)) return 1;

    /* a folder that only wraps one more folder is flagged with the innermost path */
    put("wrap/inner/a.txt");
    put("wrap/inner/b.txt");
    snprintf(a, sizeof(a), "%s/wrap", g_dir);
    paths[0] = a;
    CHECK(rp_preview_archive("REL", paths, 1, &r, err, sizeof(err)) == 0);
    snprintf(want, sizeof(want), "%s/wrap/inner", g_dir);
    CHECK(r.nnested == 1 && strcmp(r.nested[0].path, a) == 0 && strcmp(r.nested[0].flatten_to, want) == 0);
    CHECK(has(&r, "REL/wrap/inner/a.txt"));
    /* diz and NFO at the archive root, the files in the one REL/ drawer */
    CHECK(strcmp(r.entries[0], "file_id.diz") == 0 && strcmp(r.entries[1], "REL.NFO") == 0 &&
          strcmp(r.entries[2], "REL/") == 0 && !has(&r, "REL/file_id.diz") && !has(&r, "REL/REL.NFO"));
    rp_preview_free(&r);

    /* real content beside the subfolder: no warning */
    put("pack/readme.txt");
    put("pack/sub/a.txt");
    snprintf(b, sizeof(b), "%s/pack", g_dir);
    paths[0] = b;
    CHECK(rp_preview_archive("REL", paths, 1, &r, err, sizeof(err)) == 0);
    CHECK(r.nnested == 0);
    rp_preview_free(&r);

    /* a folder named like the release merges instead of nesting */
    put("rel/a.txt");
    snprintf(c, sizeof(c), "%s/rel", g_dir);
    paths[0] = c;
    CHECK(rp_preview_archive("REL", paths, 1, &r, err, sizeof(err)) == 0);
    CHECK(has(&r, "REL/a.txt") && !has(&r, "REL/rel/"));
    rp_preview_free(&r);

    /* no drawer: the three fixed rows still list */
    CHECK(rp_preview_archive("REL", paths, 0, &r, err, sizeof(err)) == 0);
    CHECK(r.nentries == 3 && strcmp(r.entries[0], "file_id.diz") == 0 && strcmp(r.entries[1], "REL.NFO") == 0);
    rp_preview_free(&r);

    rp_remove_tree(g_dir, err, sizeof(err));
    if (failures == 0) printf("archive_preview: ok\n");
    return failures ? 1 : 0;
}
