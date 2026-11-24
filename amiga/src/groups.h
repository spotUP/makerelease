/* Group drawers, shared byte for byte with the desktop release baker: the
 * same profile.json and the same template files, so a drawer copies straight
 * across. Validation mirrors the desktop messages (file and field named).
 * Amiga-frugal fixed caps, strictly enforced: overlong values are an error,
 * never a silent truncation of release-critical bytes. */
#ifndef RP_GROUPS_H
#define RP_GROUPS_H

#include <stddef.h>

#define RP_ID_MAX 31
#define RP_LABEL_MAX 63
#define RP_URL_MAX 127
#define RP_PREFIX_MAX 15
#define RP_BASE_MAX 127
#define RP_KEY_MAX 127
#define RP_NICK_MAX 31
#define RP_ROLE_MAX 31
#define RP_MEMBERS_MAX 64

typedef enum { R_UPROUGH, R_XINNOVATIONS, R_PLACEHOLDER } rp_renderer;

typedef struct {
    char nickname[RP_NICK_MAX + 1];
    char role[RP_ROLE_MAX + 1];
} rp_member;

typedef struct {
    char id[RP_ID_MAX + 1];
    char label[RP_LABEL_MAX + 1];
    char url[RP_URL_MAX + 1];
    char file_prefix[RP_PREFIX_MAX + 1];
    int code_length;
    rp_renderer renderer;
    int backend_doorserver;
    char server_base[RP_BASE_MAX + 1];
    char server_key[RP_KEY_MAX + 1];
    rp_member members[RP_MEMBERS_MAX];
    size_t nmembers;
} rp_group;

/* 1 when s is a valid drawer name: a-z, 0-9, "-", at most RP_ID_MAX. */
int rp_is_group_id(const char *s);

/* Drawer names under dir (skips dot files), sorted, up to max. Returns the
 * count, or -1 with err set. */
int rp_list_groups(const char *dir, char ids[][RP_ID_MAX + 1], int max, char *err, size_t errcap);

/* The same validation over profile.json text already in memory; file is only
 * the name shown in messages. Returns 0, or -1 with err naming file+field. */
int rp_parse_group(const char *text, const char *file, const char *id, rp_group *g, char *err, size_t errcap);

/* profile.json of one group. Returns 0, or -1 with err naming file+field. */
int rp_load_group(const char *dir, const char *id, rp_group *g, char *err, size_t errcap);

/* A template file as Latin-1 bytes with line endings normalised to LF.
 * optional && missing returns *out NULL and 0. Returns 0 ok, -1 on error. */
int rp_read_template(const char *path, int optional, char **out, char *err, size_t errcap);

#endif
