#include "editor_logic.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main(void) {
    char err[128], f[8], buf[256];
    int v = 0;
    size_t n = 0;
    rp_member m[RP_MEMBERS_MAX];

    CHECK(rp_editor_set_field(f, sizeof f, "  abc \t", "Label", err, sizeof err) == 0 && strcmp(f, "abc") == 0);
    CHECK(rp_editor_set_field(f, sizeof f, "1234567", "Label", err, sizeof err) == 0);
    strcpy(f, "keep");
    CHECK(rp_editor_set_field(f, sizeof f, "12345678", "Label", err, sizeof err) == -1);
    CHECK(strcmp(f, "keep") == 0 && strstr(err, "Label is too long") && strstr(err, "7"));
    CHECK(rp_editor_set_field(f, sizeof f, NULL, "Label", err, sizeof err) == 0 && f[0] == '\0');

    CHECK(rp_editor_code_length("5", &v, err, sizeof err) == 0 && v == 5);
    CHECK(rp_editor_code_length(" 32 ", &v, err, sizeof err) == 0 && v == 32);
    CHECK(rp_editor_code_length("0", &v, err, sizeof err) == -1);
    CHECK(rp_editor_code_length("33", &v, err, sizeof err) == -1);
    CHECK(rp_editor_code_length("", &v, err, sizeof err) == -1);
    CHECK(rp_editor_code_length("5x", &v, err, sizeof err) == -1);

    CHECK(rp_editor_check_id("up-rough", err, sizeof err) == 0);
    CHECK(rp_editor_check_id("Up", err, sizeof err) == -1 && strstr(err, "a-z"));
    CHECK(rp_editor_check_id("", err, sizeof err) == -1);
    CHECK(rp_editor_check_id("a b", err, sizeof err) == -1);

    CHECK(rp_editor_members_parse("zed | Code\nAnna | Music\n", m, RP_MEMBERS_MAX, &n, err, sizeof err) == 0 && n == 2);
    CHECK(strcmp(m[0].nickname, "Anna") == 0 && strcmp(m[1].role, "Code") == 0);
    CHECK(rp_editor_members_format(m, n, buf, sizeof buf) == 0 && strstr(buf, "Anna") && strstr(buf, "zed"));
    CHECK(rp_editor_members_format(m, n, buf, 5) == -1);
    CHECK(rp_editor_members_parse("| orphan role\n", m, RP_MEMBERS_MAX, &n, err, sizeof err) == -1 && strstr(err, "Members: Line 1"));
    CHECK(rp_editor_members_parse("averyveryveryverylongnicknamethatdoesnotfit | x\n", m, RP_MEMBERS_MAX, &n, err, sizeof err) == -1);
    CHECK(rp_editor_members_parse("", m, RP_MEMBERS_MAX, &n, err, sizeof err) == 0 && n == 0);

    if (failures == 0) puts("editor_logic: all passed");
    return failures ? 1 : 0;
}
