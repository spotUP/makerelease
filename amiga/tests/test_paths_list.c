#include "paths_list.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

int main(void) {
    static rp_paths p;
    char big[RP_PATH_CAP + 8];
    int i;
    CHECK(rp_paths_add(&p, "") == 0 && rp_paths_add(&p, NULL) == 0 && p.n == 0);
    CHECK(rp_paths_add(&p, "Work:Music/") == 1 && strcmp(p.path[0], "Work:Music") == 0);
    CHECK(rp_paths_add(&p, "work:music") == 0 && p.n == 1); /* same drawer, other case */
    CHECK(rp_paths_add(&p, "Work:") == 1 && strcmp(p.path[1], "Work:") == 0); /* a colon stays */
    CHECK(rp_paths_add(&p, "Ram:a.txt") == 1 && p.n == 3);
    rp_paths_remove(&p, 1);
    CHECK(p.n == 2 && strcmp(p.path[1], "Ram:a.txt") == 0 && strcmp(p.path[0], "Work:Music") == 0);
    rp_paths_remove(&p, 9);
    CHECK(p.n == 2);
    CHECK(rp_paths_find(&p, "Ram:a.txt") == 1 && rp_paths_find(&p, "Nope:") == -1);
    rp_paths_replace(&p, 0, "Work:Music/Inner");
    CHECK(strcmp(p.path[0], "Work:Music/Inner") == 0);
    rp_paths_remove(&p, 1);
    rp_paths_remove(&p, 0);
    CHECK(p.n == 0);
    memset(big, 'a', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    CHECK(rp_paths_add(&p, big) == -1);
    for (i = 0; i < RP_PATHS_MAX; i++) {
        char t[16];
        snprintf(t, sizeof t, "Dh0:d%d", i);
        CHECK(rp_paths_add(&p, t) == 1);
    }
    CHECK(rp_paths_add(&p, "Dh0:extra") == -1 && p.n == RP_PATHS_MAX);
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("test_paths_list: ok\n");
    return 0;
}
