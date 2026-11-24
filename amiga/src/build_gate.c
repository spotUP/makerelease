#include "build_gate.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

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

static int blank(const char *s) {
    if (s == NULL) return 1;
    for (; *s != '\0'; s++) {
        if (!isspace((unsigned char)*s)) return 0;
    }
    return 1;
}

int rp_build_required(const char *description, const char *notes, char *err, size_t errcap) {
    if (blank(description)) {
        snprintf(err, errcap, "Description: enter the description on the Release tab first.");
        return RP_REQUIRED_DESCRIPTION;
    }
    if (blank(notes)) {
        snprintf(err, errcap, "Notes: enter the release notes on the Notes tab first.");
        return RP_REQUIRED_NOTES;
    }
    return 0;
}

int rp_manual_number(const char *text, char *out, size_t outcap, char *err, size_t errcap) {
    size_t n;
    const char *p;
    if (text == NULL) text = "";
    while (isspace((unsigned char)*text)) text++;
    n = strlen(text);
    while (n > 0 && isspace((unsigned char)text[n - 1])) n--;
    if (n >= outcap) n = outcap - 1;
    memcpy(out, text, n);
    out[n] = '\0';
    if (n == 0) {
        snprintf(err, errcap, "Release number: enter the number on the Release tab first.");
        return -1;
    }
    for (p = out; *p != '\0'; p++) {
        if (iscntrl((unsigned char)*p)) {
            snprintf(err, errcap, "Release number: remove the control characters on the Release tab.");
            return -1;
        }
    }
    return 0;
}
