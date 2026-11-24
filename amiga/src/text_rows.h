#ifndef RP_TEXT_ROWS_H
#define RP_TEXT_ROWS_H

#include <stddef.h>

#include "groups.h"
#include "releases_client.h"

/* Text-row entry for the three desktop tables (memberlist-table.ts,
 * boards-table.ts, media-embeds-table.ts): one entry per line, fields split
 * by '|' and trimmed. Blank lines are skipped; LF, CRLF and CR all end a line.
 * Parsers fill caller arrays (no heap) and return the row count, or -1 with
 * err set (1-based line number of the bad row plus a message). A field that
 * does not fit its buffer is an error, never silently cut. A row a desktop
 * table would drop (empty) is skipped the same way. Formatters return a
 * malloc'd text for prefilling the field (NULL on out of memory); '|' and
 * line breaks inside a value are replaced by '/' and ' '. */

typedef struct {
    size_t line;
    char msg[96];
} rp_row_err;

/* ---- members: "Nickname | role" (role optional) ----
 * Like MemberlistTable.getMembers: rows with the same nickname merge their
 * skills (rp_merge_member_skills) and the result is sorted by nickname,
 * case-insensitive. A role without a nickname is an error (the desktop drops
 * such a row; in a text field that is a typo worth reporting). */
int rp_parse_member_rows(const char *text, rp_member *out, size_t max, rp_row_err *err);
char *rp_format_member_rows(const rp_member *members, size_t n);

/* ---- boards: "Name | Sysop | HQ | Status" (all but Name optional) ----
 * Like boardsFromRows: a row counts if it has a name; entry order is kept. */
#define RP_BOARD_NAME_MAX 63
#define RP_BOARD_SYSOP_MAX 63
#define RP_BOARD_HQ_MAX 127
#define RP_BOARD_STATUS_MAX 63
typedef struct {
    char name[RP_BOARD_NAME_MAX + 1];
    char sysop[RP_BOARD_SYSOP_MAX + 1];
    char hq[RP_BOARD_HQ_MAX + 1];
    char status[RP_BOARD_STATUS_MAX + 1];
} rp_board_row;
int rp_parse_board_rows(const char *text, rp_board_row *out, size_t max, rp_row_err *err);
/* Pointer view for rp_rel_form / rp_x_board; out[i] points into rows. */
void rp_board_rows_view(const rp_board_row *rows, size_t n, rp_rel_board *out);
/* Formats seed boards (rp_seed_info.boards) as well as parsed ones via the view. */
char *rp_format_board_rows(const rp_rel_board *boards, size_t n);

/* ---- media embeds: "youtube | caption | url" or "image | caption | path" ----
 * Type is case-insensitive. Like getMediaEmbeds: a youtube row without a URL
 * is skipped; caption may be empty. Like getValidationError: an image row
 * without a file path is an error. Order is kept, so the image rows' paths
 * (embeds with is_image) match the server's positional embedImage upload. */
#define RP_EMBED_CAPTION_MAX 127
#define RP_EMBED_URL_MAX 255
#define RP_EMBED_PATH_MAX 255
typedef struct {
    int is_image;                          /* 0 = youtube */
    char caption[RP_EMBED_CAPTION_MAX + 1];
    char url[RP_EMBED_URL_MAX + 1];        /* youtube only, else "" */
    char path[RP_EMBED_PATH_MAX + 1];      /* image only, else "" */
} rp_embed_row;
int rp_parse_embed_rows(const char *text, rp_embed_row *out, size_t max, rp_row_err *err);
/* rp_rel_form view; strings point into rows. */
void rp_embed_rows_view(const rp_embed_row *rows, size_t n, rp_rel_embed *out);
char *rp_format_embed_rows(const rp_embed_row *rows, size_t n);
/* Field text plus one "image | <file name> | <path>" row for a picked image file (the file requester's result), as a new
 * malloc'd text; the row starts on its own line. NULL for an empty path, a path with '|' or a line break (the row
 * would not parse back to it), a path over RP_EMBED_PATH_MAX, or out of memory. The caption is the file name,
 * cut to RP_EMBED_CAPTION_MAX. */
char *rp_embed_append_image(const char *text, const char *path);

/* ---- row table: a rows text held as one raw line per row, for the list editors ----
 * Row i is text line i + 1, blank rows included, so a parser's error line names the row. The
 * text goes through the same parsers as before; the table only splits and joins lines. */
typedef struct {
    int ncols; /* the last column shows the rest of the line, extra '|' fields included */
    size_t n, cap;
    char **row;
} rp_rtab;
/* Replaces the rows with the lines of text (LF, CRLF or CR). 0, or -1 out of memory (rows emptied). */
int rp_rtab_set_text(rp_rtab *t, const char *text);
/* The rows joined, one per line, as a malloc'd text ("" for none); NULL out of memory. */
char *rp_rtab_text(const rp_rtab *t);
/* Column col of row as trimmed text, cut to cap - 1 bytes; "" past the row's fields. */
void rp_rtab_cell(const rp_rtab *t, size_t row, int col, char *out, size_t cap);
/* Rewrites row from ncols column texts ('|' becomes '/', line breaks spaces, trailing empty
 * columns dropped). 0, or -1 for a bad row or out of memory (row unchanged). */
int rp_rtab_set_row(rp_rtab *t, size_t row, const char *const *v);
/* Appends an empty row; its index, or -1 out of memory. */
int rp_rtab_add(rp_rtab *t);
void rp_rtab_remove(rp_rtab *t, size_t row);
void rp_rtab_free(rp_rtab *t);

#endif
