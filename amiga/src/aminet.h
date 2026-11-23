/* Aminet upload, mirroring app/src/main/aminet.ts + shared/aminet-readme.ts:
 * anonymous FTP to main.aminet.net/new/, the uploader's email as the
 * password, archive first then its .readme. Plain TCP (no TLS), so it runs
 * over an abstract socket seam (rp_tcp_ops) that host tests fake. */
#ifndef RP_AMINET_H
#define RP_AMINET_H

#include <stddef.h>

/* TCP seam. open returns a handle >= 0 or -1 (err filled); recv returns
 * bytes read, 0 at EOF, -1 on error. */
typedef struct {
    int (*open)(void *ctx, const char *host, int port, char *err, size_t errcap);
    int (*send)(void *ctx, int h, const char *data, size_t len);
    int (*recv)(void *ctx, int h, char *buf, size_t cap);
    void (*close)(void *ctx, int h);
} rp_tcp_ops;

#ifdef __amigaos__
extern const rp_tcp_ops rp_tcp_amiga_ops;   /* in http_amiga.c, ctx unused */
#endif

#define RP_AMINET_HOST "main.aminet.net"
#define RP_AMINET_DIR "/new/"
#define RP_AMINET_SHORT_MAX 40
#define RP_AMINET_LINE_MAX 78
#define RP_AMINET_FILENAME_MAX 30

/* aminetFileNames: archive lowercased, readme = same stem + ".readme". 0, or -1 if cap too small. */
int rp_aminet_names(const char *archive_base, char *archive, char *readme, size_t cap);
/* normalizeReadmeText for a Latin-1 input: CR/CRLF -> LF, non-ASCII folded (ae oe aa ue ...,
 * '?' otherwise), one trailing newline. malloc'd; caller frees. NULL on OOM. */
char *rp_aminet_normalize_readme(const char *latin1);
/* aminetFileNameProblems + aminetReadmeProblems: messages joined with "; " into msg.
 * Returns the number of problems (0 = may upload). */
int rp_aminet_problems(const char *archive, const char *readme, const char *readme_text, char *msg, size_t cap);

typedef struct { const char *role, *name; } rp_aminet_credit;
typedef struct {
    const char *short_text, *type, *architecture, *uploader_email, *uploader_name;
    const char *group_label, *group_url, *entry_creators, *description, *release_notes;
    const rp_aminet_credit *credits; size_t n_credits;
} rp_aminet_readme_in;
/* renderAminetReadme: header block, blank line, wrapped body (78 columns, LF). malloc'd; NULL on OOM. */
char *rp_aminet_render_readme(const rp_aminet_readme_in *in);

/* uploadToAminet: validates, then uploads archive and readme in one FTP session. data/len = the
 * built archive bytes; readme_text = the already normalized readme. Returns 0 on success, -1
 * otherwise; msg is the status line (desktop wording). Never contacts anything but via ops. */
int rp_aminet_upload(const rp_tcp_ops *ops, void *ctx, const char *archive_base, const char *uploader_email,
                     const char *readme_text, const char *data, size_t len, char *msg, size_t cap);

#endif
