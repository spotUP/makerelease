#include "build_gate.h"

#include <stddef.h>

/* FNV-1a over 32 bits (kept to 32 so the m68k and host hashes agree); a 0x1F byte ends each field so
 * "ab","c" and "a","bc" differ. */
unsigned long rp_gate_add(unsigned long h, const char *s) {
    if (s != NULL) {
        for (; *s != '\0'; s++) h = ((h ^ (unsigned char)*s) * 16777619UL) & 0xFFFFFFFFUL;
    }
    return ((h ^ 0x1FUL) * 16777619UL) & 0xFFFFFFFFUL;
}

void rp_gate_mark(rp_build_gate *gate, unsigned long state) {
    gate->built = 1;
    gate->hash = state;
}

void rp_gate_clear(rp_build_gate *gate) { gate->built = 0; }

int rp_gate_open(const rp_build_gate *gate, unsigned long state) { return gate->built && gate->hash == state; }
