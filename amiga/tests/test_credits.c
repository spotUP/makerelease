#include "credits.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* No TS test file exists for credits-creators / members; every case mirrors
 * the documented TS behaviour (see thoughts/spot/notes/agent-credits.md). */
static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

#define CHECK_STR(got, want) do { \
    char *g_ = (got); \
    if (!g_ || strcmp(g_, (want)) != 0) { printf("FAIL %d: got [%s] expect [%s]\n", __LINE__, g_ ? g_ : "<null>", (want)); failures++; } \
    free(g_); \
} while (0)

static void mem(rp_member *m, const char *nick, const char *role) {
    strcpy(m->nickname, nick);
    strcpy(m->role, role);
}

int main(void) {
    rp_credit c[RP_CREDITS_MAX];
    size_t n;
    rp_member m[8];

    /* creatorsFromCredits: order kept, repeats (any case) once, blanks skipped */
    n = rp_parse_credits("Code: FluBBa; Music: Spot; Text: Spot; Text: flubba", c, RP_CREDITS_MAX);
    CHECK(n == 4);
    CHECK_STR(rp_creators_from_credits(c, n), "FluBBa, Spot");
    CHECK_STR(rp_creators_from_credits(c, 0), "");

    /* parse: names share a role, trimmed, newline also ends a group */
    n = rp_parse_credits("  Code : A , B ;\n Graphics:C\r\nMusic: D", c, RP_CREDITS_MAX);
    CHECK(n == 4);
    CHECK(strcmp(c[0].role, "Code") == 0 && strcmp(c[0].name, "A") == 0);
    CHECK(strcmp(c[1].role, "Code") == 0 && strcmp(c[1].name, "B") == 0);
    CHECK(strcmp(c[2].role, "Graphics") == 0 && strcmp(c[2].name, "C") == 0);
    CHECK(strcmp(c[3].role, "Music") == 0 && strcmp(c[3].name, "D") == 0);

    /* parse: credits table rows split on '|', so a role may hold ':'; a row missing role or name is dropped */
    n = rp_parse_credits("Code: Gfx | A, B\nMusic\n | C\nText | T", c, RP_CREDITS_MAX);
    CHECK(n == 3);
    CHECK(strcmp(c[0].role, "Code: Gfx") == 0 && strcmp(c[0].name, "A") == 0);
    CHECK(strcmp(c[1].role, "Code: Gfx") == 0 && strcmp(c[1].name, "B") == 0);
    CHECK(strcmp(c[2].role, "Text") == 0 && strcmp(c[2].name, "T") == 0);

    /* parse: a row needs role and name; no colon, empty role, empty names dropped */
    n = rp_parse_credits("Code; : X; Music: ; Gfx: , ; Text: T", c, RP_CREDITS_MAX);
    CHECK(n == 1);
    CHECK(strcmp(c[0].role, "Text") == 0 && strcmp(c[0].name, "T") == 0);
    CHECK(rp_parse_credits("", c, RP_CREDITS_MAX) == 0);
    CHECK(rp_parse_credits("A: x; B: y; C: z", c, 2) == 2);

    /* creditNamesForRole: loose role match, dedupe within the matched set */
    n = rp_parse_credits("Code: Spot; Additional code: Hoff, spot; Music: Lazy; ANSI art: Lazy; Logo: Ink", c, RP_CREDITS_MAX);
    CHECK_STR(rp_credit_names_for_role(c, n, RP_ROLE_CODER), "Spot, Hoff");
    CHECK_STR(rp_credit_names_for_role(c, n, RP_ROLE_DESIGN), "Lazy, Ink");
    n = rp_parse_credits("Programmer: P; Developer: D; Music: M", c, RP_CREDITS_MAX);
    CHECK_STR(rp_credit_names_for_role(c, n, RP_ROLE_CODER), "P, D");
    CHECK_STR(rp_credit_names_for_role(c, n, RP_ROLE_DESIGN), "");
    /* a name that only matched a non-coder role first still shows for coder */
    n = rp_parse_credits("Music: Zed; Code: Zed", c, RP_CREDITS_MAX);
    CHECK_STR(rp_credit_names_for_role(c, n, RP_ROLE_CODER), "Zed");

    /* creditsError */
    n = rp_parse_credits("Code: A", c, RP_CREDITS_MAX);
    CHECK(rp_credits_error(c, 0, 1) != NULL);
    CHECK(strcmp(rp_credits_error(c, 0, 1), "Add at least one credit - the release history creator line is built from them.") == 0);
    CHECK(rp_credits_error(c, 0, 0) == NULL);
    CHECK(rp_credits_error(c, n, 1) == NULL);

    /* mergeMemberSkills: one entry per member, first-seen order, no repeated skills */
    mem(&m[0], "Spot", "coder");
    mem(&m[1], "Hoff", "music, gfx");
    mem(&m[2], "Spot", "Coder, musician");
    mem(&m[3], "  ", "x");
    mem(&m[4], " Hoff ", "GFX,, text");
    n = rp_merge_member_skills(m, 5);
    CHECK(n == 2);
    CHECK(strcmp(m[0].nickname, "Spot") == 0 && strcmp(m[0].role, "coder, musician") == 0);
    CHECK(strcmp(m[1].nickname, "Hoff") == 0 && strcmp(m[1].role, "music, gfx, text") == 0);

    /* nickname match is exact (case), blank role stays blank, overflow skill dropped whole */
    mem(&m[0], "a", "");
    mem(&m[1], "A", "x");
    mem(&m[2], "a", "012345678901234567890123456789");
    mem(&m[3], "a", "yy");
    n = rp_merge_member_skills(m, 4);
    CHECK(n == 2);
    CHECK(strcmp(m[0].role, "012345678901234567890123456789, yy") != 0);
    CHECK(strcmp(m[0].role, "012345678901234567890123456789") == 0);
    CHECK(strcmp(m[1].nickname, "A") == 0 && strcmp(m[1].role, "x") == 0);

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("test_credits ok\n");
    return 0;
}
