#ifndef RP_ARCHIVE_H
#define RP_ARCHIVE_H

#include <stddef.h>

/* The archiver command line, run in the staged drawer that holds file_id.diz,
 * <base>.NFO (with_nfo) and the <base> drawer with the files, so the archive
 * root lists those three: lha -r a "<archive>" "file_id.diz" "<base>.NFO"
 * "<base>", or zip -r for ZIP. Caller frees. NULL on OOM. */
char *rp_archive_cmd(const char *bin, const char *archive, const char *base, int with_nfo);

/* Amiga only (archive_run.c): runs cmd with the given drawer current.
 * Returns 0, or -1 with err set (launch failure or nonzero return code). */
int rp_run_command(const char *workdir, const char *cmd, char *err, size_t errcap);

#endif
