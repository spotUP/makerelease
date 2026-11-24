/* Group editing core: serialize, save, delete and duplicate a group drawer
 * with the desktop rules (app/src/main/profiles.ts saveProfile/deleteProfile,
 * app/src/shared/profile.ts serializeProfile). No UI. A refused call leaves
 * the groups folder as it was. All errors are written to err. */
#ifndef RP_PROFILE_SAVE_H
#define RP_PROFILE_SAVE_H

#include <stddef.h>

#include "groups.h"

/* profile.json text exactly as serializeProfile writes it: fixed key order,
 * two-space indent, trailing newline, id not written. 0 or -1. */
int rp_group_serialize(const rp_group *g, char *out, size_t cap, char *err, size_t errcap);

/* Validates g (by parsing the serialized text with rp_parse_group), then
 * writes dir/<id>/profile.json via profile.json.tmp + rename. diz_or_null /
 * nfo_or_null: template text written as-is (Latin-1 bytes) to file_id.diz /
 * release.nfo; NULL leaves the file alone (an empty nfo writes no release.nfo); remove_nfo deletes release.nfo.
 * is_new requires the drawer to be absent, otherwise it must exist. */
int rp_group_save(const char *dir, const rp_group *g, int is_new, const char *diz_or_null, const char *nfo_or_null,
                  int remove_nfo, char *err, size_t errcap);

/* Deletes the drawer and its files. Refuses the last group. */
int rp_group_delete(const char *dir, const char *id, char *err, size_t errcap);

/* Copies profile.json (id is the drawer name, so nothing to rewrite) and the
 * template files into a new drawer new_id. Refuses an existing new_id. */
int rp_group_duplicate(const char *dir, const char *from_id, const char *new_id, char *err, size_t errcap);

#endif
