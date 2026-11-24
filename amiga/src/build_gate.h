#ifndef RP_BUILD_GATE_H
#define RP_BUILD_GATE_H

#include <stddef.h>

/* Publish and Aminet upload unlock only after a Build of the CURRENT form (desktop wizard.ts: Publish unlocks
 * after Build). The form state is hashed field by field; Build stores the hash, Publish compares. The gate is
 * in memory only, so a restart locks it again. */
typedef struct {
    int built;
    unsigned long hash;
} rp_build_gate;

#define RP_GATE_BEGIN 2166136261UL

/* Folds s (NULL counts as "") and a field separator into h; returns the new hash. */
unsigned long rp_gate_add(unsigned long h, const char *s);

void rp_gate_mark(rp_build_gate *gate, unsigned long state);
void rp_gate_clear(rp_build_gate *gate);
/* 1 when a Build finished for exactly this state. */
int rp_gate_open(const rp_build_gate *gate, unsigned long state);

/* What Build refuses to run without (desktop index.html: the Description input and the NFO text area are "required",
 * main.ts nativeFieldErrors words it "<Label> is required."). Blank, whitespace only included, counts as missing.
 * 0 when both are filled; RP_REQUIRED_DESCRIPTION or RP_REQUIRED_NOTES (Description first, the page order) with the
 * refusal text in err. */
#define RP_REQUIRED_DESCRIPTION 1
#define RP_REQUIRED_NOTES 2
int rp_build_required(const char *description, const char *notes, char *err, size_t errcap);

#endif
