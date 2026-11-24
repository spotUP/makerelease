#include "last_choice.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

int main(void) {
    const char *path = "last_choice_test.tmp";
    rp_last_choice c, back;
    memset(&c, 0, sizeof c);
    /* no file: empty choice, LHA */
    remove(path);
    CHECK(rp_last_choice_load(path, &back) == -1 && back.group[0] == '\0' && back.zip == 0);
    /* a saved choice comes back */
    strcpy(c.group, "uprough");
    c.zip = 1;
    CHECK(rp_last_choice_save(path, &c) == 0);
    CHECK(rp_last_choice_load(path, &back) == 0 && strcmp(back.group, "uprough") == 0 && back.zip == 1);
    c.zip = 0;
    CHECK(rp_last_choice_save(path, &c) == 0);
    CHECK(rp_last_choice_load(path, &back) == 0 && back.zip == 0);
    /* a group line alone is LHA; an empty file is no choice */
    { FILE *f = fopen(path, "wb"); fputs("xinnovations\r\n", f); fclose(f); }
    CHECK(rp_last_choice_load(path, &back) == 0 && strcmp(back.group, "xinnovations") == 0 && back.zip == 0);
    { FILE *f = fopen(path, "wb"); fclose(f); }
    CHECK(rp_last_choice_load(path, &back) == -1 && back.group[0] == '\0');
    /* an unwritable path reports failure */
    CHECK(rp_last_choice_save("/nonexistent-dir/x", &c) == -1);
    remove(path);
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("test_last_choice: ok\n");
    return 0;
}
