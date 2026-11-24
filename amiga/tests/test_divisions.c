#include "divisions.h"

#include "nfo_x.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

int main(void) {
    rp_group g;
    const rp_division *d;
    memset(&g, 0, sizeof g);
    g.renderer = R_UPROUGH;
    d = rp_division_for(&g);
    CHECK(d != NULL && d->nrows == 1 && d->rows[0].row == 8 && strcmp(d->rows[0].label, "Release line") == 0);
    CHECK(strstr(d->banner, "Up Rough Presents:") != NULL && rp_diz_slot_width(d->banner, d->rows[0].row) == 42);
    g.renderer = R_XINNOVATIONS;
    d = rp_division_for(&g);
    CHECK(d != NULL && d->nrows == 1 && d->rows[0].row == RP_X_RELEASE_ROW);
    CHECK(d->banner == rp_x_banner() && rp_diz_slot_width(d->banner, d->rows[0].row) == 42);
    g.renderer = R_PLACEHOLDER;
    CHECK(rp_division_for(&g) == NULL);
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("test_divisions: ok\n");
    return 0;
}
