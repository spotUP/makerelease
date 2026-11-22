#include "date.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK_STR(a, b) do { \
    if (strcmp((a), (b)) != 0) { printf("FAIL %d: got [%s] expect [%s]\n", __LINE__, (a), (b)); failures++; } \
} while (0)

int main(void) {
    char out[16];
    rp_iso_date(0, out, sizeof(out));
    CHECK_STR(out, "1970-01-01");
    rp_iso_date(10957, out, sizeof(out));
    CHECK_STR(out, "2000-01-01");
    rp_iso_date(20777, out, sizeof(out));
    CHECK_STR(out, "2026-11-20");
    rp_iso_date(2922, out, sizeof(out));
    CHECK_STR(out, "1978-01-01");
    if (failures == 0) puts("date: all passed");
    return failures != 0;
}
