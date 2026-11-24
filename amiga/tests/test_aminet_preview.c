#include "aminet_preview.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main(void) {
    static const rp_aminet_credit cr[] = {{"Code", "Spot"}};
    rp_aminet_readme_in in = {"A demo", "demo/aga", "m68k-amigaos", "spot@uprough.net", "Spot",
        "Up Rough", "https://www.uprough.net", "Spot", "Our demo.", "", cr, 1};
    rp_aminet_preview p;

    CHECK(rp_aminet_make_preview("UP-HYPOC.LHA", &in, &p) == 0);
    CHECK(p.n_problems == 0 && p.problems[0] == 0);
    CHECK(!strcmp(p.archive, "up-hypoc.lha") && !strcmp(p.readme_name, "up-hypoc.readme"));
    CHECK(p.readme && !strncmp(p.readme, "Short:        A demo\n", 21) && strstr(p.readme, "  Code: Spot\n"));
    {
        char *v = rp_aminet_preview_view(&p);
        CHECK(v && !strncmp(v, "No problems.\n\nShort:", 20));
        free(v);
    }
    rp_aminet_preview_free(&p);
    CHECK(p.readme == NULL);

    /* problems come back with the preview: empty short, bad file name */
    in.short_text = "";
    CHECK(rp_aminet_make_preview("a b.lha", &in, &p) == 0);
    CHECK(p.n_problems == 3 && strstr(p.problems, "Short: is empty") && strstr(p.problems, "may only contain"));
    {
        char *v = rp_aminet_preview_view(&p);
        CHECK(v && !strncmp(v, "Problems: ", 10) && strstr(v, "Short: is empty") && strstr(v, "\n\nShort:"));
        free(v);
    }
    rp_aminet_preview_free(&p);

    /* name longer than the buffers */
    {
        char big[200];
        memset(big, 'a', sizeof big - 1); big[sizeof big - 1] = 0;
        CHECK(rp_aminet_make_preview(big, &in, &p) == -1 && p.readme == NULL && p.n_problems == 1);
    }

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("test_aminet_preview ok\n");
    return 0;
}
