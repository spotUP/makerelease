#ifndef RP_DIVISIONS_H
#define RP_DIVISIONS_H

#include <stddef.h>

#include "groups.h"

/* The two divisions a group releases under (desktop divisions.ts): which diz art and which rows of it the form edits. A group is the
 * /X Innovations division when its renderer is X Innovations, else Up Rough; a placeholder group has no banner art, so no rows. */
typedef struct {
    int row;           /* editableDizRows[].row, zero-based */
    const char *label; /* editableDizRows[].label */
} rp_diz_row;

typedef struct {
    const char *banner; /* defaultDizBanner */
    const rp_diz_row *rows;
    size_t nrows;
} rp_division;

/* The division of group g, or NULL for a placeholder group. */
const rp_division *rp_division_for(const rp_group *g);

#endif
