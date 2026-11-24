#include "divisions.h"

#include "nfo_x.h"

/* UP_ROUGH_BANNER of divisions.ts; the /X one is rp_x_banner(). */
static const char *const UP_ROUGH_BANNER =
    "    _ _ __ ___    ____  _ _ __ _________\n"
    "   | | Y  / _ \\../ __ \\| | Y  / __  | | |\n"
    "dNo| | | | | | || /_ | | | | | |_ | l | |dS!\n"
    "|  | | | | |`| || || | | | | | || |___| |  |\n"
    "|  | | l | |__/'| || | | | | | || |   | |  |\n"
    "|  | l___| |    | || | | l l | ll | | | |  |\n"
    "|  `\\______|    |_|`\\_/'\\___/`\\__/l_l_|_|  |\n"
    "|            Up Rough Presents:            |\n"
    "|                                          |\n"
    "`------------------------------------------'";

static const rp_diz_row UP_ROUGH_ROWS[] = { { 8, "Release line" } };
static const rp_diz_row X_ROWS[] = { { RP_X_RELEASE_ROW, "Release line" } };

const rp_division *rp_division_for(const rp_group *g) {
    static rp_division up_rough = { NULL, UP_ROUGH_ROWS, 1 };
    static rp_division x = { NULL, X_ROWS, 1 };
    if (g->renderer == R_PLACEHOLDER) return NULL;
    up_rough.banner = UP_ROUGH_BANNER;
    x.banner = rp_x_banner();
    return g->renderer == R_XINNOVATIONS ? &x : &up_rough;
}
