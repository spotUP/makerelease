#ifndef RP_LAST_CHOICE_H
#define RP_LAST_CHOICE_H

#include "groups.h"

/* The group and archive format open when the packer last ran (desktop main.ts remembers both in localStorage), so the next start opens
 * on them. The file holds two lines: the group id, then LHA or ZIP. */
typedef struct {
    char group[RP_ID_MAX + 1];
    int zip; /* 1 = ZIP, 0 = LHA */
} rp_last_choice;

/* 0 loaded; -1 when the file is missing or has no group line (c is then empty: no group, LHA). */
int rp_last_choice_load(const char *path, rp_last_choice *c);
/* 0 saved; -1 when the file cannot be written. */
int rp_last_choice_save(const char *path, const rp_last_choice *c);

#endif
