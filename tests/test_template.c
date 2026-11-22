#include "template.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

#define CHECK_STR(a, b) do { \
    if (strcmp((a), (b)) != 0) { printf("FAIL %d:\n  got    [%s]\n  expect [%s]\n", __LINE__, (a), (b)); failures++; } \
} while (0)

static rp_values no_values(void) {
    rp_values v = { NULL, 0, NULL, 0 };
    return v;
}

static void test_scalar_plain(void) {
    rp_scalar s[] = { { "group", "Up Rough" }, { "code", "ABC12" } };
    rp_values v = { s, 2, NULL, 0 };
    int line = -1;
    char err[256];
    char *out = rp_render_template("by {{group}} ({{code}})", &v, &line, err, sizeof(err));
    CHECK(out != NULL);
    CHECK_STR(out, "by Up Rough (ABC12)");
    free(out);
}

static void test_value_dollar_stays_literal(void) {
    rp_scalar s[] = { { "description", "costs $& and $1" } };
    rp_values v = { s, 1, NULL, 0 };
    int line = -1;
    char err[256];
    char *out = rp_render_template("{{description}}", &v, &line, err, sizeof(err));
    CHECK(out != NULL);
    CHECK_STR(out, "costs $& and $1");
    free(out);
}

static void test_fixed_width_pads(void) {
    rp_scalar s[] = { { "code", "AB" } };
    rp_values v = { s, 1, NULL, 0 };
    int line = -1;
    char err[256];
    /* marker {{code     }} is 13 wide; mirrors the desktop fixture */
    char *out = rp_render_template("[{{code     }}]", &v, &line, err, sizeof(err));
    CHECK(out != NULL);
    CHECK_STR(out, "[AB           ]");
    free(out);
}

static void test_fixed_width_overflow_is_error(void) {
    rp_scalar s[] = { { "code", "TOO-LONG-CODE" } };
    rp_values v = { s, 1, NULL, 0 };
    int line = -1;
    char err[256];
    char *out = rp_render_template("ok\n{{code }}", &v, &line, err, sizeof(err));
    CHECK(out == NULL);
    CHECK(line == 2);
    CHECK_STR(err, "line 2: value for code is 13 characters, field is 9");
}

static void test_unknown_placeholder(void) {
    rp_values v = no_values();
    int line = -1;
    char err[256];
    char *out = rp_render_template("{{Group}}", &v, &line, err, sizeof(err));
    CHECK(out == NULL);
    CHECK(line == 1);
    CHECK_STR(err, "line 1: unknown placeholder {{Group}}");
}

static void test_block_repeats_frame(void) {
    const char *members[] = { "spot code", "pixel gfx" };
    rp_block b[] = { { "members", members, 2 } };
    rp_values v = { NULL, 0, b, 1 };
    int line = -1;
    char err[256];
    char *out = rp_render_template("| {{members}} |", &v, &line, err, sizeof(err));
    CHECK(out != NULL);
    CHECK_STR(out, "| spot code |\n| pixel gfx |");
    free(out);
}

static void test_empty_block_removes_line(void) {
    rp_block b[] = { { "news", NULL, 0 } };
    rp_values v = { NULL, 0, b, 1 };
    int line = -1;
    char err[256];
    char *out = rp_render_template("head\n  {{news}}\nbare", &v, &line, err, sizeof(err));
    CHECK(out != NULL);
    CHECK_STR(out, "head\n\nbare");
    free(out);
}

static void test_empty_value_trims_frame_padding(void) {
    /* Mirrors the desktop fixture: only trailing whitespace goes, so the
     * '|' survives and '| x ' keeps its suffix space. */
    const char *notes[] = { "", "x" };
    rp_block b[] = { { "notes", notes, 2 } };
    rp_values v = { NULL, 0, b, 1 };
    int line = -1;
    char err[256];
    char *out = rp_render_template("| {{notes}} ", &v, &line, err, sizeof(err));
    CHECK(out != NULL);
    CHECK_STR(out, "|\n| x ");
    free(out);
}

static void test_block_inline_is_error(void) {
    const char *news[] = { "x" };
    rp_block b[] = { { "news", news, 1 } };
    rp_values v = { NULL, 0, b, 1 };
    int line = -1;
    char err[256];
    char *out = rp_render_template("news: {{news}}", &v, &line, err, sizeof(err));
    CHECK(out == NULL);
    CHECK(line == 1);
    CHECK_STR(err, "line 1: block placeholder {{news}} must stand alone on its line");
}

static void test_block_with_scalar_is_error(void) {
    rp_scalar s[] = { { "group", "g" } };
    const char *news[] = { "x" };
    rp_block b[] = { { "news", news, 1 } };
    rp_values v = { s, 1, b, 1 };
    int line = -1;
    char err[256];
    char *out = rp_render_template("{{group}} {{news}}", &v, &line, err, sizeof(err));
    CHECK(out == NULL);
    CHECK_STR(err, "line 1: block placeholder {{news}} must stand alone on its line");
}

static void test_scalar_block_on_line_with_text_is_error(void) {
    rp_scalar s[] = { { "group", "g" } };
    rp_values v = { s, 1, NULL, 0 };
    int line = -1;
    char err[256];
    /* a block name used inline: desktop reports "must stand alone" */
    char *out = rp_render_template("by {{members}}", &v, &line, err, sizeof(err));
    CHECK(out == NULL);
    CHECK_STR(err, "line 1: block placeholder {{members}} must stand alone on its line");
}

static void test_normalise_endings(void) {
    char text[] = "a\r\nb\rc\n";
    rp_normalise_line_endings(text);
    CHECK_STR(text, "a\nb\nc\n");
}

int main(void) {
    test_scalar_plain();
    test_value_dollar_stays_literal();
    test_fixed_width_pads();
    test_fixed_width_overflow_is_error();
    test_unknown_placeholder();
    test_block_repeats_frame();
    test_empty_block_removes_line();
    test_empty_value_trims_frame_padding();
    test_block_inline_is_error();
    test_block_with_scalar_is_error();
    test_scalar_block_on_line_with_text_is_error();
    test_normalise_endings();
    if (failures == 0) puts("template: all passed");
    return failures != 0;
}
