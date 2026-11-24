#include "aminet_preview.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int rp_aminet_make_preview(const char *archive_base, const rp_aminet_readme_in *in, rp_aminet_preview *out) {
    char *raw;
    memset(out, 0, sizeof *out);
    if (rp_aminet_names(archive_base, out->archive, out->readme_name, sizeof out->archive) != 0) {
        snprintf(out->problems, sizeof out->problems, "the archive file name is too long for Aminet");
        out->n_problems = 1;
        return -1;
    }
    raw = rp_aminet_render_readme(in);
    out->readme = raw ? rp_aminet_normalize_readme(raw) : NULL;
    free(raw);
    if (!out->readme) {
        snprintf(out->problems, sizeof out->problems, "the Aminet readme could not be prepared");
        out->n_problems = 1;
        return -1;
    }
    out->n_problems = rp_aminet_problems(out->archive, out->readme_name, out->readme, out->problems, sizeof out->problems);
    return 0;
}

void rp_aminet_preview_free(rp_aminet_preview *p) {
    free(p->readme);
    p->readme = NULL;
}

char *rp_aminet_preview_view(const rp_aminet_preview *p) {
    const char *readme = p->readme ? p->readme : "";
    size_t n = strlen(p->problems) + strlen(readme) + 32;
    char *out = malloc(n);
    if (!out) return NULL;
    if (p->n_problems > 0) snprintf(out, n, "Problems: %s\n\n%s", p->problems, readme);
    else snprintf(out, n, "No problems.\n\n%s", readme);
    return out;
}
