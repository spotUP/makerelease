#ifndef RP_LAST_CHOICE_H
#define RP_LAST_CHOICE_H

#include "groups.h"

#define RP_LAST_FMT_MAX 32

/* The group and archive format open when the packer last ran (desktop main.ts remembers both in localStorage), so the next start opens
 * on them. The file holds the group id, then LHA or ZIP (the open group's format; older files stop here), then one "<group id> LHA|ZIP"
 * line per group that ever picked one: the format is a per-group habit like the desktop's archive-format map. */
typedef struct {
    char group[RP_ID_MAX + 1];
    int zip; /* 1 = ZIP, 0 = LHA */
    int nfmt;
    char fmt_id[RP_LAST_FMT_MAX][RP_ID_MAX + 1];
    int fmt_zip[RP_LAST_FMT_MAX];
} rp_last_choice;

/* The format remembered for group id: 1 and *zip set when there is one, 0 when the group never picked. */
int rp_last_format_get(const rp_last_choice *c, const char *id, int *zip);
/* Remember zip for group id (replaces its entry; a full table drops the new group). */
void rp_last_format_set(rp_last_choice *c, const char *id, int zip);

/* 0 loaded; -1 when the file is missing or has no group line (c is then empty: no group, LHA). */
int rp_last_choice_load(const char *path, rp_last_choice *c);
/* 0 saved; -1 when the file cannot be written. */
int rp_last_choice_save(const char *path, const rp_last_choice *c);

#endif
