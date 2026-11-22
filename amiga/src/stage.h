#ifndef RP_STAGE_H
#define RP_STAGE_H

#include <stddef.h>

int rp_write_file(const char *path, const void *data, size_t len, char *err, size_t errcap);
int rp_make_dirs(const char *path, char *err, size_t errcap);
int rp_copy_tree(const char *src, const char *dst, char *err, size_t errcap);
int rp_remove_tree(const char *path, char *err, size_t errcap);
/* rename, with a copy-and-delete fallback across volumes. */
int rp_move_dir(const char *src, const char *dst, char *err, size_t errcap);

#endif
