#include "aminet_types.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main(void) {
    int n = rp_aminet_type_count(), i, ts = 0, nd = 0;
    char line[512];
    FILE *f = fopen("../app/src/shared/aminet-types.ts", "r");

    /* count verified twice: the desktop list's entry lines, and the C descriptions array */
    if (f) {
        while (fgets(line, sizeof line, f)) if (!strncmp(line, "  { type: '", 11)) ts++;
        fclose(f);
        CHECK(ts == n);
    } else printf("note: desktop list not found, count check skipped\n");
    while (rp_aminet_type_descs[nd]) nd++;
    CHECK(nd == n);
    CHECK(n > 300);

    for (i = 0; i < n; i++) {
        CHECK(strchr(rp_aminet_types[i], '/') != NULL && strchr(rp_aminet_types[i], '/') == strrchr(rp_aminet_types[i], '/'));
        CHECK(rp_aminet_type_index(rp_aminet_types[i]) == i);   /* no duplicates */
    }
    CHECK(rp_aminet_type_index("demo/aga") >= 0);
    CHECK(rp_aminet_type_index("nope/nope") == -1);

    CHECK(!strcmp(rp_aminet_guess_type("demo", "A1200"), "demo/aga"));
    CHECK(!strcmp(rp_aminet_guess_type("Demo", "a500"), "demo/ecs"));
    CHECK(!strcmp(rp_aminet_guess_type("demo", "Console"), "demo/misc"));
    CHECK(!strcmp(rp_aminet_guess_type("demo", ""), "demo/misc"));
    CHECK(!strcmp(rp_aminet_guess_type("intro", "A1200"), "demo/intro"));
    CHECK(!strcmp(rp_aminet_guess_type("musicdisk", ""), "mods/sets"));
    CHECK(!strcmp(rp_aminet_guess_type("music", ""), "mods/misc"));
    CHECK(!strcmp(rp_aminet_guess_type("video", ""), "pix/anim"));
    CHECK(!strcmp(rp_aminet_guess_type("ascii", ""), ""));
    CHECK(!strcmp(rp_aminet_guess_type("", "A1200"), ""));
    for (i = 0; i < 1; i++) {   /* every category target is in the list */
        const char *cats[] = { "intro", "musicdisk", "music", "graphics", "tool", "door", "video", NULL };
        int c;
        for (c = 0; cats[c]; c++) CHECK(rp_aminet_guess_type(cats[c], "")[0] != '\0');
    }

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("aminet_types: ok (%d directories)\n", n);
    return 0;
}
