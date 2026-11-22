#include "naming.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK_STR(a, b) do { \
    if (strcmp((a), (b)) != 0) { printf("FAIL %d: got [%s] expect [%s]\n", __LINE__, (a), (b)); failures++; } \
} while (0)

int main(void) {
    char out[64];
    rp_base_name("UP-", "hypoc", out, sizeof(out));
    CHECK_STR(out, "UP-HYPOC");
    rp_archive_name("EX-", "TEST1", 1, out, sizeof(out));
    CHECK_STR(out, "EX-TEST1.LHA");
    rp_archive_name("", "abc12", 0, out, sizeof(out));
    CHECK_STR(out, "ABC12.ZIP");
    rp_upper("hypoc-2", out, sizeof(out));
    CHECK_STR(out, "HYPOC-2");
    rp_lower("UP-HYPOC.LHA", out, sizeof(out));
    CHECK_STR(out, "up-hypoc.lha");
    if (failures == 0) puts("naming: all passed");
    return failures != 0;
}
