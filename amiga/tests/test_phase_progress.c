#include "phase_progress.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

int main(void) {
    char line[48], small[8];
    CHECK(rp_phase_percent(1, 5) == 0 && rp_phase_percent(2, 5) == 20 && rp_phase_percent(5, 5) == 80);
    CHECK(rp_phase_percent(6, 5) == 100);
    CHECK(rp_phase_percent(0, 5) == 0 && rp_phase_percent(-3, 5) == 0 && rp_phase_percent(3, 0) == 0);
    CHECK(rp_phase_percent(99, 5) == 100);
    rp_phase_line(2, RP_PHASES, "Writing the NFO...", line, sizeof line);
    CHECK(strcmp(line, "Step 2 of 5: Writing the NFO...") == 0);
    rp_phase_line(1, 5, "Staging", small, sizeof small);
    CHECK(strcmp(small, "Step 1 ") == 0);
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("test_phase_progress: ok\n");
    return 0;
}
