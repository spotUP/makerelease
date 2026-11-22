#include "archive.h"

#include <stdio.h>

#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>

/* Runs the archiver with workdir current (the staged outer drawer), the
 * Amiga way: swap CurrentDir around SystemTagList, then swap back. */
int rp_run_command(const char *workdir, const char *cmd, char *err, size_t errcap) {
    BPTR lock, old;
    LONG rc;
    lock = Lock((STRPTR)workdir, ACCESS_READ);
    if (!lock) {
        snprintf(err, errcap, "%s: could not be entered", workdir);
        return -1;
    }
    old = CurrentDir(lock);
    rc = SystemTagList((STRPTR)cmd, NULL);
    UnLock(CurrentDir(old));
    if (rc == -1) {
        snprintf(err, errcap, "%s: could not be run (is it in PATH?)", cmd);
        return -1;
    }
    if (rc != 0) {
        snprintf(err, errcap, "archiver failed (return code %ld)", (long)rc);
        return -1;
    }
    return 0;
}
