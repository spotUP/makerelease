#include "nfo_x.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

static char *read_file(const char *path, size_t *len_out) {
    FILE *f = fopen(path, "rb");
    char *buf;
    size_t n;
    long len;
    if (!f) {
        printf("FAIL: cannot open %s\n", path);
        failures++;
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc((size_t)(len < 0 ? 0 : len) + 1);
    n = len > 0 ? fread(buf, 1, (size_t)len, f) : 0;
    fclose(f);
    buf[n] = '\0';
    if (len_out) *len_out = n;
    return buf;
}

/* Byte-compare got against a golden file generated from the TypeScript renderer. */
static void expect_file(const char *got, const char *golden_name) {
    char path[128];
    size_t n;
    char *want;
    snprintf(path, sizeof(path), "tests/fixtures/%s", golden_name);
    want = read_file(path, &n);
    if (!want) return;
    if (!got || strlen(got) != n || memcmp(got, want, n) != 0) {
        size_t i = 0;
        while (got && i < n && got[i] == want[i]) i++;
        printf("FAIL %s: differs at byte %lu (got len %lu, want %lu)\n", golden_name, (unsigned long)i,
               got ? (unsigned long)strlen(got) : 0UL, (unsigned long)n);
        failures++;
    }
    free(want);
}

static void test_nfo(void) {
    static const rp_member members[16] = {
        { "sANDMAN", "sysops" }, { "zALO", "sysops, coders" }, { "sPOT", "coders,sysops" }, { "gOTO80", "artists" },
        { "nickname0", "musicians" }, { "nickname1", "musicians" }, { "nickname2", "musicians" },
        { "nickname3", "musicians" }, { "nickname4", "musicians" }, { "nickname5", "musicians" },
        { "nickname6", "musicians" }, { "nickname7", "musicians" }, { "nickname8", "musicians" },
        { "nickname9", "musicians" }, { "nickname10", "musicians" }, { "nickname11", "musicians" },
    };
    static const rp_x_board boards[3] = {
        { "uPTOWN", "sPOT", "wEBHQ", "SOON" }, { "diGiTAL hiGHWAY", "zALO", "sHQ", "ASK4IT" },
        { "tHE yARD", "zANER", "gHQ", "tEMP-dOWN" },
    };
    char notes[1024];
    size_t w;
    char *shipped = read_file("tests/fixtures/x-shipped.diz.in", NULL);
    char *art0 = read_file("tests/fixtures/x-1.list0.txt", NULL);
    char *art1 = read_file("tests/fixtures/x-1.list1.txt", NULL);
    char *nfo, *derived0, *derived1;
    rp_x_entry list[2];
    rp_nfo_x_form f;
    if (!shipped || !art0 || !art1) return;
    strcpy(notes, "Description\r\n-----------\r\nSecond-level cursor menu for AmiExpress BBS systems.\r\n    word");
    for (w = 1; w < 30; w++) strcat(notes, " word");
    strcat(notes, "\r\n\r\ntrailing   \r\n"
                  "averyveryveryveryveryveryveryveryveryveryveryveryveryverylongwordwithoutbreaks ok");
    /* list art is derived from the banner, not typed */
    derived0 = rp_x_list_art(rp_x_banner());
    derived1 = rp_x_list_art(shipped);
    CHECK(derived0 && strcmp(derived0, art0) == 0);
    CHECK(derived1 && strcmp(derived1, art1) == 0);
    list[0].number = "o18";
    list[0].art = art0;
    list[1].number = "o17";
    list[1].art = art1;
    memset(&f, 0, sizeof f);
    f.release_name = "mORE dOORS 1.5";
    f.release_date = "2026-09-21";
    f.coder = "zalo";
    f.system = "aMiEXPRESS";
    f.design = "dinoLONGERTHAN10";
    f.notes = notes;
    f.news = "\n  hello world  \n\nsecond line of news that is a bit long to centre\n\n";
    f.members = members;
    f.nmembers = 16;
    f.boards = boards;
    f.nboards = 3;
    f.entries = list;
    f.nentries = 2;
    nfo = rp_render_nfo_x(&f);
    expect_file(nfo, "x-1.NFO");
    free(nfo);

    memset(&f, 0, sizeof f);
    f.release_name = "test";
    f.release_date = "2026-09-21";
    f.coder = "zalo";
    f.system = "aMiEXPRESS";
    f.design = "dino";
    f.notes = "notes";
    f.news = "";
    nfo = rp_render_nfo_x(&f);
    expect_file(nfo, "x-2.NFO");
    free(nfo);

    f.release_name = "Name $& here";
    f.release_date = "$$";
    f.coder = "$&";
    f.system = "$1";
    f.design = "$'";
    f.notes = "costs $& $1 $$ nothing";
    f.news = "news $& too";
    nfo = rp_render_nfo_x(&f);
    expect_file(nfo, "x-3.NFO");
    free(nfo);
    free(derived0);
    free(derived1);
    free(shipped);
    free(art0);
    free(art1);
}

static void test_diz(void) {
    char *a = rp_diz_apply_release_line(rp_x_banner(), RP_X_RELEASE_ROW, "mORE dOORS cURSOR mENU 1.5");
    char *b, *c, *t;
    char name[64];
    char *shipped = read_file("tests/fixtures/x-shipped.diz.in", NULL);
    char *names = read_file("tests/fixtures/x-names.txt", NULL);
    expect_file(a, "x-1.DIZ");
    /* two rows, the second clipped-free tagline on row 9 */
    b = rp_diz_apply_release_line(rp_x_banner(), 8, "a release line far too long to fit between the frames of the art");
    t = rp_diz_apply_release_line(b, 9, "tagline");
    expect_file(t, "x-2.DIZ");
    free(b);
    free(t);
    b = rp_diz_apply_release_line(rp_x_banner(), 8, "");
    c = rp_diz_apply_release_line(b, 99, "clamped");
    expect_file(c, "x-3.DIZ");
    free(b);
    free(c);
    /* Released at: row 8 reads "name at place"; empty place or name leaves the diz as it was */
    b = rp_diz_with_released_at(rp_x_banner(), "Demo", " Mysdata 2026 ");
    c = rp_diz_apply_release_line(rp_x_banner(), RP_X_RELEASE_ROW, "Demo at Mysdata 2026");
    CHECK(b && c && strcmp(b, c) == 0);
    free(b);
    free(c);
    b = rp_diz_with_released_at(rp_x_banner(), "Demo", "");
    CHECK(b && strcmp(b, rp_x_banner()) == 0);
    free(b);
    b = rp_diz_with_released_at(rp_x_banner(), "", "Mysdata");
    CHECK(b && strcmp(b, rp_x_banner()) == 0);
    free(b);
    /* a typed Diz line wins, trimmed, even without name or place; blank falls back to Released at */
    b = rp_diz_with_line(rp_x_banner(), "", "", "  Hand Typed  ");
    c = rp_diz_apply_release_line(rp_x_banner(), RP_X_RELEASE_ROW, "Hand Typed");
    CHECK(b && c && strcmp(b, c) == 0);
    free(b);
    free(c);
    b = rp_diz_with_line(rp_x_banner(), "Demo", "Mysdata", " \t");
    c = rp_diz_with_released_at(rp_x_banner(), "Demo", "Mysdata");
    CHECK(b && c && strcmp(b, c) == 0);
    free(b);
    free(c);
    free(a);
    if (shipped && names) {
        char want[128];
        rp_x_release_name_in_banner(shipped, name, sizeof(name));
        snprintf(want, sizeof(want), "%s\n\n%s\n", name, "x y");
        CHECK(strcmp(name, "mORE dOORS cURSOR mENU 1.5") == 0);
        rp_x_release_name_in_banner(rp_x_banner(), name, sizeof(name));
        CHECK(name[0] == '\0');
        rp_x_release_name_in_banner("a\n|  Presents: |\n|---|\n| x y |", name, sizeof(name));
        CHECK(strcmp(name, "x y") == 0);
    }
    free(shipped);
    free(names);
}

/* The name line wraps a long release name onto further framed rows; it never cuts the name. */
static void test_name_not_cut(void) {
    static const char *names[2] = {
        "Test release name",
        "A much longer release name that goes on and on past sixty columns of field width",
    };
    int i;
    for (i = 0; i < 2; i++) {
        rp_nfo_x_form f;
        char *nfo;
        memset(&f, 0, sizeof f);
        f.release_name = names[i];
        f.release_date = f.coder = f.system = f.design = f.notes = f.news = "";
        nfo = rp_render_nfo_x(&f);
        CHECK(nfo != NULL);
        if (!nfo) continue;
        if (i == 0) {
            CHECK(strstr(nfo, "<<-- Test release name -->>") != NULL);
        } else {
            CHECK(strstr(nfo, "<<-- A much longer release name that goes on and on past | %% |\n") != NULL);
            CHECK(strstr(nfo, "  sixty columns of field width -->>  ") != NULL);
        }
        free(nfo);
    }
}

int main(void) {
    test_nfo();
    test_diz();
    test_name_not_cut();
    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("nfo_x: ok\n");
    return 0;
}
