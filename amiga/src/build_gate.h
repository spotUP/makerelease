#ifndef RP_BUILD_GATE_H
#define RP_BUILD_GATE_H

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

#endif
