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
    rp_suggest_code("Fleisch Bitte AD 006", 5, out, sizeof(out));
    CHECK_STR(out, "FB006");
    rp_suggest_code("Hypocrisy", 5, out, sizeof(out));
    CHECK_STR(out, "HYPOC");
    rp_suggest_code("Fleisch Bitte 12345678", 5, out, sizeof(out));
    CHECK_STR(out, "45678");
    rp_code_filter("fb-0 06x", 5, out, sizeof(out));
    CHECK_STR(out, "FB006");
    rp_code_filter("ab", 5, out, sizeof(out));
    CHECK_STR(out, "AB");
    rp_code_filter("\xe5" "bc", 5, out, sizeof(out));
    CHECK_STR(out, "BC");
    {
        char near[8][RP_CODE_NEAR_LEN];
        int n = rp_code_near("FB006", near, 4);
        if (n != 4) { printf("FAIL %d: near count %d\n", __LINE__, n); failures++; }
        CHECK_STR(near[0], "FB007");
        CHECK_STR(near[1], "FB005");
        CHECK_STR(near[3], "FB004");
        n = rp_code_near("AB99", near, 3); /* top of the width: only downward */
        if (n != 3) { printf("FAIL %d: near count %d\n", __LINE__, n); failures++; }
        CHECK_STR(near[0], "AB98");
        CHECK_STR(near[2], "AB96");
        n = rp_code_near("HYPO2", near, 2);
        CHECK_STR(near[0], "HYPO3");
        n = rp_code_near("HYPOC", near, 8);
        if (n != 8) { printf("FAIL %d: near count %d\n", __LINE__, n); failures++; }
        CHECK_STR(near[0], "HYPO2");
        CHECK_STR(near[7], "HYPO9");
        n = rp_code_near("A0", near, 8); /* 0 steps up only */
        if (n != 8) { printf("FAIL %d: near count %d\n", __LINE__, n); failures++; }
        CHECK_STR(near[0], "A1");
        CHECK_STR(near[7], "A8");
    }
    if (failures == 0) puts("naming: all passed");
    return failures != 0;
}
