#include "diz_count.h"
#include "nfo_x.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

int main(void) {
    char out[128];
    size_t w = rp_diz_slot_width(rp_x_banner(), RP_X_RELEASE_ROW);
    /* the slot is the span between the row's frame characters of the built-in banner */
    CHECK(w > 10 && w < 45);
    CHECK(rp_diz_slot_width("|     |", 0) == 5);
    CHECK(rp_diz_slot_width("ab\n.-    -.", 1) == 4); /* up to three frame characters each side */
    CHECK(rp_diz_slot_width("", 0) == 0);
    /* typed line wins, blanks not counted */
    rp_diz_line_counter("Release line", 20, "  HELLO  ", "Name", "Place", out, sizeof out);
    CHECK(strcmp(out, "Release line: 5/20 characters.") == 0);
    /* name and place: "<name> at <place>" */
    rp_diz_line_counter("Release line", 20, "", "Demo", "Gathering", out, sizeof out);
    CHECK(strcmp(out, "Release line: 17/20 characters.") == 0);
    /* name only, place only */
    rp_diz_line_counter("Release line", 20, "", "Demo", "", out, sizeof out);
    CHECK(strcmp(out, "Release line: 4/20 characters.") == 0);
    rp_diz_line_counter("Release line", 20, "", "", "Gathering", out, sizeof out);
    CHECK(strcmp(out, "Release line: 0/20 characters.") == 0);
    /* exactly the width fits, one more is cut off */
    rp_diz_line_counter("Release line", 5, "12345", "", "", out, sizeof out);
    CHECK(strcmp(out, "Release line: 5/5 characters.") == 0);
    rp_diz_line_counter("Release line", 5, "123456", "", "", out, sizeof out);
    CHECK(strcmp(out, "Release line: 6 characters, the diz fits 5; the rest is cut off.") == 0);
    /* the division's row label leads the line */
    rp_diz_line_counter("Tagline", 20, "HI", "", "", out, sizeof out);
    CHECK(strcmp(out, "Tagline: 2/20 characters.") == 0);
    /* a small buffer is cut but terminated */
    rp_diz_line_counter("Release line", 5, "123456", "", "", out, 8);
    CHECK(strlen(out) == 7);
    if (failures == 0) printf("test_diz_count: ok\n");
    return failures ? 1 : 0;
}
