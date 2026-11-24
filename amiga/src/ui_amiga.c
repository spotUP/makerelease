/* Release Packer MUI shell. A plain MUI 3.8 program built against the UP-MCC
 * release headers only (vendor/up-mcc): every class opens by file name, so
 * the code here holds no class implementation. Needs UP-MCC 1.0 in
 * MUI:Libs/mui (UP-<Name>.mcc) with up-mcc.library in LIBS:, and at least
 * 48 KB of stack (StackSwap below, same technique as UP-Demo). */
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <dos/dos.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#ifndef MUIA_Window_Borderless
#define MUIA_Window_Borderless 0x80429d2bUL
#endif
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include <mcc/up.h>

#include <libraries/asl.h>
#include <proto/asl.h>

#include "archive.h"
#include "build_gate.h"
#include "last_choice.h"
#include "creators.h"
#include "diz_count.h"
#include "divisions.h"
#include "paths_list.h"
#include "phase_progress.h"
#include "sections.h"
#include "platform_tag.h"
#include "aminet_short.h"
#include "aminet.h"
#include "aminet_preview.h"
#include "release_preview.h"
#include "aminet_types.h"
#include "archive_preview.h"
#include "credits.h"
#include "date.h"
#include "groups.h"
#include "ui_editor.h"
#include "naming.h"
#include "text_norm.h"
#include "nfo_uprough.h"
#include "nfo_x.h"
#include "online.h"
#include "release_seed.h"
#include "stage.h"
#include "template.h"

struct Library *MUIMasterBase;

#define SHELL_STACK_BYTES 49152UL
#define IntuitionBase shell_IntuitionBase
struct IntuitionBase *IntuitionBase;

void shell_requester(const char *text) {
    struct EasyStruct es;
    struct IntuitionBase *prev = IntuitionBase;
    struct IntuitionBase *ib = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR) "intuition.library", 36);
    if (ib == NULL) return;
    IntuitionBase = ib;
    es.es_StructSize = sizeof es;
    es.es_Flags = 0;
    es.es_Title = (UBYTE *)"Release Packer";
    es.es_TextFormat = (UBYTE *)"%s";
    es.es_GadgetFormat = (UBYTE *)"Ok";
    EasyRequest(NULL, &es, NULL, (APTR)text);
    CloseLibrary((struct Library *)ib);
    IntuitionBase = prev;
}

Object *shell_label(const char *text, ULONG tone) {
    return MUI_NewObject((char *)"UP-Text.mcc", UPA_Text_Contents, (ULONG)text, UPA_Text_Tone, tone, TAG_DONE);
}

/* A row label: fixed to its text width so the field beside it gets the rest of the row (an unbounded label took half and cut the placeholder). */
Object *row_label(const char *text) {
    return MUI_NewObject((char *)"UP-Text.mcc", UPA_Text_Contents, (ULONG)text, UPA_Text_Tone, (ULONG)UPV_Text_Tone_Normal, UPA_Text_Fixed,
                         (ULONG)TRUE, TAG_DONE);
}

/* A label for a multi-line row: a column group sizes a row by its least stretchy cell, so the label sits above a spacer that stretches. */
Object *area_label(const char *text) {
    return MUI_NewObject((char *)MUIC_Group, MUIA_Group_Child, (ULONG)row_label(text), MUIA_Group_Child, (ULONG)MUI_NewObject((char *)MUIC_Rectangle, TAG_DONE), TAG_DONE);
}

/* A row of one or two widgets; a NULL second half leaves a full-width row. */
Object *shell_row(Object *a, Object *b) {
    if (b == NULL) {
        return MUI_NewObject((char *)MUIC_Group, MUIA_Group_Horiz, (ULONG)TRUE, MUIA_FillArea, (ULONG)FALSE,
                             MUIA_Group_Child, (ULONG)a, TAG_DONE);
    }
    return MUI_NewObject((char *)MUIC_Group, MUIA_Group_Horiz, (ULONG)TRUE, MUIA_FillArea, (ULONG)FALSE,
                         MUIA_Group_Child, (ULONG)a, MUIA_Group_Child, (ULONG)b, TAG_DONE);
}

/* A two-column form: labels in one column (as wide as the widest), fields in the other, so every input shares a left edge. */
Object *shell_form(Object *l1, Object *f1, Object *l2, Object *f2, Object *l3, Object *f3, Object *l4, Object *f4,
                          Object *l5, Object *f5, Object *l6, Object *f6, Object *l7, Object *f7, Object *l8, Object *f8, Object *l9, Object *f9, Object *l10, Object *f10) {
    Object *kids[20];
    struct TagItem t[24];
    int i, n = 0;
    kids[0] = l1; kids[1] = f1; kids[2] = l2; kids[3] = f2; kids[4] = l3; kids[5] = f3; kids[6] = l4;
    kids[7] = f4; kids[8] = l5; kids[9] = f5; kids[10] = l6; kids[11] = f6; kids[12] = l7; kids[13] = f7; kids[14] = l8; kids[15] = f8; kids[16] = l9; kids[17] = f9; kids[18] = l10; kids[19] = f10;
    t[n].ti_Tag = MUIA_Group_Columns;
    t[n++].ti_Data = 2;
    t[n].ti_Tag = MUIA_FillArea;
    t[n++].ti_Data = (ULONG)FALSE;
    for (i = 0; i < 20; i++) {
        if (kids[i] == NULL) continue;
        t[n].ti_Tag = MUIA_Group_Child;
        t[n++].ti_Data = (ULONG)kids[i];
    }
    t[n].ti_Tag = TAG_DONE;
    t[n].ti_Data = 0;
    return MUI_NewObjectA((char *)MUIC_Group, t);
}

/* A raised card: a heading line, then the widgets (NULLs skipped). */
/* A page card sits at the top of its page: a PageMode group is as tall as its tallest page and centres the shorter cards. */
#define SHELL_PAD 4
/* A fixed blank gap (w x h px) for window padding and the space above the bottom row. */
Object *shell_gap(ULONG w, ULONG h) {
    return MUI_NewObject((char *)MUIC_Rectangle, MUIA_FillArea, (ULONG)FALSE, w ? MUIA_FixWidth : TAG_IGNORE, w,
                         h ? MUIA_FixHeight : TAG_IGNORE, h, TAG_DONE);
}

Object *top_page(Object *card) {
    return MUI_NewObject((char *)MUIC_Group, MUIA_FillArea, (ULONG)FALSE, MUIA_Group_VertSpacing, 0UL, MUIA_Group_Child, (ULONG)card,
                         MUIA_Group_Child, (ULONG)MUI_NewObject((char *)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Surface, MUIA_UserData, (ULONG)UPV_Panel_KeepSpacing, MUIA_Weight, 1UL, TAG_DONE), TAG_DONE); /* the filler erases what a taller page left below this card */
}

Object *shell_card(const char *title, Object *a, Object *b, Object *c, Object *d, Object *e, Object *f, Object *g) {
    struct TagItem t[16];
    Object *kids[8];
    int i, n = 0;
    kids[0] = shell_label(title, UPV_Text_Tone_Title);
    kids[1] = a;
    kids[2] = b;
    kids[3] = c;
    kids[4] = d;
    kids[5] = e;
    kids[6] = f;
    kids[7] = g;
    t[n].ti_Tag = UPA_Panel_Role;
    t[n++].ti_Data = (ULONG)UPV_Panel_Role_Raised;
    t[n].ti_Tag = UPA_Panel_FlatTopLeft; /* flush against the first tab */
    t[n++].ti_Data = (ULONG)TRUE;
    t[n].ti_Tag = MUIA_Weight; /* outweighs the surface filler in top_page: a multi-line text area takes the spare window height */
    t[n++].ti_Data = 1000UL;
    for (i = 0; i < 8; i++) {
        if (kids[i] == NULL) continue;
        t[n].ti_Tag = MUIA_Group_Child;
        t[n++].ti_Data = (ULONG)kids[i];
    }
    t[n].ti_Tag = TAG_DONE;
    t[n].ti_Data = 0;
    return MUI_NewObjectA((char *)"UP-Panel.mcc", t);
}

/* The window build NULL-checks every object and names the fix, so there is
 * no pre-check: creating classes just to throw them away again is exactly
 * what wedged the rig (a bare UP-Cycle freezes in OM_NEW, and bare
 * create/dispose cycles of the others die at random points). */

#define ID_BUILD 1
#define ID_PICK 2
#define ID_PREVIEW 3
#define ID_FLATTEN 4
#define ID_COVER 5
#define ID_PUBLISH 6
#define ID_REVOKE 7
#define ID_DOOR 8
#define ID_NAME 9
#define ID_AMINET 10
#define ID_PLATFORM 11
#define ID_GROUP 12
#define ID_CODE 13
#define ID_AMINET_PREVIEW 14
#define ID_NFO_PREVIEW 15
#define ID_EDIT_GROUPS 16
#define ID_FORMAT 17
#define ID_EMBED_PICK 18
#define ID_ADDFILE 19
#define ID_REMOVEFILE 20
#define ID_REFRESH 21 /* redraw the file lists; ID_PREVIEW (the button) also runs the dry run */
#define ID_TAB 22      /* tab switched: refresh the NFO or readme preview shown */
#define ID_AUTHORS 24  /* the Authors line changed: redo the creators preview and the name suggestions */
#define ID_BACK 26     /* Back button: previous tab page */
#define ID_NEXT 27     /* Next button: next tab page */
#define ID_DIZCOUNT 25 /* Diz line, Released at or the name changed: redo the release line counter */
#define ID_FOCUS 23    /* another field got the cursor: the one just left may have changed the preview */
#define ID_CANCEL 28   /* Cancel button: stop the running Publish / Revoke / Door / Aminet transfer */

#define RP_MAX_FILES_PATH 511

static rp_paths g_paths;
static char g_cover[RP_MAX_FILES_PATH + 1];

/* The ASL file requester for an image. Puts the full path in out (cap bytes). Returns 1 when a file was picked. */
static int shell_pick_image(const char *title, char *out, size_t cap) {
    struct FileRequester *req = AllocAslRequestTags(ASL_FileRequest, ASLFR_TitleText, (ULONG)title,
                                                    ASLFR_RejectIcons, TRUE, TAG_DONE);
    if (req == NULL) {
        shell_requester("The file requester could not open.\nFree some memory and try again.");
        return 0;
    }
    if (!AslRequest(req, NULL) || req->rf_File == NULL || req->rf_File[0] == '\0') {
        FreeAslRequest(req);
        return 0;
    }
    strncpy(out, req->rf_Dir, cap - 1);
    out[cap - 1] = '\0';
    AddPart((STRPTR)out, req->rf_File, cap);
    FreeAslRequest(req);
    return 1;
}

static int shell_pick_cover(void) { return shell_pick_image("Pick the cover image", g_cover, sizeof(g_cover)); }

/* Revoke cannot be undone: ask first, like the desktop confirm. Returns 1 on Revoke. */
static int shell_confirm_revoke(const char *code) {
    struct EasyStruct es;
    struct IntuitionBase *prev = IntuitionBase;
    struct IntuitionBase *ib = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR) "intuition.library", 36);
    LONG r;
    if (ib == NULL) return 0;
    IntuitionBase = ib;
    es.es_StructSize = sizeof es;
    es.es_Flags = 0;
    es.es_Title = (UBYTE *)"Release Packer";
    es.es_TextFormat = (UBYTE *)"Revoke release %s?\nThis removes its catalog entry, frees its release number\nand code, and deletes the published archive.\nOnly the most recently created release can be revoked.";
    es.es_GadgetFormat = (UBYTE *)"Revoke|Cancel";
    r = EasyRequest(NULL, &es, NULL, (APTR)code);
    CloseLibrary((struct Library *)ib);
    IntuitionBase = prev;
    return r == 1;
}

/* The ASL requester for a drawer or a file: the pick joins g_paths. Returns 1 when the list changed. */
static int shell_pick_path(int drawers_only) {
    struct FileRequester *req;
    char path[RP_PATH_CAP];
    int added;
    req = AllocAslRequestTags(ASL_FileRequest, ASLFR_TitleText, (ULONG)(drawers_only ? "Add a drawer" : "Add a file"),
                              ASLFR_DrawersOnly, (ULONG)(drawers_only ? TRUE : FALSE), ASLFR_RejectIcons, TRUE, TAG_DONE);
    if (req == NULL) {
        shell_requester("The file requester could not open.\nFree some memory and try again.");
        return 0;
    }
    if (!AslRequest(req, NULL)) {
        FreeAslRequest(req);
        return 0;
    }
    snprintf(path, sizeof(path), "%s", req->rf_Dir);
    if (!drawers_only && req->rf_File != NULL && req->rf_File[0] != '\0' && !AddPart((STRPTR)path, req->rf_File, sizeof(path))) {
        FreeAslRequest(req);
        return 0;
    }
    FreeAslRequest(req);
    added = rp_paths_add(&g_paths, path);
    if (added < 0) shell_requester("The list is full or the path is too long.\nRemove an entry first.");
    return added > 0;
}

static const char *base_name_of(const char *path) {
    const char *s = strrchr(path, '/');
    const char *c = strrchr(path, ':');
    if (c && (!s || c > s)) s = c;
    return s ? s + 1 : path;
}

static int same_name_fold(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a++, cb = *b++;
        if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 'a' + 'A');
        if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 'a' + 'A');
        if (ca != cb) return 0;
    }
    return *a == *b;
}

/* The archive preview: the list shows g_prev.entries; the warning row appears while a picked drawer only wraps one more drawer. */
static rp_preview g_prev;
static char g_warn[1100];

/* The hook gets the cell in a1: the trampoline pushes it for the C function (no register-argument syntax needed). */
ULONG rp_preview_row_c(UP_ListCell *cell);
ULONG rp_preview_row_c(UP_ListCell *cell) {
    if (cell->row < 0 || (size_t)cell->row >= g_prev.nentries) return (ULONG) "";
    return (ULONG)g_prev.entries[cell->row];
}
extern void rp_preview_row_tramp(void);
__asm__(".text\n.even\n.globl _rp_preview_row_tramp\n_rp_preview_row_tramp:\n"
        "\tmove.l %a1,-(%sp)\n\tjsr _rp_preview_row_c\n\taddq.l #4,%sp\n\trts\n");

ULONG rp_paths_row_c(UP_ListCell *cell);
ULONG rp_paths_row_c(UP_ListCell *cell) {
    if (cell->row < 0 || (size_t)cell->row >= g_paths.n) return (ULONG) "";
    return (ULONG)g_paths.path[cell->row];
}
extern void rp_paths_row_tramp(void);
__asm__(".text\n.even\n.globl _rp_paths_row_tramp\n_rp_paths_row_tramp:\n"
        "\tmove.l %a1,-(%sp)\n\tjsr _rp_paths_row_c\n\taddq.l #4,%sp\n\trts\n");

static struct Hook g_preview_hook, g_paths_hook;

/* The Release tab notes field; read by Build, Publish and Aminet. */
static Object *g_notes_field;
/* The Release tab news field (multi-line, like the desktop news text); read by Build and Publish. */
static Object *g_news_field;
/* The Release tab authors field: "Role: Name, Name; Role: Name", parsed by rp_parse_credits. */
static Object *g_authors_field;
/* The Release tab date (printed in the NFO, prefilled with today) and Released at (the diz line's "at ..."); the Aminet tab readme preview. */
static Object *g_also_aminet_chk;
static Object *g_sec_system[1];
static Object *g_revoke_field; /* desktop revoke-code: code to revoke, blank = the Code field */
static rp_sections g_sections; /* of the open group, set by shell_apply_sections */
static const rp_division *g_division; /* likewise; NULL for a placeholder group */
static Object *g_sec_history[2], *g_sec_boards[2], *g_sec_media[3], *g_sec_publish[6], *g_sec_door[1]; /* parts hidden per group, see rp_sections_for */
static Object *g_gauge; /* Build/Preview progress bar, hidden between runs */
/* A network transfer runs inside the transport, which calls shell_net_pump between socket chunks: the window
 * stays live, the form (g_net_pages) is disabled and the Cancel button shows. */
static Object *g_net_app, *g_net_status, *g_net_pages, *g_net_cancel_btn;
static int g_net_cancel, g_net_quit;
static unsigned long g_net_shown; /* last percent drawn, ~0UL = the waiting line */
static Object *g_date_field, *g_released_field, *g_diz_line_field, *g_readme_view, *g_nfo_view;
static Object *g_diz_count; /* "Release line: N/width characters" under the Diz line */
static Object *g_template_status; /* desktop template-status: preview notes of a placeholder group, own line on the NFO tab */
/* The manual release number (desktop release-number-text): its row shows only for a group without a release server. */
static Object *g_number_field, *g_number_row;
static char g_number_text[24];
/* The Platform detail text (which console / the Other tag) and its row, shown for Console and Other only. */
static Object *g_platform_text_field, *g_platform_row;
static char g_platform_tag_buf[64];
/* The Aminet Short line (40 characters at most), prefilled from the Description when empty. */
static Object *g_short_field;

const char *shell_field_text(Object *field) {
    ULONG ptr = 0;
    STRPTR txt = NULL;
    GetAttr(UPA_Field_String, field, &ptr);
    if (ptr != 0) GetAttr(MUIA_String_Contents, (Object *)ptr, (ULONG *)&txt);
    return txt ? (const char *)txt : "";
}

/* The platform tag for the Platform dropdown and its detail text (static buffer, used before the next call). */
static const char *shell_platform_tag(Object *platform_field) {
    ULONG pa = 0;
    GetAttr(UPA_Cycle_Active, platform_field, &pa);
    rp_platform_tag((int)pa, shell_field_text(g_platform_text_field), g_platform_tag_buf, sizeof g_platform_tag_buf);
    return g_platform_tag_buf;
}

/* Show the detail row for Console and Other only. */
static void shell_platform_row(Object *platform_field) {
    ULONG pa = 0;
    GetAttr(UPA_Cycle_Active, platform_field, &pa);
    SetAttrs(g_platform_row, MUIA_ShowMe, (ULONG)rp_platform_wants_text((int)pa), TAG_DONE);
}

/* The multi-line fields as NFO-ready text (rp_text_normalize) in static copies: the class returns its own live buffer. */
#define SHELL_NOTES_MAX 2000
#define SHELL_NEWS_MAX 1000
#define SHELL_README_MAX 4000
#define SHELL_NFO_VIEW_MAX 4000 /* the Preview tab box: the diz and the NFO together */
static char g_notes_buf[SHELL_NOTES_MAX + 1], g_news_buf[SHELL_NEWS_MAX + 1];

static const char *shell_area_text(Object *area, char *buf, size_t cap) {
    ULONG ptr = 0;
    GetAttr(UPA_TextArea_Text, area, &ptr);
    rp_text_normalize(ptr ? (const char *)ptr : "", buf, cap);
    return buf;
}

static const char *shell_notes_text(void) { return shell_area_text(g_notes_field, g_notes_buf, sizeof g_notes_buf); }
static const char *shell_news_text(void) { return shell_area_text(g_news_field, g_news_buf, sizeof g_news_buf); }

/* The Roster tab: members ("Nickname | role"), boards (X Innovations only) and media embeds, one row per line. */
#define SHELL_ROWS_MAX 3000
static Object *g_members_field, *g_boards_field, *g_embeds_field;
static rp_member g_rows_members[RP_MEMBERS_MAX];
static rp_board_row g_rows_boards[RP_ONLINE_BOARDS_MAX];
static rp_embed_row g_rows_embeds[RP_ONLINE_EMBEDS_MAX];
static size_t g_nrows_members, g_nrows_boards, g_nrows_embeds;

/* The field a failed check names (desktop field-errors.ts marks it): its tab and object, focused by the main loop.
 * NULL when the failure belongs to no single field. */
#define SHELL_TAB_RELEASE 0
#define SHELL_TAB_NOTES 1
#define SHELL_TAB_ROSTER 2
#define SHELL_TAB_FILES 4
#define SHELL_TAB_NFO 3
#define SHELL_TAB_AMINET 6
#define SHELL_TAB_COUNT 7 /* pages in the tab group, in tab_captions order */
static Object *g_err_obj;
static ULONG g_err_tab = (ULONG)-1; /* -1: no failed field to show */

static void shell_err_at(ULONG tab, Object *obj) {
    g_err_tab = tab;
    g_err_obj = obj;
}

static const char *shell_area_raw(Object *area) {
    ULONG ptr = 0;
    GetAttr(UPA_TextArea_Text, area, &ptr);
    return ptr ? (const char *)ptr : "";
}

/* Pick an image file and add it to the Media embeds field as an "image | caption | path" row. */
static void shell_embed_pick(void) {
    char path[RP_MAX_FILES_PATH + 1];
    char *text;
    if (!shell_pick_image("Pick the embed image", path, sizeof(path))) return;
    text = rp_embed_append_image(shell_area_raw(g_embeds_field), path);
    if (text == NULL) {
        shell_requester("That file cannot be added as a media embed.\nIts path is too long or contains a vertical bar.");
        return;
    }
    SetAttrs(g_embeds_field, UPA_TextArea_Text, (ULONG)text, TAG_DONE);
    free(text);
}

/* Parse the Roster tab for group g into the g_rows_* arrays. -1 with "<Field>: Line N: msg" in msg on a bad row. Boards
 * are read for X Innovations, embeds for door-server groups (as on the desktop); the others stay empty. */
static int shell_roster_read(const rp_group *g, char *msg, size_t cap) {
    rp_row_err err;
    int n;
    g_nrows_members = g_nrows_boards = g_nrows_embeds = 0;
    n = rp_parse_member_rows(shell_area_raw(g_members_field), g_rows_members, RP_MEMBERS_MAX, &err);
    if (n < 0) { shell_err_at(SHELL_TAB_ROSTER, g_members_field); snprintf(msg, cap, "Members: Line %lu: %s", (unsigned long)err.line, err.msg); return -1; }
    g_nrows_members = (size_t)n;
    if (g->renderer == R_XINNOVATIONS) {
        n = rp_parse_board_rows(shell_area_raw(g_boards_field), g_rows_boards, RP_ONLINE_BOARDS_MAX, &err);
        if (n < 0) { shell_err_at(SHELL_TAB_ROSTER, g_boards_field); snprintf(msg, cap, "Boards: Line %lu: %s", (unsigned long)err.line, err.msg); return -1; }
        g_nrows_boards = (size_t)n;
    }
    if (RP_ONLINE_HAS_SERVER(g)) {
        n = rp_parse_embed_rows(shell_area_raw(g_embeds_field), g_rows_embeds, RP_ONLINE_EMBEDS_MAX, &err);
        if (n < 0) { shell_err_at(SHELL_TAB_ROSTER, g_embeds_field); snprintf(msg, cap, "Media embeds: Line %lu: %s", (unsigned long)err.line, err.msg); return -1; }
        g_nrows_embeds = (size_t)n;
    }
    return 0;
}

static rp_credit g_credits[RP_CREDITS_MAX];

/* The parsed authors, in g_credits; returns how many. */
static size_t shell_credits(void) {
    return rp_parse_credits(shell_field_text(g_authors_field), g_credits, RP_CREDITS_MAX);
}

/* The Creators preview under Authors (desktop creators-preview) with the member names that match the name being typed
 * (desktop creators-autocomplete, shown as text because the Amiga form has no popup list). */
static Object *g_creators_text, *g_creators_row;
static char g_creators_buf[480];
static rp_member g_creators_members[RP_MEMBERS_MAX];

/* The roster's nicknames; a Members line that does not parse leaves none (Build names the line). */
static size_t shell_creators_members(void) {
    rp_row_err err;
    int n = rp_parse_member_rows(shell_area_raw(g_members_field), g_creators_members, RP_MEMBERS_MAX, &err);
    return n < 0 ? 0 : (size_t)n;
}

static void shell_creators_refresh(int x_group) {
    char tok[RP_NICK_MAX + 1];
    const char *hit[RP_CREATORS_SUGGEST_MAX];
    size_t n = shell_credits(), nm = shell_creators_members(), k, i, len;
    rp_creators_token(shell_field_text(g_authors_field), tok, sizeof tok);
    rp_creators_preview(g_credits, n, x_group, g_creators_buf, sizeof g_creators_buf);
    k = rp_creators_matches(tok, g_creators_members, nm, hit, RP_CREATORS_SUGGEST_MAX);
    for (i = 0; i < k && g_sections.creators; i++) { /* member hits need a release server (desktop creatorsAutocomplete) */
        len = strlen(g_creators_buf);
        snprintf(g_creators_buf + len, sizeof g_creators_buf - len, "%s%s", i == 0 ? " | Members: " : ", ", hit[i]);
    }
    SetAttrs(g_creators_text, UPA_Text_Contents, (ULONG)g_creators_buf, TAG_DONE);
}

/* The Authors field was left: a name that is the start of exactly one member becomes that member (desktop picks with Enter). */
static void shell_creators_complete(void) {
    char out[256];
    if (g_sections.creators && rp_creators_complete(shell_field_text(g_authors_field), g_creators_members, shell_creators_members(), out, sizeof out)) shell_set_field(g_authors_field, out);
}

/* The /X System field (desktop x-system), prefilled from the Platform tag while the user has not typed their own. */
static Object *g_system_field, *g_system_row;
static char g_system_prefill[64];

static void shell_system_prefill(Object *platform_field) {
    const char *now = shell_field_text(g_system_field);
    if (now[0] != '\0' && strcmp(now, g_system_prefill) != 0) return;
    snprintf(g_system_prefill, sizeof g_system_prefill, "%s", shell_platform_tag(platform_field));
    shell_set_field(g_system_field, g_system_prefill);
}

/* The System text the /X NFO prints: the field, or the Platform tag when it was cleared. */
static const char *shell_system_text(const char *platform_tag) {
    const char *t = shell_field_text(g_system_field);
    return t[0] != '\0' ? t : platform_tag;
}

/* Copies a malloc'd string into dst (cut to cap) and frees it; NULL gives "". */
static void shell_take(char *dst, size_t cap, char *s) {
    dst[0] = '\0';
    if (s == NULL) return;
    strncpy(dst, s, cap - 1);
    dst[cap - 1] = '\0';
    free(s);
}

void shell_set_field(Object *field, const char *text) {
    ULONG ptr = 0;
    GetAttr(UPA_Field_String, field, &ptr);
    if (ptr != 0) SetAttrs((Object *)ptr, MUIA_String_Contents, (ULONG)text, TAG_DONE);
}

/* Today as YYYY-MM-DD (the Release date default). */
static void shell_today(char *out, size_t cap) {
    struct DateStamp ds;
    DateStamp(&ds);
    rp_iso_date((long)ds.ds_Days + 2922, out, cap);
}

#define RP_AMINET_PREFS_PATH "PROGDIR:aminet.prefs"
#define RP_LAST_CHOICE_PATH "PROGDIR:last.prefs"

/* Remember the open group and format for the next start, and the format for that group (desktop main.ts keeps both in localStorage). */
static int shell_last_save(const rp_group *groups, int ngroups, Object *group_cycle, Object *format_cycle) {
    ULONG ga = 0, fa = 0;
    rp_last_choice c;
    GetAttr(UPA_Cycle_Active, group_cycle, &ga);
    GetAttr(UPA_Cycle_Active, format_cycle, &fa);
    if (ga >= (ULONG)ngroups) return 0;
    rp_last_choice_load(RP_LAST_CHOICE_PATH, &c); /* keeps the other groups' formats; a missing file is empty */
    snprintf(c.group, sizeof c.group, "%s", groups[ga].id);
    c.zip = fa != 0;
    rp_last_format_set(&c, groups[ga].id, c.zip);
    return rp_last_choice_save(RP_LAST_CHOICE_PATH, &c);
}

/* Select the remembered group and format; a group that no longer exists leaves the first one. Returns the open group's index. */
static ULONG shell_last_restore(const rp_group *groups, int ngroups, Object *group_cycle, Object *format_cycle) {
    rp_last_choice c;
    ULONG i;
    if (rp_last_choice_load(RP_LAST_CHOICE_PATH, &c) != 0) return 0;
    SetAttrs(format_cycle, UPA_Cycle_Active, (ULONG)(c.zip ? 1 : 0), TAG_DONE);
    for (i = 0; i < (ULONG)ngroups; i++) {
        if (strcmp(groups[i].id, c.group) == 0) {
            int z;
            if (rp_last_format_get(&c, c.group, &z)) SetAttrs(format_cycle, UPA_Cycle_Active, (ULONG)z, TAG_DONE);
            SetAttrs(group_cycle, UPA_Cycle_Active, i, TAG_DONE);
            return i;
        }
    }
    return 0;
}

/* The format the newly chosen group last used; a group that never picked one keeps the current format. */
static void shell_last_format_apply(const rp_group *groups, int ngroups, Object *group_cycle, Object *format_cycle) {
    ULONG ga = 0;
    rp_last_choice c;
    int z;
    GetAttr(UPA_Cycle_Active, group_cycle, &ga);
    if (ga >= (ULONG)ngroups) return;
    rp_last_choice_load(RP_LAST_CHOICE_PATH, &c);
    if (rp_last_format_get(&c, groups[ga].id, &z)) SetAttrs(format_cycle, UPA_Cycle_Active, (ULONG)z, TAG_DONE);
}

/* Aminet Directory cycle entries: index 0 is "(choose)" (no directory), index i+1 is rp_aminet_types[i]. */
static const char *g_type_labels[RP_AMINET_TYPES_MAX + 2];

static void shell_type_labels_init(void) {
    int i;
    g_type_labels[0] = "(choose)";
    for (i = 0; rp_aminet_types[i] && i < RP_AMINET_TYPES_MAX; i++) g_type_labels[i + 1] = rp_aminet_types[i];
    g_type_labels[i + 1] = NULL;
}

static const char *shell_type_dir(Object *cycle) {
    ULONG a = 0;
    GetAttr(UPA_Cycle_Active, cycle, &a);
    return a > 0 && a <= (ULONG)rp_aminet_type_count() ? rp_aminet_types[a - 1] : "";
}

/* An unknown or empty directory selects "(choose)". */
static void shell_set_type(Object *cycle, const char *dir) {
    SetAttrs(cycle, UPA_Cycle_Active, (ULONG)(rp_aminet_type_index(dir) + 1), TAG_DONE);
}

/* Category cycle: index 0 is "(choose)", index i+1 is rp_release_categories[i]. */
static const char *g_category_labels[RP_RELEASE_CATEGORY_COUNT + 2];
static Object *g_category_cycle, *g_category_lbl;

static void shell_category_labels_init(void) {
    int i;
    g_category_labels[0] = "(choose)";
    for (i = 0; rp_release_categories[i] && i < RP_RELEASE_CATEGORY_COUNT; i++) g_category_labels[i + 1] = rp_release_categories[i];
    g_category_labels[i + 1] = NULL;
}

/* The chosen history category, "" when none. */
static const char *shell_category(void) {
    ULONG a = 0;
    if (!g_sections.history) return ""; /* hidden, so not sent (desktop entryCategory is empty without historyEntry) */
    GetAttr(UPA_Cycle_Active, g_category_cycle, &a);
    return a > 0 && a <= (ULONG)RP_RELEASE_CATEGORY_COUNT ? rp_release_categories[a - 1] : "";
}

/* Publish and Aminet upload need a Build of the CURRENT form, made in this run (desktop: Publish unlocks after Build). */
static rp_build_gate g_gate;

/* Hash of every field that goes into the archive, the NFO or the server entry. */
static unsigned long shell_form_state(Object *group_cycle, Object *name_field, Object *code_field, Object *platform_field,
                                      Object *desc_field, Object *format_cycle) {
    ULONG ga = 0, pa = 0, fa = 0;
    char num[48];
    unsigned long h = RP_GATE_BEGIN;
    GetAttr(UPA_Cycle_Active, group_cycle, &ga);
    GetAttr(UPA_Cycle_Active, platform_field, &pa);
    GetAttr(UPA_Cycle_Active, format_cycle, &fa);
    snprintf(num, sizeof num, "%lu/%lu/%lu", (unsigned long)ga, (unsigned long)pa, (unsigned long)fa);
    h = rp_gate_add(h, num);
    h = rp_gate_add(h, shell_category());
    h = rp_gate_add(h, shell_platform_tag(platform_field));
    h = rp_gate_add(h, shell_field_text(name_field));
    h = rp_gate_add(h, shell_field_text(code_field));
    h = rp_gate_add(h, shell_field_text(desc_field));
    h = rp_gate_add(h, shell_field_text(g_authors_field));
    h = rp_gate_add(h, shell_field_text(g_system_field));
    h = rp_gate_add(h, shell_field_text(g_date_field));
    h = rp_gate_add(h, shell_field_text(g_released_field));
    h = rp_gate_add(h, shell_field_text(g_diz_line_field));
    h = rp_gate_add(h, shell_field_text(g_number_field));
    {
        size_t gi;
        for (gi = 0; gi < g_paths.n; gi++) h = rp_gate_add(h, g_paths.path[gi]);
    }
    h = rp_gate_add(h, shell_area_raw(g_notes_field));
    h = rp_gate_add(h, shell_area_raw(g_news_field));
    h = rp_gate_add(h, shell_area_raw(g_members_field));
    h = rp_gate_add(h, shell_area_raw(g_boards_field));
    return rp_gate_add(h, shell_area_raw(g_embeds_field));
}

/* AmigaOS clock (days since 1978-01-01) as Unix seconds, for the seed's stale-draft ages. */
static long shell_unix_now(void) {
    struct DateStamp ds;
    DateStamp(&ds);
    return ((long)ds.ds_Days + 2922L) * 86400L + (long)ds.ds_Minute * 60L + (long)ds.ds_Tick / 50L;
}

/* The release server's seed for the open group (next number, warnings). Static: rp_seed_info is about 7 KB. */
static rp_seed_info g_seed;
static jnode *g_seed_tree;
static char g_code_checked[48]; /* "<group>:<code>" last asked about, so a repeat notify does not ask again */

static int shell_seed_load(const rp_group *g, char *msg, size_t cap) {
    json_free(g_seed_tree);
    g_seed_tree = NULL;
    memset(&g_seed, 0, sizeof g_seed);
    g_code_checked[0] = '\0';
    return rp_online_seed(g, rp_http_amiga, NULL, shell_unix_now(), &g_seed, &g_seed_tree, msg, cap);
}

/* What the last prefill left in each Roster area (read back from the field), so a later prefill can tell untouched text from user edits. */
static char *g_prefilled_members, *g_prefilled_boards;

/* Put text in an area only when the user has not edited it (empty, or still the last prefill). */
static void shell_prefill_area(Object *area, char **last, const char *text) {
    if (!rp_prefill_replaceable(shell_area_raw(area), *last ? *last : "")) return;
    SetAttrs(area, UPA_TextArea_Text, (ULONG)text, TAG_DONE);
    free(*last);
    *last = strdup(shell_area_raw(area));
}

/* Fill the Roster tab for group g: the seed's merged members and boards when seeded, else the group file's members.
 * Text the user typed stays; the embeds are never prefilled. */
static void shell_roster_prefill(const rp_group *g, int seeded) {
    char *m = seeded ? rp_format_member_rows(g_seed.members, g_seed.nmembers) : rp_format_member_rows(g->members, g->nmembers);
    char *b = (seeded && g->renderer == R_XINNOVATIONS) ? rp_format_board_rows(g_seed.boards, g_seed.nboards) : NULL;
    shell_prefill_area(g_members_field, &g_prefilled_members, m ? m : "");
    shell_prefill_area(g_boards_field, &g_prefilled_boards, b ? b : "");
    free(m);
    free(b);
}

static int g_seed_ok; /* the open group's seed loaded, so the status line can show the release number */

/* The status line for the open group: the release number preview, then the seed's warnings. Redone on every Code or Format edit. */
static void shell_number_status(Object *status, const rp_group *g, Object *code_field, Object *format_cycle) {
    char ok[2200], pv[96], code[40];
    ULONG fa = 0;
    GetAttr(UPA_Cycle_Active, format_cycle, &fa);
    rp_upper(shell_field_text(code_field), code, sizeof(code));
    if (rp_release_number_preview(g_seed.number_text, g->file_prefix, code, g->code_length, fa == 0, pv, sizeof(pv)) != 0) pv[0] = '\0';
    snprintf(ok, sizeof(ok), "%s%s%s", pv, pv[0] && g_seed.warnings[0] ? " - " : "", g_seed.warnings);
    SetAttrs(status, UPA_Text_Contents, (ULONG)ok, TAG_DONE);
}

static void shell_show_all(Object **objs, size_t n, int show) {
    size_t i;
    for (i = 0; i < n; i++) SetAttrs(objs[i], MUIA_ShowMe, (ULONG)show, TAG_DONE);
}

/* Hides the boards, media, Publish and Door parts the group does not have (desktop visibleSections); a label hides with its field. */
static void shell_apply_sections(const rp_group *g) {
    rp_sections s = g_sections = rp_sections_for(g);
    g_division = rp_division_for(g);
    shell_show_all(g_sec_history, 2, s.history);
    shell_show_all(g_sec_boards, 2, s.boards);
    shell_show_all(g_sec_system, 1, s.system);
    shell_show_all(g_sec_media, 3, s.media);
    shell_show_all(g_sec_publish, 6, s.publish);
    shell_show_all(g_sec_door, 1, s.door);
    SetAttrs(g_template_status, MUIA_ShowMe, (ULONG)s.diz_preview, TAG_DONE);
    SetAttrs(g_diz_count, MUIA_ShowMe, (ULONG)s.diz_line, TAG_DONE); /* a placeholder diz has no release line row */
    SetAttrs(g_number_row, MUIA_ShowMe, (ULONG)s.number, TAG_DONE);
    if (!s.number) shell_set_field(g_number_field, ""); /* a server counts releases: a hidden typed number must not linger to confuse the status */
}

/* On open and on a group change: load the seed, fill the Roster tab, and show the seed's warnings (or the failure) in the status line. */
static void shell_seed_status(Object *status, const rp_group *g, Object *code_field, Object *format_cycle) {
    char msg[400];
    g_seed_ok = 0;
    shell_apply_sections(g);
    if (!RP_ONLINE_HAS_SERVER(g)) {
        shell_roster_prefill(g, 0);
        return;
    }
    if (shell_seed_load(g, msg, sizeof(msg)) != 0) {
        shell_roster_prefill(g, 0);
        SetAttrs(status, UPA_Text_Contents, (ULONG)msg, TAG_DONE);
        return;
    }
    shell_roster_prefill(g, 1);
    g_seed_ok = 1;
    shell_number_status(status, g, code_field, format_cycle);
}

/* Once the typed code has the group's length, ask the server whether it is taken. */
static void shell_code_check(Object *status, const rp_group *groups, Object *group_cycle, Object *code_field) {
    ULONG ga = 0;
    const rp_group *g;
    char code[40], key[48], base[64], msg[400];
    int rc;
    GetAttr(UPA_Cycle_Active, group_cycle, &ga);
    g = &groups[ga];
    rp_upper(shell_field_text(code_field), code, sizeof(code));
    if (!RP_ONLINE_HAS_SERVER(g) || (int)strlen(code) != g->code_length) return;
    snprintf(key, sizeof(key), "%s:%s", g->id, code);
    if (strcmp(key, g_code_checked) == 0) return;
    strcpy(g_code_checked, key);
    rc = rp_online_code_check(g, code, rp_http_amiga, NULL, msg, sizeof(msg));
    if (rc == 0) snprintf(msg, sizeof(msg), "Code %s is free.", code);
    SetAttrs(status, UPA_Text_Contents, (ULONG)msg, TAG_DONE);
}

static void shell_refresh_preview(Object *list, Object *warn_row, Object *warn_text, const rp_group *groups,
                                  Object *group_cycle, Object *code_field, Object *status) {
    ULONG gactive = 0;
    char code[40], base[64], errmsg[256];
    const char *paths[RP_PATHS_MAX];
    size_t pi;
    rp_preview_free(&g_prev);
    SetAttrs(warn_row, MUIA_ShowMe, FALSE, TAG_DONE);
    {
        GetAttr(UPA_Cycle_Active, group_cycle, &gactive);
        rp_upper(shell_field_text(code_field), code, sizeof(code));
        rp_base_name(groups[gactive].file_prefix, code, base, sizeof(base));
        for (pi = 0; pi < g_paths.n; pi++) paths[pi] = g_paths.path[pi];
        /* nothing picked: zero paths still lists file_id.diz, <REL>/ and <REL>/<REL>.NFO */
        if (rp_preview_archive(base, paths, g_paths.n, &g_prev, errmsg, sizeof(errmsg)) != 0) {
            SetAttrs(status, UPA_Text_Contents, (ULONG)errmsg, TAG_DONE);
        } else if (g_prev.nnested > 0) {
            snprintf(g_warn, sizeof(g_warn), "Extra nested folder: %s only wraps %s", g_prev.nested[0].path,
                     base_name_of(g_prev.nested[0].flatten_to));
            SetAttrs(warn_text, UPA_Text_Contents, (ULONG)g_warn, TAG_DONE);
            SetAttrs(warn_row, MUIA_ShowMe, TRUE, TAG_DONE);
        }
    }
    SetAttrs(list, UPA_List_Count, (ULONG)g_prev.nentries, UPA_List_Active, (ULONG)-1, TAG_DONE);
}

static void shell_guess_type(Object *type_cycle, Object *platform_field) {
    const char *guess = rp_aminet_guess_type(shell_category(), shell_platform_tag(platform_field));
    if (guess[0]) shell_set_type(type_cycle, guess);
}

/* One phase of a long step in the status line, drawn at once (the step blocks MUI until it returns). */
static void shell_phase(Object *app, Object *status, int step, const char *text) {
    ULONG sigs = 0;
    char line[160];
    rp_phase_line(step, RP_PHASES, text, line, sizeof(line));
    SetAttrs(g_gauge, UPA_Progress_Value, (ULONG)rp_phase_percent(step, RP_PHASES), MUIA_ShowMe, (ULONG)TRUE, TAG_DONE);
    SetAttrs(status, UPA_Text_Contents, (ULONG)line, TAG_DONE);
    DoMethod(app, MUIM_Application_NewInput, (ULONG)&sigs);
}

/* The generated release texts and the names around them; Build packs them, the Preview tab shows them. */
typedef struct {
    const rp_group *g;
    int zip;
    char base[64], packed[72];
    const char *number;
    char *nfo; /* NULL: a placeholder group without release.nfo */
    char *diz;
} shell_rendered;

static int shell_fail(char *err, size_t cap, const char *m) {
    snprintf(err, cap, "%s", m);
    return -1;
}

/* The one place the NFO and file_id.diz text is made: validates the form, loads the template and roster,
 * renders both. Returns 0 (caller frees out->nfo and out->diz) or -1 with the reason in err. */
static int shell_render_release(const rp_group *groups, Object *group_cycle, Object *name_field, Object *code_field,
                                Object *platform_field, Object *desc_field, Object *format_cycle, shell_rendered *out,
                                char *err, size_t errcap) {
    /* The inner string gadgets hold the text; UPA_Field_String gives them. */
    ULONG name_ptr = 0, code_ptr = 0, desc_ptr = 0, gactive = 0, active = 0;
    STRPTR name_txt, code_txt, plat_txt, desc_txt;
    const rp_group *g;
    const rp_member *members;
    size_t nmembers;
    rp_x_board xboards[RP_ONLINE_BOARDS_MAX];
    char path[256], *diz = NULL, *nfo = NULL, code[40], *base = out->base, *packed = out->packed, date[16];
    char membuf[RP_MEMBERS_MAX][RP_NICK_MAX + RP_ROLE_MAX + 4];
    const char *memberlines[RP_MEMBERS_MAX];
    static char creditbuf[RP_CREDITS_MAX][RP_CREDIT_ROLE_MAX + RP_CREDIT_NAME_MAX + 4];
    const char *creditlines[RP_CREDITS_MAX];
    char creators[256], coder[128], design[128];
    size_t ncredits;
    rp_scalar scalars[12];
    rp_block blocks[4];
    rp_values v;
    static const char *const no_lines;
    size_t i, width;
    int errline = 0;
    char errmsg[256], *rendered;
    const char *number = "?", *category = "";

    GetAttr(UPA_Field_String, name_field, &name_ptr);
    GetAttr(UPA_Field_String, code_field, &code_ptr);
    GetAttr(UPA_Field_String, desc_field, &desc_ptr);
    name_txt = (STRPTR) "";
    code_txt = (STRPTR) "";
    plat_txt = (STRPTR) "";
    desc_txt = (STRPTR) "";
    if (name_ptr != 0) GetAttr(MUIA_String_Contents, (Object *)name_ptr, (ULONG *)&name_txt);
    if (code_ptr != 0) GetAttr(MUIA_String_Contents, (Object *)code_ptr, (ULONG *)&code_txt);
    plat_txt = (STRPTR)shell_platform_tag(platform_field);
    if (desc_ptr != 0) GetAttr(MUIA_String_Contents, (Object *)desc_ptr, (ULONG *)&desc_txt);
    if (name_txt == NULL) name_txt = (STRPTR) "";
    if (code_txt == NULL) code_txt = (STRPTR) "";
    if (desc_txt == NULL) desc_txt = (STRPTR) "";
    GetAttr(UPA_Cycle_Active, group_cycle, &gactive);
    GetAttr(UPA_Cycle_Active, format_cycle, &active);
    g = &groups[gactive];
    /* the Release tab's date; an emptied field falls back to today like the desktop */
    snprintf(date, sizeof(date), "%s", shell_field_text(g_date_field));
    if (date[0] == '\0') shell_today(date, sizeof(date));
    rp_upper(code_txt, code, sizeof(code));
    if ((int)strlen(code) != g->code_length) {
        snprintf(errmsg, sizeof(errmsg), "Code must be exactly %d letters or digits.", g->code_length);
        shell_err_at(SHELL_TAB_RELEASE, code_field);
        return shell_fail(err, errcap, errmsg);
    }
    if (shell_roster_read(g, errmsg, sizeof(errmsg)) != 0) return shell_fail(err, errcap, errmsg);
    if (g_nrows_members > 0) {
        members = g_rows_members;
        nmembers = g_nrows_members;
    } else {
        members = g->members;
        nmembers = g->nmembers;
    }
    if (RP_ONLINE_HAS_SERVER(g)) {
        /* the history entry needs the server's next number and a category */
        category = shell_category();
        if (category[0] == '\0') {
            shell_err_at(SHELL_TAB_RELEASE, g_category_cycle);
            return shell_fail(err, errcap, "Category: choose one on the Release tab first.");
        }
        if (shell_seed_load(g, errmsg, sizeof(errmsg)) != 0) return shell_fail(err, errcap, errmsg);
        if (g_seed.number_text[0]) number = g_seed.number_text;
    } else {
        /* no door counts releases: the number is what the form holds (blank is refused by Build, not here) */
        rp_manual_number(shell_field_text(g_number_field), g_number_text, sizeof(g_number_text), errmsg, sizeof(errmsg));
        number = g_number_text;
    }
    rp_base_name(g->file_prefix, code, base, sizeof(out->base));
    snprintf(packed, sizeof(out->packed), "%s.%s", base, active ? "ZIP" : "LHA");
    snprintf(path, sizeof(path), "PROGDIR:groups/%s/file_id.diz", g->id);
    if (rp_read_template(path, 0, &diz, errmsg, sizeof(errmsg)) != 0) return shell_fail(err, errcap, errmsg);
    width = 0;
    for (i = 0; i < nmembers; i++) {
        size_t nl = strlen(members[i].nickname);
        if (nl > width) width = nl;
    }
    for (i = 0; i < nmembers; i++) {
        snprintf(membuf[i], sizeof(membuf[i]), "%-*s  %s", (int)(width + 2), members[i].nickname,
                 members[i].role);
        memberlines[i] = membuf[i];
    }
    ncredits = shell_credits();
    for (i = 0; i < ncredits; i++) {
        snprintf(creditbuf[i], sizeof(creditbuf[i]), "%s: %s", g_credits[i].role, g_credits[i].name);
        creditlines[i] = creditbuf[i];
    }
    shell_take(creators, sizeof(creators), rp_creators_from_credits(g_credits, ncredits));
    shell_take(coder, sizeof(coder), rp_credit_names_for_role(g_credits, ncredits, RP_ROLE_CODER));
    shell_take(design, sizeof(design), rp_credit_names_for_role(g_credits, ncredits, RP_ROLE_DESIGN));
    scalars[0].name = "group";
    scalars[0].value = g->label;
    scalars[1].name = "url";
    scalars[1].value = g->url;
    scalars[2].name = "code";
    scalars[2].value = code;
    scalars[3].name = "filename";
    scalars[3].value = packed;
    scalars[4].name = "format";
    scalars[4].value = active ? "ZIP" : "LHA";
    scalars[5].name = "release_name";
    scalars[5].value = name_txt;
    scalars[6].name = "platform";
    scalars[6].value = plat_txt;
    scalars[7].name = "date";
    scalars[7].value = date;
    scalars[8].name = "release_number";
    scalars[8].value = number;
    scalars[9].name = "description";
    scalars[9].value = desc_txt;
    scalars[10].name = "creators";
    scalars[10].value = creators;
    scalars[11].name = "released_at";
    scalars[11].value = shell_field_text(g_released_field);
    blocks[0].name = "notes";
    blocks[0].lines = &no_lines;
    blocks[0].nlines = 0;
    blocks[1].name = "news";
    blocks[1].lines = &no_lines;
    blocks[1].nlines = 0;
    blocks[2].name = "members";
    blocks[2].lines = memberlines;
    blocks[2].nlines = nmembers;
    blocks[3].name = "credits";
    blocks[3].lines = creditlines;
    blocks[3].nlines = ncredits;
    v.scalars = scalars;
    v.nscalars = 12;
    v.blocks = blocks;
    v.nblocks = 4;
    /* The NFO: the group's own template, Up Rough's plain layout, or an
     * honest stop for /X (its framed drawing needs the catalog: phase 2). */
    if (g->renderer == R_PLACEHOLDER) {
        char nfo_path[256], *tpl = NULL;
        snprintf(nfo_path, sizeof(nfo_path), "PROGDIR:groups/%s/release.nfo", g->id);
        if (rp_read_template(nfo_path, 1, &tpl, errmsg, sizeof(errmsg)) != 0) {
            free(diz);
            return shell_fail(err, errcap, errmsg);
        }
        if (tpl != NULL) {
            nfo = rp_render_template(tpl, &v, &errline, errmsg, sizeof(errmsg));
            free(tpl);
            if (nfo == NULL) {
                free(diz);
                return shell_fail(err, errcap, errmsg);
            }
        }
    } else if (g->renderer == R_UPROUGH) {
        rp_history hist;
        rp_nfo_form nf;
        char lower[72];
        rp_lower(packed, lower, sizeof(lower));
        hist.number = number;
        hist.filenames = lower;
        hist.title = name_txt;
        hist.category = category;
        hist.creators = creators;
        nf.release_name = name_txt;
        nf.platform = plat_txt;
        nf.code = code;
        nf.format_lha = active ? 0 : 1;
        nf.release_date = date;
        nf.notes = shell_notes_text();
        nf.news = shell_news_text();
        nf.members = members;
        nf.nmembers = nmembers;
        nfo = rp_render_nfo_uprough(g->file_prefix, &hist, 1, &nf, errmsg, sizeof(errmsg));
        if (nfo == NULL) {
            free(diz);
            return shell_fail(err, errcap, errmsg[0] ? errmsg : "out of memory");
        }
    } else {
        /* /X: the framed drawing. The release list comes from the catalog
         * (phase 2), so it is empty here; the boards are the Roster tab's. */
        rp_nfo_x_form xf;
        memset(&xf, 0, sizeof xf);
        xf.release_name = name_txt;
        xf.release_date = date;
        xf.coder = coder;
        xf.system = shell_system_text(plat_txt);
        xf.design = design;
        xf.notes = shell_notes_text();
        xf.news = shell_news_text();
        xf.members = members;
        xf.nmembers = nmembers;
        for (i = 0; i < g_nrows_boards; i++) {
            xboards[i].name = g_rows_boards[i].name;
            xboards[i].sysop = g_rows_boards[i].sysop;
            xboards[i].hq = g_rows_boards[i].hq;
            xboards[i].status = g_rows_boards[i].status;
        }
        xf.boards = xboards;
        xf.nboards = g_nrows_boards;
        nfo = rp_render_nfo_x(&xf);
        if (nfo == NULL) {
            free(diz);
            return shell_fail(err, errcap, "out of memory");
        }
    }
    rendered = rp_render_template(diz, &v, &errline, errmsg, sizeof(errmsg));
    free(diz);
    if (rendered == NULL) {
        free(nfo);
        return shell_fail(err, errcap, errmsg);
    }
    if (g->renderer != R_PLACEHOLDER) {
        /* banner art: Released at turns the release line into "<name> at <place>" (placeholder groups use {{released_at}}) */
        char *at = rp_diz_with_line(rendered, name_txt, shell_field_text(g_released_field), shell_field_text(g_diz_line_field));
        free(rendered);
        rendered = at;
        if (rendered == NULL) {
            free(nfo);
            return shell_fail(err, errcap, "out of memory");
        }
    }
    out->g = g;
    out->zip = active ? 1 : 0;
    out->number = number;
    out->nfo = nfo;
    out->diz = rendered;
    return 0;
}

static void shell_build(Object *app, Object *status, const rp_group *groups, Object *group_cycle, Object *name_field,
                        Object *code_field, Object *platform_field, Object *desc_field, Object *format_cycle, int dry) {
    shell_rendered r;
    char errmsg[256];
    const rp_group *g;
    const char *base = r.base, *packed = r.packed, *number;
    char *nfo, *rendered;
    int active;
    /* A dry run (desktop buildRelease dryRun) writes the same archive and drawer into PROGDIR:dry-run, replacing the last one, and never touches the publish gate. */
    const char *root = dry ? "PROGDIR:dry-run" : "PROGDIR:releases";
    if (!dry) rp_gate_clear(&g_gate);
    /* desktop main.ts: the release needs a name and at least one file or folder */
    if (shell_field_text(name_field)[0] == '\0') {
        shell_err_at(SHELL_TAB_RELEASE, name_field);
        SetAttrs(status, UPA_Text_Contents, (ULONG) "Name: enter the release name on the Release tab first.", TAG_DONE);
        return;
    }
    /* desktop: the Description input and the NFO text area are required */
    switch (rp_build_required(shell_field_text(desc_field), shell_area_raw(g_notes_field), errmsg, sizeof(errmsg))) {
    case RP_REQUIRED_DESCRIPTION:
        shell_err_at(SHELL_TAB_RELEASE, desc_field);
        SetAttrs(status, UPA_Text_Contents, (ULONG)errmsg, TAG_DONE);
        return;
    case RP_REQUIRED_NOTES:
        shell_err_at(SHELL_TAB_NOTES, g_notes_field);
        SetAttrs(status, UPA_Text_Contents, (ULONG)errmsg, TAG_DONE);
        return;
    default:
        break;
    }
    {   /* a group without a server has no number to count: the typed one must be usable */
        ULONG ga = 0;
        char num[sizeof(g_number_text)];
        GetAttr(UPA_Cycle_Active, group_cycle, &ga);
        if (!RP_ONLINE_HAS_SERVER(&groups[ga]) &&
            rp_manual_number(shell_field_text(g_number_field), num, sizeof(num), errmsg, sizeof(errmsg)) != 0) {
            shell_err_at(SHELL_TAB_RELEASE, g_number_field);
            SetAttrs(status, UPA_Text_Contents, (ULONG)errmsg, TAG_DONE);
            return;
        }
    }
    if (g_paths.n == 0) {
        shell_err_at(SHELL_TAB_FILES, NULL);
        SetAttrs(status, UPA_Text_Contents, (ULONG) "Files: add a drawer or file on the Files tab first.", TAG_DONE);
        return;
    }
    if (shell_render_release(groups, group_cycle, name_field, code_field, platform_field, desc_field, format_cycle, &r,
                             errmsg, sizeof(errmsg)) != 0) {
        SetAttrs(status, UPA_Text_Contents, (ULONG)errmsg, TAG_DONE);
        return;
    }
    g = r.g;
    active = r.zip;
    number = r.number;
    nfo = r.nfo;
    rendered = r.diz;
    /* Staged like the desktop: outer/ holds file_id.diz next to BASE/,
     * which holds BASE.NFO and the files. Then the archive, then the whole
     * outer drawer moves into PROGDIR:releases/BASE. */
    {
        char outer[288], inner[352], work[416], nfo_name[384], archive[384], *cmd, done[448];
        struct stat st;
        shell_phase(app, status, 1, "Staging the release drawer...");
        snprintf(outer, sizeof(outer), "T:rp-%s", base);
        snprintf(inner, sizeof(inner), "%s/%s", outer, base);
        if (rp_make_dirs(inner, errmsg, sizeof(errmsg)) != 0) goto build_fail;
        snprintf(work, sizeof(work), "%s/file_id.diz", outer);
        if (rp_write_file(work, rendered, strlen(rendered), errmsg, sizeof(errmsg)) != 0) goto build_fail;
        free(rendered);
        rendered = NULL;
        if (nfo != NULL) {
            shell_phase(app, status, 2, "Writing the NFO...");
            snprintf(nfo_name, sizeof(nfo_name), "%s/%s.NFO", inner, base);
            if (rp_write_file(nfo_name, nfo, strlen(nfo), errmsg, sizeof(errmsg)) != 0) goto build_fail;
            free(nfo);
            nfo = NULL;
        }
        /* The picked files and drawers. A picked drawer that is itself
         * called BASE is the release the user already assembled by
         * hand: its contents belong directly in the release drawer. */
        if (g_paths.n > 0) {
            size_t pi;
            shell_phase(app, status, 3, "Copying the files...");
            for (pi = 0; pi < g_paths.n; pi++) {
                const char *src = g_paths.path[pi];
                DIR *scan = same_name_fold(base_name_of(src), base) ? opendir(src) : NULL;
                /* Only a drawer named like the release is merged; a file of that name is copied as a file. */
                if (scan != NULL) {
                    struct dirent *e;
                    while ((e = readdir(scan)) != NULL) {
                        char s[576], t[576];
                        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
                        snprintf(s, sizeof(s), "%s/%s", src, e->d_name);
                        snprintf(t, sizeof(t), "%s/%s", inner, e->d_name);
                        if (rp_copy_tree(s, t, errmsg, sizeof(errmsg)) != 0) {
                            closedir(scan);
                            goto build_fail;
                        }
                    }
                    closedir(scan);
                } else {
                    char t[576];
                    snprintf(t, sizeof(t), "%s/%s", inner, base_name_of(src));
                    if (rp_copy_tree(src, t, errmsg, sizeof(errmsg)) != 0) goto build_fail;
                }
            }
        }
        if (rp_make_dirs(root, errmsg, sizeof(errmsg)) != 0) goto build_fail;
        if (dry) {
            /* a preview is disposable: the newest replaces the last instead of refusing it */
            char old[384];
            snprintf(old, sizeof(old), "%s/%s", root, packed);
            remove(old);
            snprintf(old, sizeof(old), "%s/%s", root, base);
            if (rp_remove_tree(old, errmsg, sizeof(errmsg)) != 0) goto build_fail;
        }
        snprintf(archive, sizeof(archive), "%s/%s", root, packed);
        if (stat(archive, &st) == 0) {
            snprintf(errmsg, sizeof(errmsg), "%s already exists - move it away first", packed);
            goto build_fail;
        }
        {
            char finaldir[384];
            snprintf(finaldir, sizeof(finaldir), "%s/%s", root, base);
            if (stat(finaldir, &st) == 0) {
                snprintf(errmsg, sizeof(errmsg), "%s already exists in %s", base, root);
                goto build_fail;
            }
        }
        {
            /* PROGDIR: belongs to each process: the archiver would resolve it to its own drawer. */
            char abs_archive[384];
            BPTR rl = Lock((STRPTR)root, ACCESS_READ);
            if (rl == 0 || !NameFromLock(rl, (STRPTR)abs_archive, sizeof(abs_archive) - strlen(packed) - 1)) {
                if (rl != 0) UnLock(rl);
                snprintf(errmsg, sizeof(errmsg), "%s: could not be resolved", root);
                goto build_fail;
            }
            UnLock(rl);
            AddPart((STRPTR)abs_archive, (STRPTR)packed, sizeof(abs_archive));
            cmd = rp_archive_cmd(active ? "zip" : "lha", abs_archive, base);
        }
        if (cmd == NULL) {
            snprintf(errmsg, sizeof(errmsg), "out of memory");
            goto build_fail;
        }
        {
            char packing[96];
            snprintf(packing, sizeof(packing), "Packing %s...", packed);
            shell_phase(app, status, 4, packing);
        }
        if (rp_run_command(outer, cmd, errmsg, sizeof(errmsg)) != 0) {
            free(cmd);
            goto build_fail;
        }
        free(cmd);
        /* The whole outer drawer (DIZ next to BASE/) becomes
         * PROGDIR:releases/BASE (PROGDIR:dry-run/BASE for a dry run), exactly like the desktop finalize. */
        {
            char finaldir[384];
            snprintf(finaldir, sizeof(finaldir), "%s/%s", root, base);
            shell_phase(app, status, 5, "Moving the release drawer...");
            if (rp_move_dir(outer, finaldir, errmsg, sizeof(errmsg)) != 0) goto build_fail;
        }
        if (dry) {
            snprintf(done, sizeof(done), "Dry run: %s written, nothing published", archive);
        } else {
            rp_gate_mark(&g_gate, shell_form_state(group_cycle, name_field, code_field, platform_field, desc_field, format_cycle));
            if (RP_ONLINE_HAS_SERVER(g)) snprintf(done, sizeof(done), "Built %s (release %s)", archive, number);
            else snprintf(done, sizeof(done), "Built %s", archive);
        }
        SetAttrs(status, UPA_Text_Contents, (ULONG)done, TAG_DONE);
        goto build_end;
    build_fail:
        if (dry) {
            /* a read-only PROGDIR: or a missing drawer names itself in errmsg; the prefix says it was the preview's archive */
            char shown[sizeof(errmsg) + 40];
            snprintf(shown, sizeof(shown), "Preview failed: %s", errmsg);
            SetAttrs(status, UPA_Text_Contents, (ULONG)shown, TAG_DONE);
        } else SetAttrs(status, UPA_Text_Contents, (ULONG)errmsg, TAG_DONE);
    build_end:
        SetAttrs(g_gauge, MUIA_ShowMe, (ULONG)FALSE, TAG_DONE);
        {
            /* The staging drawer never outlives a Build (after a move it is already gone). */
            char rmerr[128];
            rp_remove_tree(outer, rmerr, sizeof(rmerr));
        }
        free(rendered);
        free(nfo);
        return;
    }
}

/* The Preview tab: the file_id.diz and NFO Build would write, made by the same shell_render_release. */
/* The release line counter (desktop renderDizPreview): the open division's row label and slot width, text = typed Diz line, else name at place. */
static void shell_diz_count(Object *name_field) {
    char msg[160];
    const rp_division *d = g_division;
    if (d == NULL) return; /* a placeholder group has no release line row */
    rp_diz_line_counter(d->rows[0].label, rp_diz_slot_width(d->banner, d->rows[0].row), shell_field_text(g_diz_line_field), shell_field_text(name_field),
                        shell_field_text(g_released_field), msg, sizeof msg);
    SetAttrs(g_diz_count, UPA_Text_Contents, (ULONG)msg, TAG_DONE);
}

static void shell_preview_nfo(Object *status, const rp_group *groups, Object *group_cycle, Object *name_field, Object *code_field,
                              Object *platform_field, Object *desc_field, Object *format_cycle) {
    shell_rendered r;
    char errmsg[256], text[SHELL_NFO_VIEW_MAX + 1], msg[160];
    size_t full;
    ULONG gactive = 0;
    GetAttr(UPA_Cycle_Active, group_cycle, &gactive);
    if (shell_render_release(groups, group_cycle, name_field, code_field, platform_field, desc_field, format_cycle, &r, errmsg,
                             sizeof(errmsg)) != 0) {
        SetAttrs(status, UPA_Text_Contents, (ULONG)errmsg, TAG_DONE);
        if (groups[gactive].renderer == R_PLACEHOLDER) SetAttrs(g_template_status, UPA_Text_Contents, (ULONG)errmsg, TAG_DONE);
        return;
    }
    full = rp_release_preview_view(r.diz, r.nfo, text, sizeof text);
    SetAttrs(g_nfo_view, UPA_TextArea_Text, (ULONG)text, TAG_DONE);
    if (full > SHELL_NFO_VIEW_MAX) snprintf(msg, sizeof(msg), "Preview cut at %d of %lu bytes - Build writes the whole NFO.", SHELL_NFO_VIEW_MAX, (unsigned long)full);
    else snprintf(msg, sizeof(msg), "Preview of file_id.diz and the NFO (%lu bytes).", (unsigned long)full);
    SetAttrs(status, UPA_Text_Contents, (ULONG)msg, TAG_DONE);
    if (r.g->renderer == R_PLACEHOLDER) SetAttrs(g_template_status, UPA_Text_Contents, (ULONG)msg, TAG_DONE);
    free(r.nfo);
    free(r.diz);
}

/* rp_net_pump while a transfer runs: services the window (redraw, Cancel, Quit), shows the send progress.
 * Other button ids are dropped: the form is disabled for the transfer. Returns nonzero to cancel. */
static int shell_net_pump(unsigned long done, unsigned long total) {
    ULONG sigs = 0, id = DoMethod(g_net_app, MUIM_Application_NewInput, (ULONG)&sigs);
    unsigned long pct = total > 0 ? (done >= total ? 100UL : (unsigned long)((unsigned long long)done * 100ULL / total)) : ~0UL;
    if (id == ID_CANCEL) g_net_cancel = 1;
    if (id == (ULONG)MUIV_Application_ReturnID_Quit || (SetSignal(0, 0) & SIGBREAKF_CTRL_C) != 0) g_net_cancel = g_net_quit = 1;
    if (pct != g_net_shown) {
        char line[64];
        g_net_shown = pct;
        if (total > 0) {
            snprintf(line, sizeof line, "Sending %lu of %lu KB...", done / 1024UL, (total + 1023UL) / 1024UL);
            SetAttrs(g_gauge, UPA_Progress_Value, (ULONG)pct, MUIA_ShowMe, (ULONG)TRUE, TAG_DONE);
        } else snprintf(line, sizeof line, "Waiting for the server...");
        SetAttrs(g_net_status, UPA_Text_Contents, (ULONG)line, TAG_DONE);
    }
    return g_net_cancel;
}

/* Starts (on) or ends a transfer: pump on, form off, Cancel shown; at the end a cancel replaces the result line. */
static void shell_net_busy(int on) {
    if (on) g_net_cancel = 0, g_net_shown = ~0UL - 1;
    rp_net_pump = on ? shell_net_pump : NULL;
    SetAttrs(g_net_pages, MUIA_Disabled, (ULONG)on, TAG_DONE);
    SetAttrs(g_net_cancel_btn, MUIA_ShowMe, (ULONG)on, TAG_DONE);
    if (!on) SetAttrs(g_gauge, MUIA_ShowMe, (ULONG)FALSE, TAG_DONE);
    if (!on && g_net_cancel)
        SetAttrs(g_net_status, UPA_Text_Contents, (ULONG) "Cancelled. If the whole archive was already sent, the server may still have it - check before trying again.", TAG_DONE);
}

/* Publish / Revoke / Door repository. Runs inside the main loop's shell_net_busy: the transport pumps
 * the window between socket chunks, so it stays live and Cancel works. */
static int shell_online(Object *app, Object *status, int action, const rp_group *groups, Object *group_cycle,
                         Object *name_field, Object *code_field, Object *platform_field, Object *desc_field,
                         Object *format_cycle) {
    ULONG gactive = 0, active = 0, sigs = 0;
    const rp_group *g;
    char code[40], base[64], packed[72], archive[160], diz[160], msg[400], pub_creators[256];
    int rc = -1;
    struct DateStamp ds;
    unsigned long seed;
    GetAttr(UPA_Cycle_Active, group_cycle, &gactive);
    GetAttr(UPA_Cycle_Active, format_cycle, &active);
    g = &groups[gactive];
    rp_upper(shell_field_text(code_field), code, sizeof(code));
    rp_base_name(g->file_prefix, code, base, sizeof(base));
    snprintf(packed, sizeof(packed), "%s.%s", base, active ? "ZIP" : "LHA");
    snprintf(archive, sizeof(archive), "PROGDIR:releases/%s", packed);
    snprintf(diz, sizeof(diz), "PROGDIR:releases/%s/file_id.diz", base);
    DateStamp(&ds);
    seed = (unsigned long)ds.ds_Days * 4321UL + (unsigned long)ds.ds_Minute * 3000UL + (unsigned long)ds.ds_Tick;
    if (action == ID_REVOKE) {
        char rc_txt[40];
        rp_upper(shell_field_text(g_revoke_field), rc_txt, sizeof(rc_txt));
        if (rc_txt[0] != '\0') strcpy(code, rc_txt); /* the Revoke code field wins over the Code field */
    }
    if (action == ID_REVOKE && (code[0] == '\0' || !shell_confirm_revoke(code))) {
        SetAttrs(status, UPA_Text_Contents, (ULONG)(code[0] ? "Revoke cancelled." : "Enter the code of the release to revoke."), TAG_DONE);
        return -1;
    }
    SetAttrs(status, UPA_Text_Contents, (ULONG) "Working...", TAG_DONE);
    DoMethod(app, MUIM_Application_NewInput, (ULONG)&sigs);
    if (action == ID_PUBLISH || action == ID_DOOR) {
        if (!rp_gate_open(&g_gate, shell_form_state(group_cycle, name_field, code_field, platform_field, desc_field, format_cycle))) {
            SetAttrs(status, UPA_Text_Contents, (ULONG)(g_gate.built ? "The form changed since Build - press Build again first." : "Press Build first: Publish unlocks after a Build in this session."), TAG_DONE);
            return -1;
        }
    }
    if (action == ID_PUBLISH) {
        rp_online_publish_args a;
        if (shell_roster_read(g, msg, sizeof(msg)) != 0) {
            SetAttrs(status, UPA_Text_Contents, (ULONG)msg, TAG_DONE);
            return -1;
        }
        memset(&a, 0, sizeof a);
        a.members = g_rows_members; /* n_members 0 = the group's own list */
        a.n_members = g_nrows_members;
        a.boards = g_rows_boards;
        a.n_boards = g_nrows_boards;
        a.embeds = g_rows_embeds;
        a.n_embeds = g_nrows_embeds;
        a.group = g;
        a.code = code;
        a.release_name = shell_field_text(name_field);
        a.platform_tag = shell_platform_tag(platform_field);
        a.description = shell_field_text(desc_field);
        a.release_notes = shell_notes_text();
        a.news = shell_news_text();
        a.category = shell_category();
        a.n_credits = shell_credits();
        a.credits = g_credits;
        shell_take(pub_creators, sizeof(pub_creators), rp_creators_from_credits(g_credits, a.n_credits));
        a.creators = pub_creators;
        a.format = active ? "ZIP" : "LHA";
        a.diz_path = diz;
        a.archive_path = archive;
        a.image_path = g_cover;
        a.seed = seed;
        a.http = rp_http_amiga;
        rc = rp_online_publish(&a, msg, sizeof(msg));
    } else if (action == ID_REVOKE) {
        rp_online_revoke(g, code, rp_http_amiga, NULL, msg, sizeof(msg));
    } else {
        rp_online_door(g, archive, seed, rp_http_amiga, NULL, msg, sizeof(msg));
    }
    SetAttrs(status, UPA_Text_Contents, (ULONG)msg, TAG_DONE);
    return rc;
}

/* Aminet upload (preview_only: show the readme and problems in the Readme box, upload nothing): the readme comes from the form (Aminet tab) and the group, the archive is the one
 * Build wrote. The upload runs inside shell_net_busy like shell_online; the preview touches no network. */
static void shell_aminet(Object *app, Object *status, const rp_group *groups, Object *group_cycle,
                         Object *name_field, Object *platform_field, Object *code_field, Object *desc_field, Object *format_cycle, Object *type_field,
                         Object *arch_field, Object *uname_field, Object *email_field, int preview_only) {
    ULONG gactive = 0, active = 0, sigs = 0;
    const rp_group *g;
    char code[40], base[64], packed[72], archive[160], creators[256], msg[400];
    ULONG arch_active = 0;
    rp_aminet_credit credits[RP_MEMBERS_MAX > RP_CREDITS_MAX ? RP_MEMBERS_MAX : RP_CREDITS_MAX];
    size_t ncredits;
    rp_aminet_readme_in in;
    rp_aminet_preview prev;
    char *data = NULL;
    FILE *f;
    long len;
    size_t i;
    memset(&prev, 0, sizeof prev);
    GetAttr(UPA_Cycle_Active, group_cycle, &gactive);
    GetAttr(UPA_Cycle_Active, format_cycle, &active);
    g = &groups[gactive];
    rp_upper(shell_field_text(code_field), code, sizeof(code));
    rp_base_name(g->file_prefix, code, base, sizeof(base));
    snprintf(packed, sizeof(packed), "%s.%s", base, active ? "ZIP" : "LHA");
    snprintf(archive, sizeof(archive), "PROGDIR:releases/%s", packed);
    SetAttrs(status, UPA_Text_Contents, (ULONG) "Working...", TAG_DONE);
    DoMethod(app, MUIM_Application_NewInput, (ULONG)&sigs);
    creators[0] = '\0';
    ncredits = shell_credits();
    if (ncredits > 0) {
        /* the Authors field is the one place a release's people are entered */
        for (i = 0; i < ncredits; i++) {
            credits[i].role = g_credits[i].role;
            credits[i].name = g_credits[i].name;
        }
        shell_take(creators, sizeof(creators), rp_creators_from_credits(g_credits, ncredits));
    } else {
        /* no authors entered: credit the Roster tab's members, else the group's own */
        const rp_member *mem = g->members;
        size_t nmem = g->nmembers;
        if (shell_roster_read(g, msg, sizeof(msg)) != 0) {
            SetAttrs(status, UPA_Text_Contents, (ULONG)msg, TAG_DONE);
            return;
        }
        if (g_nrows_members > 0) {
            mem = g_rows_members;
            nmem = g_nrows_members;
        }
        ncredits = nmem;
        for (i = 0; i < nmem; i++) {
            credits[i].role = mem[i].role;
            credits[i].name = mem[i].nickname;
            if (strlen(creators) + strlen(mem[i].nickname) + 3 < sizeof(creators)) {
                if (i > 0) strcat(creators, ", ");
                strcat(creators, mem[i].nickname);
            }
        }
    }
    memset(&in, 0, sizeof in);
    if (shell_field_text(g_short_field)[0] == '\0') {
        char sh[RP_AMINET_SHORT_MAX + 1];
        rp_aminet_short_prefill(shell_field_text(desc_field), sh, sizeof sh);
        shell_set_field(g_short_field, sh);
    }
    in.short_text = shell_field_text(g_short_field);
    in.type = shell_type_dir(type_field);
    GetAttr(UPA_Cycle_Active, arch_field, &arch_active);
    in.architecture = rp_aminet_architectures[arch_active <= 7 ? arch_active : 0];
    in.uploader_email = shell_field_text(email_field);
    in.uploader_name = shell_field_text(uname_field);
    in.group_label = g->label;
    in.group_url = g->url;
    in.entry_creators = creators;
    in.description = shell_field_text(desc_field);
    in.release_notes = shell_notes_text();
    in.credits = credits;
    in.n_credits = ncredits;
    {
        rp_aminet_prefs pr;
        memset(&pr, 0, sizeof pr);
        strncpy(pr.directory, in.type, sizeof(pr.directory) - 1);
        strncpy(pr.architecture, in.architecture, sizeof(pr.architecture) - 1);
        strncpy(pr.uploader, in.uploader_name, sizeof(pr.uploader) - 1);
        strncpy(pr.email, in.uploader_email, sizeof(pr.email) - 1);
        rp_aminet_prefs_save(RP_AMINET_PREFS_PATH, &pr);
    }
    if (rp_aminet_make_preview(packed, &in, &prev) != 0) {
        snprintf(msg, sizeof(msg), "Aminet readme could not be prepared: %s", prev.problems);
        goto done;
    }
    if (preview_only) {
        char *view = rp_aminet_preview_view(&prev);
        SetAttrs(g_readme_view, UPA_TextArea_Text, (ULONG)(view ? view : ""), TAG_DONE);
        free(view);
        snprintf(msg, sizeof(msg), prev.n_problems > 0 ? "Aminet readme: %d problem(s) - see the Readme box." : "Aminet readme looks fine - see the Readme box.", prev.n_problems);
        goto done;
    }
    if (!rp_gate_open(&g_gate, shell_form_state(group_cycle, name_field, code_field, platform_field, desc_field, format_cycle))) {
        snprintf(msg, sizeof(msg), "%s", g_gate.built ? "Not uploaded - the form changed since Build, press Build again first." : "Not uploaded - press Build first: the upload needs a Build in this session.");
        goto done;
    }
    if (prev.n_problems > 0) {
        snprintf(msg, sizeof(msg), "Not uploaded - %s", prev.problems);
        goto done;
    }
    f = fopen(archive, "rb");
    if (f == NULL) {
        snprintf(msg, sizeof(msg), "%s not found - press Build first.", packed);
        goto done;
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = len > 0 ? (char *)malloc((size_t)len) : NULL;
    if (data == NULL || fread(data, 1, (size_t)len, f) != (size_t)len) {
        fclose(f);
        snprintf(msg, sizeof(msg), "Could not read %s.", packed);
        goto done;
    }
    fclose(f);
    rp_aminet_upload(&rp_tcp_amiga_ops, NULL, packed, in.uploader_email, prev.readme, data, (size_t)len, msg, sizeof(msg));
done:
    free(data);
    rp_aminet_preview_free(&prev);
    SetAttrs(status, UPA_Text_Contents, (ULONG)msg, TAG_DONE);
}

/* Event ids above MUIV_Application_ReturnID_Quit are ours. */
#define RP_MAX_SHELL_GROUPS 16

static rp_group g_groups[RP_MAX_SHELL_GROUPS];
static const char *g_group_labels[RP_MAX_SHELL_GROUPS + 1];
static int g_ngroups;

/* (Re)loads the group drawers into g_groups and g_group_labels. One bad drawer never hides the others; its
 * message goes to broken. Returns -1 (reason in broken) when the group directory cannot be listed, else 0. */
static int shell_load_groups(char *broken, size_t cap) {
    char ids[RP_MAX_SHELL_GROUPS][RP_ID_MAX + 1];
    char errmsg[256];
    int i, n;
    broken[0] = '\0';
    g_ngroups = 0;
    g_group_labels[0] = NULL;
    n = rp_list_groups("PROGDIR:groups", ids, RP_MAX_SHELL_GROUPS, errmsg, sizeof(errmsg));
    if (n < 0) {
        strncpy(broken, errmsg, cap - 1);
        broken[cap - 1] = '\0';
        return -1;
    }
    for (i = 0; i < n; i++) {
        if (rp_load_group("PROGDIR:groups", ids[i], &g_groups[g_ngroups], errmsg, sizeof(errmsg)) != 0) {
            strncat(broken, errmsg, cap - strlen(broken) - 1);
            strncat(broken, "\n", cap - strlen(broken) - 1);
            continue;
        }
        g_group_labels[g_ngroups] = g_groups[g_ngroups].label;
        g_ngroups++;
    }
    g_group_labels[g_ngroups] = NULL;
    return 0;
}

/* The code follows the release name until the user types their own (the desktop Prefill rule). */
static char g_code_filled[40];

static void shell_suggest_code(Object *name_field, Object *code_field, Object *group_cycle) {
    ULONG np = 0, cp = 0, ga = 0;
    STRPTR name = (STRPTR) "", cur = (STRPTR) "";
    char sug[40];
    GetAttr(UPA_Field_String, name_field, &np);
    GetAttr(UPA_Field_String, code_field, &cp);
    if (np == 0 || cp == 0) return;
    GetAttr(MUIA_String_Contents, (Object *)np, (ULONG *)&name);
    GetAttr(MUIA_String_Contents, (Object *)cp, (ULONG *)&cur);
    if (name == NULL) name = (STRPTR) "";
    if (cur == NULL) cur = (STRPTR) "";
    if (cur[0] != '\0' && strcmp((const char *)cur, g_code_filled) != 0) return;
    GetAttr(UPA_Cycle_Active, group_cycle, &ga);
    rp_suggest_code((const char *)name, ga < (ULONG)g_ngroups ? g_groups[ga].code_length : 5, sug, sizeof(sug));
    strcpy(g_code_filled, sug);
    SetAttrs((Object *)cp, MUIA_String_Contents, (ULONG)sug, TAG_DONE);
}


static int shell_main(void) {
    Object *app = NULL, *win = NULL;
    Object *group_slot, *edit_groups_btn;
    Object *group_cycle, *name_field, *code_field, *platform_field, *desc_field, *format_cycle;
    ULONG open_group = 0;
    Object *files_list, *add_file_btn, *remove_file_btn, *pick_btn, *build_btn, *status;
    Object *embed_pick_btn, *cover_btn, *publish_btn, *revoke_btn, *door_btn, *cover_text;
    Object *aminet_btn, *aminet_preview_btn, *nfo_preview_btn, *type_field, *arch_field, *uname_field, *email_field;
    Object *tabs, *pages, *preview_btn, *quit_btn, *back_btn, *next_btn, *flatten_btn, *preview_list, *warn_text, *warn_row;
    ULONG sigs = 0, is_open = 0;
    static const char *const formats[] = { "LHA", "ZIP", NULL };
    char broken[1024];
    int i;

    MUIMasterBase = OpenLibrary((CONST_STRPTR) "muimaster.library", 19);
    if (MUIMasterBase == NULL) {
        shell_requester("muimaster.library 19 (MUI 3.8) is not installed.\nInstall MUI 3.8, then start Release Packer again.");
        return 20;
    }
    if (shell_load_groups(broken, sizeof(broken)) < 0) {
        shell_requester(broken);
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    if (g_ngroups == 0) {
        shell_requester(broken[0] ? broken : "No groups found.\nPut your group drawers in PROGDIR:groups, then start again.");
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    group_cycle = MUI_NewObject((char *)"UP-Cycle.mcc", UPA_Cycle_Entries, (ULONG)g_group_labels, TAG_DONE);
    edit_groups_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Edit groups", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre,
                                    UPA_Button_Role, (ULONG)UPV_Button_Role_Normal, MUIA_Weight, 1UL, TAG_DONE);
    group_slot = MUI_NewObject((char *)MUIC_Group, MUIA_Group_Horiz, (ULONG)TRUE, MUIA_FillArea, (ULONG)FALSE,
                               MUIA_Group_Child, (ULONG)group_cycle, MUIA_Group_Child, (ULONG)edit_groups_btn, TAG_DONE);
    name_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Release name", TAG_DONE);
    {   /* longest code any group allows; the exact length per group is still checked at Build */
        int i, maxcode = 1;
        for (i = 0; i < g_ngroups; i++) if (g_groups[i].code_length > maxcode) maxcode = g_groups[i].code_length;
        code_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "e.g. ABC", MUIA_String_MaxLen, (ULONG)maxcode + 1, TAG_DONE);
    }
    platform_field = MUI_NewObject((char *)"UP-Cycle.mcc", UPA_Cycle_Entries, (ULONG)rp_platform_labels, TAG_DONE);
    g_platform_text_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Which console, or CD32", MUIA_String_MaxLen, (ULONG)(sizeof(g_platform_tag_buf)), TAG_DONE);
    g_platform_row = g_platform_text_field ? shell_row(row_label("Platform detail"), g_platform_text_field) : NULL;
    g_short_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Aminet index line", MUIA_String_MaxLen, (ULONG)(RP_AMINET_SHORT_MAX + 1), TAG_DONE);
    desc_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "One-line description", TAG_DONE);
    format_cycle =
        MUI_NewObject((char *)"UP-Cycle.mcc", UPA_Cycle_Entries, (ULONG)formats, TAG_DONE);
    *(void **)&g_paths_hook.h_Entry = (void *)rp_paths_row_tramp;
    files_list = MUI_NewObject((char *)"UP-List.mcc", UPA_List_RowHook, (ULONG)&g_paths_hook, UPA_List_MaxRows, 4UL,
                               UPA_List_Count, 4UL, /* height from the row count at open, emptied after open */
                               TAG_DONE);
    add_file_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Add file...", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    remove_file_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Remove selected", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    pick_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Add drawer...", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    build_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Build", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, UPA_Button_Role,
                              (ULONG)UPV_Button_Role_Accent, TAG_DONE);
    preview_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Preview", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    quit_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Role, (ULONG)UPV_Button_Role_Ghost, UPA_Button_Icon, (ULONG) "x",
                             UPA_Button_IconOnly, (ULONG)TRUE, UPA_Button_Flat, (ULONG)TRUE, UPA_Button_FixedWidth, 24UL,
                             UPA_Button_FixedHeight, 24UL, TAG_DONE);
    back_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Back", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, MUIA_Disabled, (ULONG)TRUE, TAG_DONE);
    next_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Next", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    g_net_cancel_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Cancel", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, MUIA_ShowMe, (ULONG)FALSE, TAG_DONE);
    cover_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Pick cover...", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    embed_pick_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Pick image...", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    publish_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Publish", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, UPA_Button_Role,
                                (ULONG)UPV_Button_Role_Accent, TAG_DONE);
    g_revoke_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Blank = the Code above", MUIA_String_MaxLen, 32UL, TAG_DONE);
    revoke_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Revoke", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    door_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Door repo", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    g_also_aminet_chk = MUI_NewObject((char *)"UP-Checkmark.mcc", UPA_Checkmark_Label, (ULONG) "Also upload to Aminet", TAG_DONE);
    aminet_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Aminet", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    shell_type_labels_init();
    shell_category_labels_init();
    g_category_lbl = row_label("Category");
    g_category_cycle = MUI_NewObject((char *)"UP-Cycle.mcc", UPA_Cycle_Entries, (ULONG)g_category_labels, TAG_DONE);
    type_field = MUI_NewObject((char *)"UP-Cycle.mcc", UPA_Cycle_Entries, (ULONG)g_type_labels, TAG_DONE);
    arch_field = MUI_NewObject((char *)"UP-Cycle.mcc", UPA_Cycle_Entries, (ULONG)rp_aminet_architectures, TAG_DONE);
    uname_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Your name", TAG_DONE);
    email_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Your email address", TAG_DONE);
    g_authors_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Role: Name, Name; Role: Name", MUIA_String_MaxLen, 255UL, TAG_DONE);
    g_creators_text = shell_label("", UPV_Text_Tone_Muted);
    g_creators_row = g_creators_text ? shell_row(row_label("Creators"), g_creators_text) : NULL;
    g_system_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "aMiEXPRESS", MUIA_String_MaxLen, (ULONG)(sizeof(g_system_prefill) - 1), TAG_DONE);
    g_system_row = g_system_field ? shell_row(row_label("System"), g_system_field) : NULL;
    g_date_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "2026-09-21", MUIA_String_MaxLen, 15UL, TAG_DONE);
    g_diz_line_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Banner release line (optional; blank = name at Released at)", MUIA_String_MaxLen, 100UL, TAG_DONE);
    g_released_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Mysdata 2026 (optional)", MUIA_String_MaxLen, 100UL, TAG_DONE);
    g_number_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "042", MUIA_String_MaxLen, (ULONG)(sizeof(g_number_text) - 1), TAG_DONE);
    g_number_row = g_number_field ? shell_row(row_label("Release number"), g_number_field) : NULL;
    g_readme_view = MUI_NewObject((char *)"UP-Textarea.mcc", UPA_TextArea_MaxLen, (ULONG)SHELL_README_MAX, UPA_TextArea_Placeholder, (ULONG) "Press Preview readme to see the readme and its problems here", TAG_DONE);
    g_nfo_view = MUI_NewObject((char *)"UP-Textarea.mcc", UPA_TextArea_MaxLen, (ULONG)SHELL_NFO_VIEW_MAX, UPA_TextArea_Placeholder, (ULONG) "Press Preview NFO to see the file_id.diz and the NFO here", TAG_DONE);
    nfo_preview_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Preview NFO", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    aminet_preview_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Preview readme", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    g_notes_field = MUI_NewObject((char *)"UP-Textarea.mcc", UPA_TextArea_MaxLen, (ULONG)SHELL_NOTES_MAX, UPA_TextArea_Placeholder, (ULONG) "Release notes, one paragraph per line", TAG_DONE);
    g_news_field = MUI_NewObject((char *)"UP-Textarea.mcc", UPA_TextArea_MaxLen, (ULONG)SHELL_NEWS_MAX, UPA_TextArea_Placeholder, (ULONG) "News", TAG_DONE);
    g_members_field = MUI_NewObject((char *)"UP-Textarea.mcc", UPA_TextArea_MaxLen, (ULONG)SHELL_ROWS_MAX, UPA_TextArea_Placeholder, (ULONG) "Nickname | role, one member per line", TAG_DONE);
    g_boards_field = MUI_NewObject((char *)"UP-Textarea.mcc", UPA_TextArea_MaxLen, (ULONG)SHELL_ROWS_MAX, UPA_TextArea_Placeholder, (ULONG) "Name | Sysop | HQ | Status (X Innovations only)", TAG_DONE);
    g_embeds_field = MUI_NewObject((char *)"UP-Textarea.mcc", UPA_TextArea_MaxLen, (ULONG)SHELL_ROWS_MAX, UPA_TextArea_Placeholder, (ULONG) "youtube | caption | url   or   image | caption | path", TAG_DONE);
    cover_text = shell_label("(no cover image picked)", UPV_Text_Tone_Muted);
    flatten_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Remove extra nested folder", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    warn_text = shell_label("", UPV_Text_Tone_Normal);
    *(void **)&g_preview_hook.h_Entry = (void *)rp_preview_row_tramp;
    preview_list = MUI_NewObject((char *)"UP-List.mcc", UPA_List_RowHook, (ULONG)&g_preview_hook, UPA_List_NoSelect,
                                 (ULONG)TRUE, UPA_List_MaxRows, 8UL,
                                 UPA_List_Count, 6UL, /* the list asks its height from the row count at open: start at 6 rows, emptied after open */
                                 TAG_DONE);
    warn_row = (warn_text && flatten_btn) ? shell_row(warn_text, flatten_btn) : NULL;
    g_gauge = MUI_NewObject((char *)"UP-Progress.mcc", UPA_Progress_Value, 0UL, UPA_Progress_Max, 100UL, MUIA_ShowMe, (ULONG)FALSE, MUIA_FixWidth, 160UL, TAG_DONE);
    g_template_status = shell_label("", UPV_Text_Tone_Muted);
    g_diz_count = shell_label("", UPV_Text_Tone_Muted);
    status = shell_label(broken[0] ? broken : "Fill in the release, then open the Publish tab and press Build.", UPV_Text_Tone_Muted);
    if (group_cycle == NULL || group_slot == NULL || edit_groups_btn == NULL || name_field == NULL || code_field == NULL || platform_field == NULL ||
        desc_field == NULL || format_cycle == NULL || files_list == NULL || add_file_btn == NULL || remove_file_btn == NULL || pick_btn == NULL ||
        build_btn == NULL || back_btn == NULL || next_btn == NULL || g_net_cancel_btn == NULL || status == NULL || g_template_status == NULL || g_diz_count == NULL || g_gauge == NULL || preview_btn == NULL || preview_list == NULL || warn_row == NULL ||
        cover_btn == NULL || embed_pick_btn == NULL || publish_btn == NULL || revoke_btn == NULL || g_revoke_field == NULL || door_btn == NULL || cover_text == NULL ||
        aminet_btn == NULL || g_also_aminet_chk == NULL || type_field == NULL || arch_field == NULL || uname_field == NULL || email_field == NULL || g_notes_field == NULL || g_news_field == NULL || g_members_field == NULL || g_boards_field == NULL || g_embeds_field == NULL || g_authors_field == NULL || g_category_cycle == NULL || g_category_lbl == NULL ||
        g_date_field == NULL || g_released_field == NULL || g_diz_line_field == NULL || g_number_row == NULL || g_creators_row == NULL || g_system_row == NULL || g_platform_row == NULL || g_short_field == NULL || g_readme_view == NULL || aminet_preview_btn == NULL || g_nfo_view == NULL || nfo_preview_btn == NULL) {
        shell_requester("The window objects could not be created.\nFree some memory and check the UP-MCC install, then start again.");
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    {
        /* Tabs like the desktop wizard, over the steps the Amiga has data for; one page shows at a time, so no scrolling */
        Object *page_release = shell_card("Release", shell_form(row_label("Group"), group_slot, row_label("Name"), name_field,
                                                                  row_label("Code"), code_field, row_label("Platform"), platform_field,
                                                                  row_label("Format"), format_cycle, row_label("Description"), desc_field,
                                                                  row_label("Authors"), g_authors_field, g_category_lbl, g_category_cycle,
                                                                  row_label("Release date"), g_date_field, row_label("Released at"), g_released_field), g_creators_row, g_system_row, g_number_row, g_platform_row, NULL, NULL);
        Object *page_notes = shell_card("Notes and news", shell_form(area_label("Notes"), g_notes_field, area_label("News"), g_news_field,
                                                                     NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), NULL, NULL, NULL, NULL, NULL, NULL);
        Object *boards_lbl = area_label("Boards"), *embeds_lbl = area_label("Media embeds"), *cover_lbl = row_label("Cover");
        Object *cover_row = shell_row(cover_lbl, cover_text);
        Object *page_roster = shell_card("Members, boards and media", shell_form(area_label("Members"), g_members_field, boards_lbl, g_boards_field,
                                                                                 embeds_lbl, g_embeds_field,
                                                                                 NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), shell_row(embed_pick_btn, NULL), NULL, NULL, NULL, NULL, NULL);
        Object *page_nfo = shell_card("NFO and file_id.diz", shell_form(row_label("Diz line"), g_diz_line_field, area_label("Text"), g_nfo_view,
                                                                        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), shell_row(nfo_preview_btn, NULL), g_diz_count, g_template_status, NULL, NULL, NULL);
        Object *page_files = shell_card("Files to include", shell_row(pick_btn, add_file_btn), files_list,
                                        shell_row(remove_file_btn, preview_btn), warn_row, preview_list, NULL, NULL);
        Object *revoke_row = shell_row(row_label("Revoke code"), g_revoke_field);
        Object *page_publish = shell_card("Build and publish", shell_row(build_btn, NULL),
                                          cover_row,
                                          shell_row(cover_btn, publish_btn), revoke_row, shell_row(revoke_btn, door_btn), shell_row(aminet_btn, g_also_aminet_chk), NULL);
        Object *page_aminet = shell_card("Aminet upload", shell_form(row_label("Directory"), type_field, row_label("Architecture"), arch_field,
                                                                     row_label("Uploader"), uname_field, row_label("Email"), email_field,
                                                                     row_label("Short"), g_short_field,
                                                                     area_label("Readme"), g_readme_view,
                                                                     NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), shell_row(aminet_preview_btn, NULL), NULL, NULL, NULL, NULL, NULL);
        g_sec_system[0] = g_system_row;
        g_sec_history[0] = g_category_lbl, g_sec_history[1] = g_category_cycle;
        g_sec_boards[0] = boards_lbl, g_sec_boards[1] = g_boards_field;
        g_sec_media[0] = embeds_lbl, g_sec_media[1] = g_embeds_field, g_sec_media[2] = embed_pick_btn;
        g_sec_publish[0] = cover_row, g_sec_publish[1] = cover_btn, g_sec_publish[2] = publish_btn, g_sec_publish[3] = revoke_btn, g_sec_publish[4] = g_also_aminet_chk, g_sec_publish[5] = revoke_row;
        g_sec_door[0] = door_btn;
        static const char *const tab_captions[] = { "Release", "Notes", "Roster", "Preview", "Files", "Publish", "Aminet", NULL };
        Object *content;
        tabs = MUI_NewObject((char *)"UP-Tabs.mcc", UPA_Tabs_Captions, (ULONG)tab_captions, TAG_DONE);
        pages = MUI_NewObject((char *)MUIC_Group, MUIA_Group_PageMode, (ULONG)TRUE, MUIA_FillArea, (ULONG)FALSE,
                              MUIA_Group_Child, (ULONG)top_page(page_release), MUIA_Group_Child, (ULONG)top_page(page_notes), MUIA_Group_Child, (ULONG)top_page(page_roster), MUIA_Group_Child, (ULONG)top_page(page_nfo), MUIA_Group_Child, (ULONG)top_page(page_files),
                              MUIA_Group_Child, (ULONG)top_page(page_publish), MUIA_Group_Child, (ULONG)top_page(page_aminet), TAG_DONE);
        content = (tabs && pages) ? MUI_NewObject((char *)MUIC_Group, MUIA_FillArea, (ULONG)FALSE, MUIA_UserData, (ULONG)UPV_Panel_KeepSpacing, MUIA_Group_Spacing, 0UL, MUIA_Group_Child, (ULONG)shell_row(tabs, quit_btn),
                                                  MUIA_Group_Child, (ULONG)pages, MUIA_Group_Child, (ULONG)shell_gap(0, SHELL_PAD * 2),
                                                  MUIA_Group_Child, (ULONG)MUI_NewObject((char *)MUIC_Group, MUIA_Group_Horiz, (ULONG)TRUE, MUIA_FillArea, (ULONG)FALSE, MUIA_UserData, (ULONG)UPV_Panel_KeepSpacing, MUIA_Group_Spacing, 0UL,
                                                                          MUIA_Group_Child, (ULONG)shell_gap(SHELL_PAD * 2, 0), MUIA_Group_Child, (ULONG)status,
                                                                          MUIA_Group_Child, (ULONG)g_gauge, MUIA_Group_Child, (ULONG)shell_gap(SHELL_PAD, 0), MUIA_Group_Child, (ULONG)g_net_cancel_btn, MUIA_Group_Child, (ULONG)back_btn, MUIA_Group_Child, (ULONG)next_btn, MUIA_Group_Child, (ULONG)shell_gap(SHELL_PAD * 2, 0), TAG_DONE),
                                                  TAG_DONE)
                                  : NULL;
        /* Window padding: MUI adds 4 px (3 px top and bottom) itself, these bands bring every edge to 8. KeepSpacing stops UP-Panel
         * from giving the bands its card gap. */
        if (content != NULL) {
            content = MUI_NewObject((char *)MUIC_Group, MUIA_Group_Horiz, (ULONG)TRUE, MUIA_FillArea, (ULONG)FALSE, MUIA_UserData, (ULONG)UPV_Panel_KeepSpacing, MUIA_Group_Spacing, 0UL,
                                    MUIA_Group_Child, (ULONG)shell_gap(SHELL_PAD, 0), MUIA_Group_Child, (ULONG)content,
                                    MUIA_Group_Child, (ULONG)shell_gap(SHELL_PAD, 0), TAG_DONE);
        }
        if (content != NULL) {
            content = MUI_NewObject((char *)MUIC_Group, MUIA_FillArea, (ULONG)FALSE, MUIA_UserData, (ULONG)UPV_Panel_KeepSpacing, MUIA_Group_Spacing, 0UL,
                                    MUIA_Group_Child, (ULONG)shell_gap(0, SHELL_PAD + 1), MUIA_Group_Child, (ULONG)content,
                                    MUIA_Group_Child, (ULONG)shell_gap(0, SHELL_PAD + 1), TAG_DONE);
        }
        win = content == NULL ? NULL
            : MUI_NewObject((char *)MUIC_Window, MUIA_Window_ID, 0x524C5350,
                        MUIA_Window_Borderless, (ULONG)TRUE, MUIA_Window_DragBar, (ULONG)FALSE, MUIA_Window_CloseGadget, (ULONG)FALSE,
                        MUIA_Window_SizeGadget, (ULONG)FALSE, MUIA_Window_DepthGadget, (ULONG)FALSE,
                        MUIA_Window_Width, (ULONG)MUIV_Window_Width_Screen(70),
                        MUIA_Window_LeftEdge, (ULONG)MUIV_Window_LeftEdge_Centered,
                        MUIA_Window_RootObject,
                        (ULONG)MUI_NewObject((char *)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Surface,
                                             UPA_Panel_Popup, (ULONG)TRUE, MUIA_Group_Child, (ULONG)content, TAG_DONE),
                        TAG_DONE);
    }
    if (win == NULL) {
        shell_requester("The window could not be created.\nFree some memory and check the UP-MCC install, then start again.");
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    app = MUI_NewObject((char *)MUIC_Application, MUIA_Application_Title, (ULONG) "Release Packer",
                        MUIA_Application_Base, (ULONG) "RLSPACK", MUIA_Application_Window, (ULONG)win, TAG_DONE);
    if (app == NULL) {
        shell_requester("The application could not be created.\nFree some memory and check the UP-MCC install, then start again.");
        MUI_DisposeObject(win);
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)app, 2UL, MUIM_Application_ReturnID,
             (ULONG)MUIV_Application_ReturnID_Quit);
    DoMethod(quit_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID,
             (ULONG)MUIV_Application_ReturnID_Quit);
    DoMethod(build_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_BUILD);
    DoMethod(back_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_BACK);
    DoMethod(next_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_NEXT);
    DoMethod(g_net_cancel_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_CANCEL);
    g_net_app = app, g_net_status = status, g_net_pages = pages;
    DoMethod(add_file_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_ADDFILE);
    DoMethod(remove_file_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_REMOVEFILE);
    DoMethod(pick_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_PICK);
    DoMethod(preview_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_PREVIEW);
    DoMethod(flatten_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_FLATTEN);
    DoMethod(embed_pick_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_EMBED_PICK);
    DoMethod(cover_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_COVER);
    DoMethod(publish_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_PUBLISH);
    DoMethod(revoke_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_REVOKE);
    DoMethod(aminet_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_AMINET);
    DoMethod(nfo_preview_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_NFO_PREVIEW);
    DoMethod(aminet_preview_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_AMINET_PREVIEW);
    /* live preview: no verified MUI timer, so refresh on tab switch and on field exit instead of debouncing */
    DoMethod(tabs, MUIM_Notify, UPA_Tabs_Active, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_TAB);
    DoMethod(win, MUIM_Notify, MUIA_Window_ActiveObject, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_FOCUS);
    DoMethod(platform_field, MUIM_Notify, UPA_Cycle_Active, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_PLATFORM);
    DoMethod(door_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_DOOR);
    DoMethod(g_category_cycle, MUIM_Notify, UPA_Cycle_Active, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_PLATFORM);
    /* restored before the notifies so the programmatic set does not queue a second seed load */
    open_group = shell_last_restore(g_groups, g_ngroups, group_cycle, format_cycle);
    DoMethod(group_cycle, MUIM_Notify, UPA_Cycle_Active, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_GROUP);
    DoMethod(format_cycle, MUIM_Notify, UPA_Cycle_Active, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_FORMAT);
    DoMethod(edit_groups_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_EDIT_GROUPS);
    {
        ULONG code_str = 0;
        GetAttr(UPA_Field_String, code_field, &code_str);
        if (code_str != 0) DoMethod((Object *)code_str, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_CODE);
    }
    {
        ULONG name_str = 0;
        GetAttr(UPA_Field_String, name_field, &name_str);
        if (name_str != 0) {
            DoMethod((Object *)name_str, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_NAME);
        }
    }
    {
        Object *dz[2];
        size_t di;
        dz[0] = g_diz_line_field, dz[1] = g_released_field;
        for (di = 0; di < 2; di++) {
            ULONG dz_str = 0;
            GetAttr(UPA_Field_String, dz[di], &dz_str);
            if (dz_str != 0) DoMethod((Object *)dz_str, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_DIZCOUNT);
        }
        shell_diz_count(name_field);
    }
    {
        ULONG auth_str = 0;
        GetAttr(UPA_Field_String, g_authors_field, &auth_str);
        if (auth_str != 0) DoMethod((Object *)auth_str, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_AUTHORS);
    }
    DoMethod(tabs, MUIM_Notify, UPA_Tabs_Active, MUIV_EveryTime, (ULONG)pages, 3UL, MUIM_Set, MUIA_Group_ActivePage,
             MUIV_TriggerValue);
    SetAttrs(warn_row, MUIA_ShowMe, FALSE, TAG_DONE);
    {
        rp_aminet_prefs pr;
        if (rp_aminet_prefs_load(RP_AMINET_PREFS_PATH, &pr) == 0) {
            shell_set_type(type_field, pr.directory);
            SetAttrs(arch_field, UPA_Cycle_Active, (ULONG)(rp_aminet_architecture_index(pr.architecture) > 0 ? rp_aminet_architecture_index(pr.architecture) : 0), TAG_DONE);
            shell_set_field(uname_field, pr.uploader);
            shell_set_field(email_field, pr.email);
        }
    }
    {
        char today[16];
        shell_today(today, sizeof today);
        shell_set_field(g_date_field, today);
    }
    if (shell_type_dir(type_field)[0] == '\0') shell_guess_type(type_field, platform_field);
    shell_platform_row(platform_field);
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Open, win, &is_open);
    if (!is_open) {
        shell_requester("The window did not open.\nThe screen or window could not be opened. Close other programs to free memory, then start again.");
        MUI_DisposeObject(app);
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    SetAttrs(preview_list, UPA_List_Count, 0UL, TAG_DONE);
    SetAttrs(files_list, UPA_List_Count, 0UL, TAG_DONE);
    shell_seed_status(status, &g_groups[open_group], code_field, format_cycle);
    shell_creators_refresh(g_groups[open_group].renderer == R_XINNOVATIONS);
    shell_system_prefill(platform_field);
    for (;;) {
        ULONG id = DoMethod(app, MUIM_Application_NewInput, (ULONG)&sigs);
        int last_save_failed = 0;
        if (id == (ULONG)MUIV_Application_ReturnID_Quit || (SetSignal(0, 0) & SIGBREAKF_CTRL_C) != 0) break;
        if (id == ID_BUILD) {
            shell_build(app, status, g_groups, group_cycle, name_field, code_field, platform_field, desc_field, format_cycle, 0);
        }
        if (id == ID_EDIT_GROUPS && rp_editor_run(app, win, "PROGDIR:groups") == 1) {
            /* The cycle entries are fixed at creation, so swap in a new cycle built from the reloaded groups. */
            if (shell_load_groups(broken, sizeof(broken)) < 0 || g_ngroups == 0) {
                shell_requester(broken[0] ? broken : "No usable groups left.\nRestart Release Packer after fixing the group drawers.");
                break;
            }
            DoMethod(group_slot, MUIM_Group_InitChange);
            DoMethod(group_slot, OM_REMMEMBER, (ULONG)group_cycle);
            DoMethod(group_slot, OM_REMMEMBER, (ULONG)edit_groups_btn);
            MUI_DisposeObject(group_cycle);
            group_cycle = MUI_NewObject((char *)"UP-Cycle.mcc", UPA_Cycle_Entries, (ULONG)g_group_labels, TAG_DONE);
            if (group_cycle == NULL) {
                shell_requester("The Group cycle could not be rebuilt.\nRestart Release Packer.");
                break;
            }
            DoMethod(group_slot, OM_ADDMEMBER, (ULONG)group_cycle);
            DoMethod(group_slot, OM_ADDMEMBER, (ULONG)edit_groups_btn);
            DoMethod(group_slot, MUIM_Group_ExitChange);
            DoMethod(group_cycle, MUIM_Notify, UPA_Cycle_Active, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_GROUP);
            shell_seed_status(status, &g_groups[0], code_field, format_cycle);
        }
        if (id == ID_AUTHORS) {
            ULONG ga = 0;
            GetAttr(UPA_Cycle_Active, group_cycle, &ga);
            shell_creators_refresh(ga < (ULONG)g_ngroups && g_groups[ga].renderer == R_XINNOVATIONS);
        }
        if (id == ID_FOCUS) {
            ULONG ao = 0, inner = 0;
            GetAttr(MUIA_Window_ActiveObject, win, &ao);
            GetAttr(UPA_Field_String, g_authors_field, &inner);
            if (ao != inner) shell_creators_complete();
            shell_system_prefill(platform_field);
        }
        if (id == ID_PLATFORM) {
            shell_system_prefill(platform_field);
            shell_platform_row(platform_field);
            shell_guess_type(type_field, platform_field);
        }
        if (id == ID_GROUP) shell_last_format_apply(g_groups, g_ngroups, group_cycle, format_cycle);
        if (id == ID_GROUP || id == ID_FORMAT) last_save_failed = shell_last_save(g_groups, g_ngroups, group_cycle, format_cycle) != 0;
        if (id == ID_GROUP) {
            ULONG ga = 0;
            GetAttr(UPA_Cycle_Active, group_cycle, &ga);
            if (ga < (ULONG)g_ngroups) shell_seed_status(status, &g_groups[ga], code_field, format_cycle);
            if (ga < (ULONG)g_ngroups) shell_creators_refresh(g_groups[ga].renderer == R_XINNOVATIONS);
        }
        if (id == ID_CODE || id == ID_FORMAT) {
            ULONG ga = 0;
            GetAttr(UPA_Cycle_Active, group_cycle, &ga);
            /* the number preview first; a code check that fires then replaces it with its verdict */
            if (g_seed_ok && ga < (ULONG)g_ngroups) shell_number_status(status, &g_groups[ga], code_field, format_cycle);
        }
        if (id == ID_CODE) shell_code_check(status, g_groups, group_cycle, code_field);
        /* last, so the seed and number lines above do not hide it: a read-only PROGDIR: loses the remembered group and format */
        if ((id == ID_GROUP || id == ID_FORMAT) && last_save_failed)
            SetAttrs(status, UPA_Text_Contents, (ULONG) "Could not write PROGDIR:last.prefs (is PROGDIR: read-only?): the group and format are not remembered for the next start.", TAG_DONE);
        if (id == ID_NAME) shell_suggest_code(name_field, code_field, group_cycle);
        if (id == ID_NAME || id == ID_DIZCOUNT) shell_diz_count(name_field);
        if (id == ID_EMBED_PICK) shell_embed_pick();
        if (id == ID_COVER && shell_pick_cover()) SetAttrs(cover_text, UPA_Text_Contents, (ULONG)g_cover, TAG_DONE);
        if (id == ID_PUBLISH || id == ID_REVOKE || id == ID_DOOR) {
            int ok;
            shell_net_busy(1);
            ok = shell_online(app, status, (int)id, g_groups, group_cycle, name_field, code_field, platform_field, desc_field,
                                  format_cycle);
            ULONG also = 0;
            GetAttr(MUIA_Selected, g_also_aminet_chk, &also);
            /* "Also upload to Aminet": only after the publish was accepted; the Aminet result replaces the status line */
            if (id == ID_PUBLISH && ok == 0 && also) {
                shell_aminet(app, status, g_groups, group_cycle, name_field, platform_field, code_field, desc_field, format_cycle, type_field, arch_field,
                             uname_field, email_field, 0);
            }
            shell_net_busy(0);
            if (g_net_quit) break;
        }
        if (id == ID_NFO_PREVIEW) {
            shell_preview_nfo(status, g_groups, group_cycle, name_field, code_field, platform_field, desc_field, format_cycle);
        }
        if (id == ID_BACK || id == ID_NEXT) {
            ULONG cur = 0;
            int to;
            GetAttr(UPA_Tabs_Active, tabs, &cur);
            to = rp_page_step((int)cur, id == ID_NEXT ? 1 : -1, NULL, SHELL_TAB_COUNT);
            if (to >= 0) SetAttrs(tabs, UPA_Tabs_Active, (ULONG)to, TAG_DONE);
        }
        if (id == ID_TAB || id == ID_FOCUS) {
            ULONG tab = (ULONG)-1;
            GetAttr(UPA_Tabs_Active, tabs, &tab);
            SetAttrs(back_btn, MUIA_Disabled, (ULONG)(rp_page_step((int)tab, -1, NULL, SHELL_TAB_COUNT) < 0), TAG_DONE);
            SetAttrs(next_btn, MUIA_Disabled, (ULONG)(rp_page_step((int)tab, 1, NULL, SHELL_TAB_COUNT) < 0), TAG_DONE);
            if (tab == SHELL_TAB_NFO) shell_preview_nfo(status, g_groups, group_cycle, name_field, code_field, platform_field, desc_field, format_cycle);
            else if (tab == SHELL_TAB_AMINET) {
                shell_aminet(app, status, g_groups, group_cycle, name_field, platform_field, code_field, desc_field, format_cycle, type_field, arch_field,
                             uname_field, email_field, 1);
            }
            /* a failed check here is only a stale preview: never jump to the field it names */
            g_err_tab = (ULONG)-1;
            g_err_obj = NULL;
        }
        if (id == ID_AMINET || id == ID_AMINET_PREVIEW) {
            if (id == ID_AMINET) shell_net_busy(1);
            shell_aminet(app, status, g_groups, group_cycle, name_field, platform_field, code_field, desc_field, format_cycle, type_field, arch_field,
                         uname_field, email_field, id == ID_AMINET_PREVIEW);
            if (id == ID_AMINET) shell_net_busy(0);
            if (g_net_quit) break;
        }
        if ((id == ID_PICK && shell_pick_path(1)) || (id == ID_ADDFILE && shell_pick_path(0))) id = ID_REFRESH;
        if (id == ID_REMOVEFILE) {
            ULONG row = (ULONG)-1;
            GetAttr(UPA_List_Active, files_list, &row);
            if ((LONG)row >= 0 && row < g_paths.n) {
                rp_paths_remove(&g_paths, row);
                id = ID_REFRESH;
            } else SetAttrs(status, UPA_Text_Contents, (ULONG) "Remove selected: click an entry in the Files list first.", TAG_DONE);
        }
        if (id == ID_FLATTEN && g_prev.nnested > 0) {
            int at = rp_paths_find(&g_paths, g_prev.nested[0].path);
            if (at >= 0) rp_paths_replace(&g_paths, (size_t)at, g_prev.nested[0].flatten_to);
            id = ID_REFRESH;
        }
        if (id == ID_PREVIEW || id == ID_REFRESH) SetAttrs(files_list, UPA_List_Count, (ULONG)g_paths.n, UPA_List_Active, (ULONG)-1, TAG_DONE);
        if (id == ID_PREVIEW || id == ID_REFRESH) {
            shell_refresh_preview(preview_list, warn_row, warn_text, g_groups, group_cycle, code_field, status);
        }
        /* the Preview button also writes the real archive to PROGDIR:dry-run, once the lists show the files */
        if (id == ID_PREVIEW) {
            shell_build(app, status, g_groups, group_cycle, name_field, code_field, platform_field, desc_field, format_cycle, 1);
            /* the dry run only reports: a missing Name or note must not pull the user off the Files tab they are previewing */
            g_err_tab = (ULONG)-1;
            g_err_obj = NULL;
        }
        if (g_err_tab != (ULONG)-1) {
            /* the failed check names a field: show its tab and put the cursor there (inner string gadget for UP-Field) */
            ULONG inner = 0;
            SetAttrs(tabs, UPA_Tabs_Active, g_err_tab, TAG_DONE);
            if (g_err_obj != NULL) {
                GetAttr(UPA_Field_String, g_err_obj, &inner);
                SetAttrs(win, MUIA_Window_ActiveObject, inner != 0 ? inner : (ULONG)g_err_obj, TAG_DONE);
            }
            g_err_tab = (ULONG)-1;
            g_err_obj = NULL;
        }
        if (sigs != 0) Wait(sigs | SIGBREAKF_CTRL_C);
    }
    MUI_DisposeObject(app);
    json_free(g_seed_tree);
    rp_preview_free(&g_prev);
    CloseLibrary(MUIMasterBase);
    return 0;
}

static struct StackSwapStruct g_sss;
static int g_rc;

static void run_body(void) {
    g_rc = shell_main();
}

static void __attribute__((noinline)) run_swapped(void) {
    __asm__ volatile("move.l %0,%%a0\n\t"
                     "move.l %1,%%a6\n\t"
                     "jsr -732(%%a6)\n\t"
                     "move.l %2,%%a1\n\t"
                     "jsr (%%a1)\n\t"
                     "move.l %0,%%a0\n\t"
                     "move.l %1,%%a6\n\t"
                     "jsr -732(%%a6)\n\t"
                     :
                     : "i"(&g_sss), "m"(SysBase), "i"(run_body)
                     : "d0", "d1", "a0", "a1", "a6", "cc", "memory");
}

static int ui_run_stack(void);

int rp_ui_run(int argc, char **argv) {
    int rc;
    (void)argc;
    (void)argv;
    /* SetAttrs/GetAttr call through this base; left NULL it froze the machine at the first SetAttrs. */
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR) "intuition.library", 36);
    if (IntuitionBase == NULL) return 20;
    rc = ui_run_stack();
    CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}

static int ui_run_stack(void) {
    struct Task *me = FindTask(NULL);
    unsigned long have = (unsigned long)me->tc_SPUpper - (unsigned long)me->tc_SPLower;
    UBYTE *mem;
    if (have >= SHELL_STACK_BYTES) return shell_main();
    mem = (UBYTE *)AllocMem(SHELL_STACK_BYTES, MEMF_PUBLIC | MEMF_CLEAR);
    if (mem == NULL) return shell_main();
    g_sss.stk_Lower = mem;
    g_sss.stk_Upper = (ULONG)(mem + SHELL_STACK_BYTES);
    g_sss.stk_Pointer = (APTR)(mem + SHELL_STACK_BYTES);
    run_swapped();
    FreeMem(mem, SHELL_STACK_BYTES);
    return g_rc;
}
