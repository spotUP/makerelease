#ifndef RP_PHASE_PROGRESS_H
#define RP_PHASE_PROGRESS_H

#include <stddef.h>

/* Build and Preview (dry run) go through RP_PHASES steps; the gauge on the status row shows how many have been started. */
#define RP_PHASES 5

/* Percent done when step (1-based) starts: (step - 1) of total finished. Out of range is clamped to 0..100; total < 1 gives 0. */
int rp_phase_percent(int step, int total);
/* The stage line: "Step 2 of 5: Writing the NFO...". */
void rp_phase_line(int step, int total, const char *text, char *out, size_t cap);

#endif
