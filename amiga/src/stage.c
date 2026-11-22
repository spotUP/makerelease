#include "stage.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __amigaos__
#include <proto/dos.h>
#endif

int rp_write_file(const char *path, const void *data, size_t len, char *err, size_t errcap) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        snprintf(err, errcap, "%s: could not be written", path);
        return -1;
    }
    if (len > 0 && fwrite(data, 1, len, f) != len) {
        snprintf(err, errcap, "%s: could not be written", path);
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

int rp_make_dirs(const char *path, char *err, size_t errcap) {
    char tmp[1024], *p;
    struct stat st;
    if (strlen(path) >= sizeof(tmp)) {
        snprintf(err, errcap, "%s: path too long", path);
        return -1;
    }
    strcpy(tmp, path);
    /* Every segment: failures are ignored, so existing drawers (and Amiga
     * volume roots like T:) pass through; the final check decides. */
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode))) {
        snprintf(err, errcap, "%s: could not be created", path);
        return -1;
    }
    return 0;
}

static int is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int copy_file(const char *src, const char *dst, char *err, size_t errcap) {
    FILE *in = fopen(src, "rb");
    FILE *out;
    char buf[8192];
    size_t n;
    if (!in) {
        snprintf(err, errcap, "%s: could not be read", src);
        return -1;
    }
    out = fopen(dst, "wb");
    if (!out) {
        snprintf(err, errcap, "%s: could not be written", dst);
        fclose(in);
        return -1;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            snprintf(err, errcap, "%s: could not be written", dst);
            fclose(in);
            fclose(out);
            return -1;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

int rp_copy_tree(const char *src, const char *dst, char *err, size_t errcap) {
    DIR *d = opendir(src);
    struct dirent *e;
    if (!d) {
        snprintf(err, errcap, "%s: could not be read", src);
        return -1;
    }
    if (rp_make_dirs(dst, err, errcap) != 0) {
        closedir(d);
        return -1;
    }
    while ((e = readdir(d)) != NULL) {
        char s[1024], t[1024];
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        snprintf(s, sizeof(s), "%s/%s", src, e->d_name);
        snprintf(t, sizeof(t), "%s/%s", dst, e->d_name);
        if (is_dir(s)) {
            if (rp_copy_tree(s, t, err, errcap) != 0) {
                closedir(d);
                return -1;
            }
        } else if (copy_file(s, t, err, errcap) != 0) {
            closedir(d);
            return -1;
        }
    }
    closedir(d);
    return 0;
}

int rp_remove_tree(const char *path, char *err, size_t errcap) {
    DIR *d = opendir(path);
    struct dirent *e;
    if (!d) {
        /* A missing tree is already gone. */
        return 0;
    }
    while ((e = readdir(d)) != NULL) {
        char full[1024];
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
        if (is_dir(full)) {
            if (rp_remove_tree(full, err, errcap) != 0) {
                closedir(d);
                return -1;
            }
        } else if (remove(full) != 0) {
            snprintf(err, errcap, "%s: could not be removed", full);
            closedir(d);
            return -1;
        }
    }
    closedir(d);
    if (rmdir(path) != 0) {
        snprintf(err, errcap, "%s: could not be removed", path);
        return -1;
    }
    return 0;
}

int rp_move_dir(const char *src, const char *dst, char *err, size_t errcap) {
    if (rename(src, dst) == 0) return 0;
    /* Across volumes: copy, then remove the source. */
    int existed = is_dir(dst);
    if (rp_copy_tree(src, dst, err, errcap) != 0) {
        /* Drop the half-copied target, but never a directory that was already there. */
        if (!existed) {
            char ignore[64];
            rp_remove_tree(dst, ignore, sizeof(ignore));
        }
        return -1;
    }
    if (rp_remove_tree(src, err, errcap) != 0) return -1;
    return 0;
}
