#ifndef RP_ARCHIVE_H
#define RP_ARCHIVE_H

#include <stddef.h>

/* The archiver command line, run in the drawer that holds the staged <base>
 * drawer (which holds file_id.diz, the NFO and the files, so the archive root
 * is <base>/): lha -r a "<archive>" "<base>", or zip -r for ZIP.
 * Caller frees. NULL on OOM. */
char *rp_archive_cmd(const char *bin, const char *archive, const char *base);

/* Amiga only (archive_run.c): runs cmd with the given drawer current.
 * Returns 0, or -1 with err set (launch failure or nonzero return code). */
int rp_run_command(const char *workdir, const char *cmd, char *err, size_t errcap);

#endif
