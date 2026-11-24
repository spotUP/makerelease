#ifndef RP_PATHS_LIST_H
#define RP_PATHS_LIST_H

#include <stddef.h>

#define RP_PATHS_MAX 16
#define RP_PATH_CAP 512

/* The files and drawers picked on the Files tab (desktop renderSelectedFiles): the order is the pick order. */
typedef struct {
    char path[RP_PATHS_MAX][RP_PATH_CAP];
    size_t n;
} rp_paths;

/* Adds a path; a trailing slash after a drawer name is dropped (a colon is kept). Returns 1 added, 0 empty or already listed
 * (compared case-insensitively, like the Amiga filesystem), -1 list full or path too long. */
int rp_paths_add(rp_paths *p, const char *path);
/* Removes entry i; an index out of range does nothing. */
void rp_paths_remove(rp_paths *p, size_t i);
/* Replaces entry i (the nested-folder fix); an index out of range does nothing. */
void rp_paths_replace(rp_paths *p, size_t i, const char *path);
/* The index of a path (exact match), or -1. */
int rp_paths_find(const rp_paths *p, const char *path);

#endif
