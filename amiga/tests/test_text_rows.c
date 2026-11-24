#include "text_rows.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Row formats mirror memberlist-table.ts getMembers, boards-table.ts
 * boardsFromRows and media-embeds-table.ts getMediaEmbeds/getValidationError. */
static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %d: %s\n", __LINE__, #cond); failures++; } \
} while (0)

#define CHECK_STR(got, want) do { \
    char *g_ = (got); \
    if (!g_ || strcmp(g_, (want)) != 0) { printf("FAIL %d: got [%s] expect [%s]\n", __LINE__, g_ ? g_ : "<null>", (want)); failures++; } \
    free(g_); \
} while (0)

int main(void) {
    rp_row_err err;
    rp_member m[8];
    rp_board_row b[8];
    rp_embed_row e[8];
    rp_rel_board bv[8];
    rp_rel_embed ev[8];
    int n;

    /* members: trimmed, blank lines skipped, same nickname merged, sorted case-insensitive */
    n = rp_parse_member_rows("  Spot | coder\n\nHoff|music, gfx\r\nSpot | Coder, text\nalpha\r", m, 8, &err);
    CHECK(n == 3);
    CHECK(strcmp(m[0].nickname, "alpha") == 0 && m[0].role[0] == '\0');
    CHECK(strcmp(m[1].nickname, "Hoff") == 0 && strcmp(m[1].role, "music, gfx") == 0);
    CHECK(strcmp(m[2].nickname, "Spot") == 0 && strcmp(m[2].role, "coder, text") == 0);
    CHECK(rp_parse_member_rows("", m, 8, &err) == 0);
    CHECK(rp_parse_member_rows(NULL, m, 8, &err) == 0);
    CHECK(rp_parse_member_rows("  |  \n\n", m, 8, &err) == 0);
    /* members: errors carry the 1-based line */
    CHECK(rp_parse_member_rows("a | b\n | coder", m, 8, &err) == -1 && err.line == 2);
    CHECK(rp_parse_member_rows("a | b | c", m, 8, &err) == -1 && err.line == 1);
    CHECK(rp_parse_member_rows("0123456789012345678901234567890123", m, 8, &err) == -1 && err.line == 1);
    CHECK(rp_parse_member_rows("a\nb\nc", m, 2, &err) == -1 && err.line == 3);
    CHECK_STR(rp_format_member_rows(m, 0), "");

    /* members: format round trip, '|' in a value cannot break the row */
    n = rp_parse_member_rows("Hoff | music\nSpot", m, 8, &err);
    CHECK_STR(rp_format_member_rows(m, (size_t)n), "Hoff | music\nSpot\n");
    strcpy(m[0].role, "a|b");
    CHECK_STR(rp_format_member_rows(m, 1), "Hoff | a/b\n");

    /* boards: only the name is required, order kept, empty rows skipped */
    n = rp_parse_board_rows("uPTOWN | sPOT | wEBHQ | SOON\n\nOther\n Third | | | ACTIVE", b, 8, &err);
    CHECK(n == 3);
    CHECK(strcmp(b[0].name, "uPTOWN") == 0 && strcmp(b[0].sysop, "sPOT") == 0 && strcmp(b[0].hq, "wEBHQ") == 0 && strcmp(b[0].status, "SOON") == 0);
    CHECK(strcmp(b[1].name, "Other") == 0 && b[1].sysop[0] == '\0' && b[1].status[0] == '\0');
    CHECK(strcmp(b[2].name, "Third") == 0 && b[2].hq[0] == '\0' && strcmp(b[2].status, "ACTIVE") == 0);
    CHECK(rp_parse_board_rows("| sysop", b, 8, &err) == -1 && err.line == 1);
    CHECK(rp_parse_board_rows("a|b|c|d|e", b, 8, &err) == -1);
    CHECK(rp_parse_board_rows("ok\nx|0123456789012345678901234567890123456789012345678901234567890123456789", b, 8, &err) == -1 && err.line == 2);
    n = rp_parse_board_rows("A | s | h | st\nB", b, 8, &err);
    rp_board_rows_view(b, (size_t)n, bv);
    CHECK(strcmp(bv[0].hq, "h") == 0 && strcmp(bv[1].name, "B") == 0);
    CHECK_STR(rp_format_board_rows(bv, (size_t)n), "A | s | h | st\nB\n");
    bv[0].hq = "";
    bv[0].status = "x";
    CHECK_STR(rp_format_board_rows(bv, 1), "A | s |  | x\n");
    bv[0].sysop = NULL;
    bv[0].status = NULL;
    CHECK_STR(rp_format_board_rows(bv, 1), "A\n");

    /* embeds: type case-insensitive, caption optional, empty youtube skipped, image needs a path */
    n = rp_parse_embed_rows("YouTube | Trailer | https://youtu.be/x\nyoutube | none |\nyoutube\nimage | Cover | Work:pic.png\nimage||RAM:a.png", e, 8, &err);
    CHECK(n == 3);
    CHECK(!e[0].is_image && strcmp(e[0].caption, "Trailer") == 0 && strcmp(e[0].url, "https://youtu.be/x") == 0 && e[0].path[0] == '\0');
    CHECK(e[1].is_image && strcmp(e[1].path, "Work:pic.png") == 0 && e[1].url[0] == '\0');
    CHECK(e[2].is_image && e[2].caption[0] == '\0' && strcmp(e[2].path, "RAM:a.png") == 0);
    CHECK(rp_parse_embed_rows("image | Cover", e, 8, &err) == -1 && err.line == 1 && strstr(err.msg, "image file") != NULL);
    CHECK(rp_parse_embed_rows("youtube | a | b\nvimeo | a | b", e, 8, &err) == -1 && err.line == 2);
    CHECK(rp_parse_embed_rows("youtube | a | b | c", e, 8, &err) == -1);
    CHECK(rp_parse_embed_rows("\n\n", e, 8, &err) == 0);
    n = rp_parse_embed_rows("youtube | T | u\nimage | C | p", e, 8, &err);
    rp_embed_rows_view(e, (size_t)n, ev);
    CHECK(strcmp(ev[0].type, "youtube") == 0 && strcmp(ev[0].youtube_url, "u") == 0 && strcmp(ev[1].type, "image") == 0 && ev[1].youtube_url == NULL);
    CHECK_STR(rp_format_embed_rows(e, (size_t)n), "youtube | T | u\nimage | C | p\n");
    e[0].caption[0] = '\0';
    CHECK_STR(rp_format_embed_rows(e, 1), "youtube |  | u\n");

    /* picked image: appended as its own row, parses back to the same path */
    CHECK_STR(rp_embed_append_image("", "Work:Pics/cover.png"), "image | cover.png | Work:Pics/cover.png\n");
    CHECK_STR(rp_embed_append_image(NULL, "RAM:a.png"), "image | a.png | RAM:a.png\n");
    CHECK_STR(rp_embed_append_image("youtube | T | u\n", "RAM:a.png"), "youtube | T | u\nimage | a.png | RAM:a.png\n");
    CHECK_STR(rp_embed_append_image("youtube | T | u", "RAM:a.png"), "youtube | T | u\nimage | a.png | RAM:a.png\n");
    CHECK(rp_embed_append_image("", "") == NULL && rp_embed_append_image("", NULL) == NULL);
    CHECK(rp_embed_append_image("", "RAM:a|b.png") == NULL && rp_embed_append_image("", "RAM:a\nb.png") == NULL);
    {
        char longp[RP_EMBED_PATH_MAX + 3];
        char *t;
        memset(longp, 'a', sizeof longp);
        longp[RP_EMBED_PATH_MAX + 1] = '\0';
        CHECK(rp_embed_append_image("", longp) == NULL);
        longp[RP_EMBED_PATH_MAX] = '\0'; /* longest accepted path; its caption is cut to the caption limit */
        t = rp_embed_append_image("", longp);
        CHECK(t != NULL && rp_parse_embed_rows(t, e, 8, &err) == 1 && strcmp(e[0].path, longp) == 0 && strlen(e[0].caption) == RP_EMBED_CAPTION_MAX);
        free(t);
    }
    {
        char *t = rp_embed_append_image("", "Work:Pics/cover.png");
        CHECK(t != NULL && rp_parse_embed_rows(t, e, 8, &err) == 1 && e[0].is_image && strcmp(e[0].path, "Work:Pics/cover.png") == 0);
        free(t);
    }

    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("test_text_rows ok\n");
    return 0;
}
