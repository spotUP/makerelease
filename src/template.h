#ifndef RP_TEMPLATE_H
#define RP_TEMPLATE_H

#include <stddef.h>

/* One {{name}} replacement. */
typedef struct {
    const char *name;
    const char *value;
} rp_scalar;

/* One lines placeholder: the frame repeats once per line. */
typedef struct {
    const char *name;
    const char *const *lines;
    size_t nlines;
} rp_block;

typedef struct {
    const rp_scalar *scalars;
    size_t nscalars;
    const rp_block *blocks;
    size_t nblocks;
} rp_values;

/*
 * Renders a template whose line endings are already LF (see
 * rp_normalise_line_endings). Returns a malloc'd string, or NULL on error
 * with errline set to the 1-based template line and errmsg holding
 * "line L: message". The caller frees both the result and nothing else.
 */
char *rp_render_template(const char *tpl, const rp_values *v, int *errline, char *errmsg, size_t errcap);

/* CRLF and lone CR become LF, in place. Runs when a template is read. */
void rp_normalise_line_endings(char *text);

#endif
