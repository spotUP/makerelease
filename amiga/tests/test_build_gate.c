#include "build_gate.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

static unsigned long state(const char *a, const char *b) {
    return rp_gate_add(rp_gate_add(RP_GATE_BEGIN, a), b);
}

int main(void) {
    rp_build_gate gate = { 0, 0 };
    /* a fresh process (restart) is locked, whatever archive sits on disk */
    CHECK(!rp_gate_open(&gate, state("Name", "ABCDE")));
    rp_gate_mark(&gate, state("Name", "ABCDE"));
    CHECK(rp_gate_open(&gate, state("Name", "ABCDE")));
    /* any field changed after Build locks it again */
    CHECK(!rp_gate_open(&gate, state("Name2", "ABCDE")));
    CHECK(!rp_gate_open(&gate, state("Name", "ABCDF")));
    /* field boundaries count */
    CHECK(state("ab", "c") != state("a", "bc"));
    CHECK(rp_gate_add(RP_GATE_BEGIN, NULL) == rp_gate_add(RP_GATE_BEGIN, ""));
    /* a new Build of the changed form reopens it; clear locks it */
    rp_gate_mark(&gate, state("Name2", "ABCDE"));
    CHECK(rp_gate_open(&gate, state("Name2", "ABCDE")));
    rp_gate_clear(&gate);
    CHECK(!rp_gate_open(&gate, state("Name2", "ABCDE")));
    {   /* Build needs a Description and Notes, like the desktop's required fields */
        char err[128];
        CHECK(rp_build_required("A demo.", "Notes line", err, sizeof err) == 0);
        CHECK(rp_build_required("", "Notes line", err, sizeof err) == RP_REQUIRED_DESCRIPTION);
        CHECK(strstr(err, "Description") != NULL);
        CHECK(rp_build_required("A demo.", "", err, sizeof err) == RP_REQUIRED_NOTES);
        CHECK(strstr(err, "Notes") != NULL);
        /* whitespace only is blank; NULL is blank; Description is reported first when both are missing */
        CHECK(rp_build_required("  \t", "Notes line", err, sizeof err) == RP_REQUIRED_DESCRIPTION);
        CHECK(rp_build_required("A demo.", " \n \n", err, sizeof err) == RP_REQUIRED_NOTES);
        CHECK(rp_build_required(NULL, NULL, err, sizeof err) == RP_REQUIRED_DESCRIPTION);
        CHECK(rp_build_required(" x ", "\n y", err, sizeof err) == 0);
    }
    {   /* a group without a server needs a typed release number */
        char err[128], out[8];
        CHECK(rp_manual_number("042", out, sizeof out, err, sizeof err) == 0 && strcmp(out, "042") == 0);
        CHECK(rp_manual_number("  7 \t", out, sizeof out, err, sizeof err) == 0 && strcmp(out, "7") == 0);
        CHECK(rp_manual_number("", out, sizeof out, err, sizeof err) == -1 && strstr(err, "Release number") != NULL);
        CHECK(rp_manual_number("  \t", out, sizeof out, err, sizeof err) == -1 && out[0] == '\0');
        CHECK(rp_manual_number(NULL, out, sizeof out, err, sizeof err) == -1);
        CHECK(rp_manual_number("4\x01" "2", out, sizeof out, err, sizeof err) == -1);
        /* a too long number is cut to the buffer, never overrun */
        CHECK(rp_manual_number("123456789", out, sizeof out, err, sizeof err) == 0 && strcmp(out, "1234567") == 0);
    }
    if (failures == 0) printf("build_gate: ok\n");
    return failures != 0;
}
