#include "phase_progress.h"

#include <stdio.h>

int rp_phase_percent(int step, int total) {
    int p;
    if (total < 1) return 0;
    p = (step - 1) * 100 / total;
    return p < 0 ? 0 : p > 100 ? 100 : p;
}

void rp_phase_line(int step, int total, const char *text, char *out, size_t cap) {
    snprintf(out, cap, "Step %d of %d: %s", step, total, text);
}
