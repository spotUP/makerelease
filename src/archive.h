#ifndef RP_ARCHIVE_H
#define RP_ARCHIVE_H

#include <stddef.h>

/* The archiver command line, run with the staged outer drawer current:
 * lha a "<archive>" "file_id.diz" "<base>", or zip -r for ZIP.
 * Caller frees. NULL on OOM. */
char *rp_archive_cmd(const char *bin, const char *archive, const char *base);

/* Amiga only (archive_run.c): runs cmd with the given drawer current.
 * Returns 0, or -1 with err set (launch failure or nonzero return code). */
int rp_run_command(const char *workdir, const char *cmd, char *err, size_t errcap);

#endif
