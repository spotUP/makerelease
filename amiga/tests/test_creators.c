#include "creators.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

int main(void) {
    rp_member m[4];
    char tok[64], out[128];
    const char *hit[8];
    rp_credit c[3];
    memset(m, 0, sizeof m);
    strcpy(m[0].nickname, "Spot");
    strcpy(m[1].nickname, "Spice");
    strcpy(m[2].nickname, "Zed");
    strcpy(m[3].nickname, "Zedd");
    /* token = text after the last separator, trimmed */
    CHECK(rp_creators_token("Code: Sp", tok, sizeof tok) == 5 && strcmp(tok, "Sp") == 0);
    CHECK(rp_creators_token("Code: Spot, Ze ", tok, sizeof tok) == 11 && strcmp(tok, "Ze") == 0);
    CHECK(rp_creators_token("Code: Spot; Mu", tok, sizeof tok) == 11 && strcmp(tok, "Mu") == 0);
    CHECK(rp_creators_token("Code: ", tok, sizeof tok) == 5 && tok[0] == '\0');
    CHECK(rp_creators_token("Spo", tok, sizeof tok) == 0 && strcmp(tok, "Spo") == 0);
    /* matches: case-insensitive prefix, empty token nothing */
    CHECK(rp_creators_matches("sp", m, 4, hit, 8) == 2 && strcmp(hit[0], "Spot") == 0 && strcmp(hit[1], "Spice") == 0);
    CHECK(rp_creators_matches("z", m, 4, hit, 1) == 1);
    CHECK(rp_creators_matches("", m, 4, hit, 8) == 0);
    CHECK(rp_creators_matches("q", m, 4, hit, 8) == 0);
    /* complete: unique prefix expands in place, keeping the text before it */
    CHECK(rp_creators_complete("Code: Spo", m, 4, out, sizeof out) == 1 && strcmp(out, "Code: Spot") == 0);
    CHECK(rp_creators_complete("Code: Spot, Spi", m, 4, out, sizeof out) == 1 && strcmp(out, "Code: Spot, Spice") == 0);
    /* ambiguous, exact (any case), unknown and empty tokens are left alone */
    CHECK(rp_creators_complete("Code: Sp", m, 4, out, sizeof out) == 0);
    CHECK(rp_creators_complete("Code: zed", m, 4, out, sizeof out) == 0);
    CHECK(rp_creators_complete("Code: Bob", m, 4, out, sizeof out) == 0);
    CHECK(rp_creators_complete("Code: ", m, 4, out, sizeof out) == 0);
    CHECK(rp_creators_complete("Code: Spo", m, 4, out, 8) == 0);
    CHECK(rp_creators_pick("Code: Spot, Sp", "Spice", out, sizeof out) == 1 && strcmp(out, "Code: Spot, Spice") == 0);
    CHECK(rp_creators_pick("Code: Spot; Music:  j", "Jester", out, sizeof out) == 1 && strcmp(out, "Code: Spot; Music:  Jester") == 0);
    CHECK(rp_creators_pick("Sp", "Spot", out, sizeof out) == 1 && strcmp(out, "Spot") == 0);
    CHECK(rp_creators_pick("Code: Sp", "Spot", out, 10) == 0);
    /* preview */
    memset(c, 0, sizeof c);
    rp_creators_preview(c, 0, out, sizeof out);
    CHECK(strstr(out, "add a credit") != NULL);
    rp_x_credits_preview(c, 0, out, sizeof out);
    CHECK(strcmp(out, "NFO header: cODER [-]  dESIGN [-] - add credits with roles like Code or Graphics") == 0);
    strcpy(c[0].role, "Code"); strcpy(c[0].name, "Spot");
    strcpy(c[1].role, "Graphics"); strcpy(c[1].name, "Zed");
    strcpy(c[2].role, "Music"); strcpy(c[2].name, "Spot");
    rp_creators_preview(c, 3, out, sizeof out);
    CHECK(strcmp(out, "Release history creator line: Spot, Zed") == 0);
    rp_x_credits_preview(c, 3, out, sizeof out);
    CHECK(strcmp(out, "NFO header: cODER [Spot]  dESIGN [Zed]") == 0);
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("test_creators: ok\n");
    return 0;
}
