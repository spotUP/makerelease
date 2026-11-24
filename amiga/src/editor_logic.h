/* Pure helpers of the group editor window (ui_editor.c): text field <-> rp_group
 * conversions and the checks the core does not word for a form. No UI, no OS. */
#ifndef RP_EDITOR_LOGIC_H
#define RP_EDITOR_LOGIC_H

#include <stddef.h>

#include "groups.h"

/* Copies src into dst (cap bytes) with surrounding blanks trimmed. Too long: -1 with "<name> is too long (at most N characters)." in err; dst untouched. */
int rp_editor_set_field(char *dst, size_t cap, const char *src, const char *name, char *err, size_t errcap);

/* The Code length field: a whole number 1..32. 0 with *out set, or -1 with err. */
int rp_editor_code_length(const char *s, int *out, char *err, size_t errcap);

/* A new group's id: rp_is_group_id with a message naming the rule. 0 or -1 with err. */
int rp_editor_check_id(const char *s, char *err, size_t errcap);

/* The members text area (one "Nickname | role" per line, the Roster tab format) into out[0..*n). 0 or -1 with "Members: Line N: msg". */
int rp_editor_members_parse(const char *text, rp_member *out, size_t max, size_t *n, char *err, size_t errcap);

/* Members as text area text. 0 or -1 when it does not fit cap (cap includes the NUL). */
int rp_editor_members_format(const rp_member *m, size_t n, char *out, size_t cap);

#endif
