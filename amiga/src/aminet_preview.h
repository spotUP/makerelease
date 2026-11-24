/* One call for the Aminet tab's preview: the readme text as it would be
 * uploaded plus the list of problems that would block the upload (wraps
 * rp_aminet_render_readme, rp_aminet_normalize_readme, rp_aminet_names and
 * rp_aminet_problems, in the order ui_amiga.c runs them for the upload). */
#ifndef RP_AMINET_PREVIEW_H
#define RP_AMINET_PREVIEW_H

#include "aminet.h"

typedef struct {
    char *readme;           /* normalized readme, malloc'd (rp_aminet_preview_free) */
    char archive[64];       /* lowercased archive name as uploaded */
    char readme_name[64];
    char problems[512];     /* messages joined with "; ", "" when none */
    int n_problems;         /* 0 = may upload */
} rp_aminet_preview;

/* archive_base: the built archive's file name (e.g. "Foo.lha"). Returns 0
 * (readme set; check n_problems), or -1 with out->problems explaining (OOM or
 * name too long) and readme NULL. */
int rp_aminet_make_preview(const char *archive_base, const rp_aminet_readme_in *in, rp_aminet_preview *out);
void rp_aminet_preview_free(rp_aminet_preview *p);

/* The preview as one text for display: "Problems: ..." or "No problems." and a blank line, then the readme. malloc'd. */
char *rp_aminet_preview_view(const rp_aminet_preview *p);

#endif
