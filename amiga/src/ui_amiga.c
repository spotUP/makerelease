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
#include "aminet.h"
#include "aminet_preview.h"
#include "aminet_types.h"
#include "archive_preview.h"
#include "credits.h"
#include "date.h"
#include "groups.h"
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

static void shell_requester(const char *text) {
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

static Object *shell_label(const char *text, ULONG tone) {
    return MUI_NewObject((char *)"UP-Text.mcc", UPA_Text_Contents, (ULONG)text, UPA_Text_Tone, tone, TAG_DONE);
}

/* A row label: fixed to its text width so the field beside it gets the rest of the row (an unbounded label took half and cut the placeholder). */
static Object *row_label(const char *text) {
    return MUI_NewObject((char *)"UP-Text.mcc", UPA_Text_Contents, (ULONG)text, UPA_Text_Tone, (ULONG)UPV_Text_Tone_Normal, UPA_Text_Fixed,
                         (ULONG)TRUE, TAG_DONE);
}

/* A row of one or two widgets; a NULL second half leaves a full-width row. */
static Object *shell_row(Object *a, Object *b) {
    if (b == NULL) {
        return MUI_NewObject((char *)MUIC_Group, MUIA_Group_Horiz, (ULONG)TRUE, MUIA_FillArea, (ULONG)FALSE,
                             MUIA_Group_Child, (ULONG)a, TAG_DONE);
    }
    return MUI_NewObject((char *)MUIC_Group, MUIA_Group_Horiz, (ULONG)TRUE, MUIA_FillArea, (ULONG)FALSE,
                         MUIA_Group_Child, (ULONG)a, MUIA_Group_Child, (ULONG)b, TAG_DONE);
}

/* A two-column form: labels in one column (as wide as the widest), fields in the other, so every input shares a left edge. */
static Object *shell_form(Object *l1, Object *f1, Object *l2, Object *f2, Object *l3, Object *f3, Object *l4, Object *f4,
                          Object *l5, Object *f5, Object *l6, Object *f6, Object *l7, Object *f7, Object *l8, Object *f8, Object *l9, Object *f9) {
    Object *kids[18];
    struct TagItem t[22];
    int i, n = 0;
    kids[0] = l1; kids[1] = f1; kids[2] = l2; kids[3] = f2; kids[4] = l3; kids[5] = f3; kids[6] = l4;
    kids[7] = f4; kids[8] = l5; kids[9] = f5; kids[10] = l6; kids[11] = f6; kids[12] = l7; kids[13] = f7; kids[14] = l8; kids[15] = f8; kids[16] = l9; kids[17] = f9;
    t[n].ti_Tag = MUIA_Group_Columns;
    t[n++].ti_Data = 2;
    t[n].ti_Tag = MUIA_FillArea;
    t[n++].ti_Data = (ULONG)FALSE;
    for (i = 0; i < 18; i++) {
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
static Object *shell_gap(ULONG w, ULONG h) {
    return MUI_NewObject((char *)MUIC_Rectangle, MUIA_FillArea, (ULONG)FALSE, w ? MUIA_FixWidth : TAG_IGNORE, w,
                         h ? MUIA_FixHeight : TAG_IGNORE, h, TAG_DONE);
}

static Object *top_page(Object *card) {
    return MUI_NewObject((char *)MUIC_Group, MUIA_FillArea, (ULONG)FALSE, MUIA_Group_VertSpacing, 0UL, MUIA_Group_Child, (ULONG)card,
                         MUIA_Group_Child, (ULONG)MUI_NewObject((char *)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Surface, MUIA_UserData, (ULONG)UPV_Panel_KeepSpacing, TAG_DONE), TAG_DONE); /* the filler erases what a taller page left below this card */
}

static Object *shell_card(const char *title, Object *a, Object *b, Object *c, Object *d, Object *e, Object *f, Object *g) {
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

#define RP_MAX_FILES_PATH 511

static char g_files[RP_MAX_FILES_PATH + 1];
static char g_cover[RP_MAX_FILES_PATH + 1];

/* The ASL file requester for the cover image. Returns 1 when a file was picked. */
static int shell_pick_cover(void) {
    struct FileRequester *req = AllocAslRequestTags(ASL_FileRequest, ASLFR_TitleText, (ULONG) "Pick the cover image",
                                                    ASLFR_RejectIcons, TRUE, TAG_DONE);
    if (req == NULL) {
        shell_requester("The file requester could not open.\nFree some memory and try again.");
        return 0;
    }
    if (!AslRequest(req, NULL) || req->rf_File == NULL || req->rf_File[0] == '\0') {
        FreeAslRequest(req);
        return 0;
    }
    strncpy(g_cover, req->rf_Dir, sizeof(g_cover) - 1);
    g_cover[sizeof(g_cover) - 1] = '\0';
    AddPart((STRPTR)g_cover, req->rf_File, sizeof(g_cover));
    FreeAslRequest(req);
    return 1;
}

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

/* The ASL drawer requester. Returns 1 when a drawer was picked. */
static int shell_pick_drawer(void) {
    struct FileRequester *req;
    size_t n;
    req = AllocAslRequestTags(ASL_FileRequest, ASLFR_TitleText, (ULONG) "Pick the files drawer",
                              ASLFR_DrawersOnly, TRUE, ASLFR_RejectIcons, TRUE, TAG_DONE);
    if (req == NULL) {
        shell_requester("The file requester could not open.\nFree some memory and try again.");
        return 0;
    }
    if (!AslRequest(req, NULL)) {
        FreeAslRequest(req);
        return 0;
    }
    strncpy(g_files, req->rf_Dir, sizeof(g_files) - 1);
    g_files[sizeof(g_files) - 1] = '\0';
    /* rf_Dir ends in / or : — drop a redundant slash, keep the colon. */
    n = strlen(g_files);
    if (n >= 2 && g_files[n - 1] == '/' && g_files[n - 2] != ':') g_files[n - 1] = '\0';
    FreeAslRequest(req);
    return 1;
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

static struct Hook g_preview_hook;

/* The Release tab notes field; read by Build, Publish and Aminet. */
static Object *g_notes_field;
/* The Release tab news field (multi-line, like the desktop news text); read by Build and Publish. */
static Object *g_news_field;
/* The Release tab authors field: "Role: Name, Name; Role: Name", parsed by rp_parse_credits. */
static Object *g_authors_field;

static const char *shell_field_text(Object *field) {
    ULONG ptr = 0;
    STRPTR txt = NULL;
    GetAttr(UPA_Field_String, field, &ptr);
    if (ptr != 0) GetAttr(MUIA_String_Contents, (Object *)ptr, (ULONG *)&txt);
    return txt ? (const char *)txt : "";
}

/* The multi-line fields as NFO-ready text (rp_text_normalize) in static copies: the class returns its own live buffer. */
#define SHELL_NOTES_MAX 2000
#define SHELL_NEWS_MAX 1000
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

static const char *shell_area_raw(Object *area) {
    ULONG ptr = 0;
    GetAttr(UPA_TextArea_Text, area, &ptr);
    return ptr ? (const char *)ptr : "";
}

/* Parse the Roster tab for group g into the g_rows_* arrays. -1 with "<Field>: Line N: msg" in msg on a bad row. Boards
 * are read for X Innovations, embeds for door-server groups (as on the desktop); the others stay empty. */
static int shell_roster_read(const rp_group *g, char *msg, size_t cap) {
    rp_row_err err;
    int n;
    g_nrows_members = g_nrows_boards = g_nrows_embeds = 0;
    n = rp_parse_member_rows(shell_area_raw(g_members_field), g_rows_members, RP_MEMBERS_MAX, &err);
    if (n < 0) { snprintf(msg, cap, "Members: Line %lu: %s", (unsigned long)err.line, err.msg); return -1; }
    g_nrows_members = (size_t)n;
    if (g->renderer == R_XINNOVATIONS) {
        n = rp_parse_board_rows(shell_area_raw(g_boards_field), g_rows_boards, RP_ONLINE_BOARDS_MAX, &err);
        if (n < 0) { snprintf(msg, cap, "Boards: Line %lu: %s", (unsigned long)err.line, err.msg); return -1; }
        g_nrows_boards = (size_t)n;
    }
    if (RP_ONLINE_HAS_SERVER(g)) {
        n = rp_parse_embed_rows(shell_area_raw(g_embeds_field), g_rows_embeds, RP_ONLINE_EMBEDS_MAX, &err);
        if (n < 0) { snprintf(msg, cap, "Media embeds: Line %lu: %s", (unsigned long)err.line, err.msg); return -1; }
        g_nrows_embeds = (size_t)n;
    }
    return 0;
}

static rp_credit g_credits[RP_CREDITS_MAX];

/* The parsed authors, in g_credits; returns how many. */
static size_t shell_credits(void) {
    return rp_parse_credits(shell_field_text(g_authors_field), g_credits, RP_CREDITS_MAX);
}

/* Copies a malloc'd string into dst (cut to cap) and frees it; NULL gives "". */
static void shell_take(char *dst, size_t cap, char *s) {
    dst[0] = '\0';
    if (s == NULL) return;
    strncpy(dst, s, cap - 1);
    dst[cap - 1] = '\0';
    free(s);
}

static void shell_set_field(Object *field, const char *text) {
    ULONG ptr = 0;
    GetAttr(UPA_Field_String, field, &ptr);
    if (ptr != 0) SetAttrs((Object *)ptr, MUIA_String_Contents, (ULONG)text, TAG_DONE);
}

#define RP_AMINET_PREFS_PATH "PROGDIR:aminet.prefs"

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
static Object *g_category_cycle;

static void shell_category_labels_init(void) {
    int i;
    g_category_labels[0] = "(choose)";
    for (i = 0; rp_release_categories[i] && i < RP_RELEASE_CATEGORY_COUNT; i++) g_category_labels[i + 1] = rp_release_categories[i];
    g_category_labels[i + 1] = NULL;
}

/* The chosen history category, "" when none. */
static const char *shell_category(void) {
    ULONG a = 0;
    GetAttr(UPA_Cycle_Active, g_category_cycle, &a);
    return a > 0 && a <= (ULONG)RP_RELEASE_CATEGORY_COUNT ? rp_release_categories[a - 1] : "";
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

/* Fill the Roster tab for group g: the seed's merged members and boards when seeded, else the group file's members. */
static void shell_roster_prefill(const rp_group *g, int seeded) {
    char *m = seeded ? rp_format_member_rows(g_seed.members, g_seed.nmembers) : rp_format_member_rows(g->members, g->nmembers);
    char *b = (seeded && g->renderer == R_XINNOVATIONS) ? rp_format_board_rows(g_seed.boards, g_seed.nboards) : NULL;
    SetAttrs(g_members_field, UPA_TextArea_Text, (ULONG)(m ? m : ""), TAG_DONE);
    SetAttrs(g_boards_field, UPA_TextArea_Text, (ULONG)(b ? b : ""), TAG_DONE);
    SetAttrs(g_embeds_field, UPA_TextArea_Text, (ULONG) "", TAG_DONE);
    free(m);
    free(b);
}

/* On open and on a group change: load the seed, fill the Roster tab, and show the seed's warnings (or the failure) in the status line. */
static void shell_seed_status(Object *status, const rp_group *g) {
    char msg[400], ok[48];
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
    snprintf(ok, sizeof(ok), "Next release number: %s", g_seed.number_text[0] ? g_seed.number_text : "unknown");
    SetAttrs(status, UPA_Text_Contents, (ULONG)(g_seed.warnings[0] ? g_seed.warnings : ok), TAG_DONE);
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
    const char *paths[1];
    rp_preview_free(&g_prev);
    SetAttrs(warn_row, MUIA_ShowMe, FALSE, TAG_DONE);
    {
        GetAttr(UPA_Cycle_Active, group_cycle, &gactive);
        rp_upper(shell_field_text(code_field), code, sizeof(code));
        rp_base_name(groups[gactive].file_prefix, code, base, sizeof(base));
        paths[0] = g_files;
        /* no drawer picked: zero paths still lists file_id.diz, <REL>/ and <REL>/<REL>.NFO */
        if (rp_preview_archive(base, paths, g_files[0] != '\0' ? 1 : 0, &g_prev, errmsg, sizeof(errmsg)) != 0) {
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

/* Same choices as the desktop app; index 0 is no platform tag. */
static const char *const g_platform_labels[] = { "None", "A500", "A1200", "Console", NULL };
static const char *const g_platform_tags[] = { "", "A500", "A1200", "Console" };

static void shell_guess_type(Object *type_cycle, Object *platform_field) {
    ULONG pa = 0;
    const char *guess;
    GetAttr(UPA_Cycle_Active, platform_field, &pa);
    guess = rp_aminet_guess_type(shell_category(), pa < 4 ? g_platform_tags[pa] : "");
    if (guess[0]) shell_set_type(type_cycle, guess);
}

static void shell_build(Object *status, const rp_group *groups, Object *group_cycle, Object *name_field,
                        Object *code_field, Object *platform_field, Object *desc_field, Object *format_cycle) {
    /* The inner string gadgets hold the text; UPA_Field_String gives them. */
    ULONG name_ptr = 0, code_ptr = 0, desc_ptr = 0, gactive = 0, active = 0;
    STRPTR name_txt, code_txt, plat_txt, desc_txt;
    const rp_group *g;
    const rp_member *members;
    size_t nmembers;
    rp_x_board xboards[RP_ONLINE_BOARDS_MAX];
    char path[256], *diz = NULL, *nfo = NULL, code[40], base[64], packed[72], date[16];
    char membuf[RP_MEMBERS_MAX][RP_NICK_MAX + RP_ROLE_MAX + 4];
    const char *memberlines[RP_MEMBERS_MAX];
    static char creditbuf[RP_CREDITS_MAX][RP_CREDIT_ROLE_MAX + RP_CREDIT_NAME_MAX + 4];
    const char *creditlines[RP_CREDITS_MAX];
    char creators[256], coder[128], design[128];
    size_t ncredits;
    rp_scalar scalars[11];
    rp_block blocks[4];
    rp_values v;
    struct DateStamp ds;
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
    {
        ULONG pa = 0;
        GetAttr(UPA_Cycle_Active, platform_field, &pa);
        plat_txt = (STRPTR)(pa < 4 ? g_platform_tags[pa] : "");
    }
    if (desc_ptr != 0) GetAttr(MUIA_String_Contents, (Object *)desc_ptr, (ULONG *)&desc_txt);
    if (name_txt == NULL) name_txt = (STRPTR) "";
    if (code_txt == NULL) code_txt = (STRPTR) "";
    if (desc_txt == NULL) desc_txt = (STRPTR) "";
    GetAttr(UPA_Cycle_Active, group_cycle, &gactive);
    GetAttr(UPA_Cycle_Active, format_cycle, &active);
    g = &groups[gactive];
    DateStamp(&ds);
    rp_iso_date((long)ds.ds_Days + 2922, date, sizeof(date));
    rp_upper(code_txt, code, sizeof(code));
    if ((int)strlen(code) != g->code_length) {
        snprintf(errmsg, sizeof(errmsg), "Code must be exactly %d letters or digits.", g->code_length);
        SetAttrs(status, UPA_Text_Contents, (ULONG)errmsg, TAG_DONE);
        return;
    }
    if (shell_roster_read(g, errmsg, sizeof(errmsg)) != 0) {
        SetAttrs(status, UPA_Text_Contents, (ULONG)errmsg, TAG_DONE);
        return;
    }
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
            SetAttrs(status, UPA_Text_Contents, (ULONG) "Choose a category on the Release tab first.", TAG_DONE);
            return;
        }
        if (shell_seed_load(g, errmsg, sizeof(errmsg)) != 0) {
            SetAttrs(status, UPA_Text_Contents, (ULONG)errmsg, TAG_DONE);
            return;
        }
        if (g_seed.number_text[0]) number = g_seed.number_text;
    }
    rp_base_name(g->file_prefix, code, base, sizeof(base));
    snprintf(packed, sizeof(packed), "%s.%s", base, active ? "ZIP" : "LHA");
    snprintf(path, sizeof(path), "PROGDIR:groups/%s/file_id.diz", g->id);
    if (rp_read_template(path, 0, &diz, errmsg, sizeof(errmsg)) != 0) {
        SetAttrs(status, UPA_Text_Contents, (ULONG)errmsg, TAG_DONE);
        return;
    }
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
    v.nscalars = 11;
    v.blocks = blocks;
    v.nblocks = 4;
    /* The NFO: the group's own template, Up Rough's plain layout, or an
     * honest stop for /X (its framed drawing needs the catalog: phase 2). */
    if (g->renderer == R_PLACEHOLDER) {
        char nfo_path[256], *tpl = NULL;
        snprintf(nfo_path, sizeof(nfo_path), "PROGDIR:groups/%s/release.nfo", g->id);
        if (rp_read_template(nfo_path, 1, &tpl, errmsg, sizeof(errmsg)) != 0) {
            SetAttrs(status, UPA_Text_Contents, (ULONG)errmsg, TAG_DONE);
            free(diz);
            return;
        }
        if (tpl != NULL) {
            nfo = rp_render_template(tpl, &v, &errline, errmsg, sizeof(errmsg));
            free(tpl);
            if (nfo == NULL) {
                SetAttrs(status, UPA_Text_Contents, (ULONG)errmsg, TAG_DONE);
                free(diz);
                return;
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
            SetAttrs(status, UPA_Text_Contents, (ULONG)(errmsg[0] ? errmsg : "out of memory"), TAG_DONE);
            free(diz);
            return;
        }
    } else {
        /* /X: the framed drawing. The release list comes from the catalog
         * (phase 2), so it is empty here; the boards are the Roster tab's. */
        rp_nfo_x_form xf;
        memset(&xf, 0, sizeof xf);
        xf.release_name = name_txt;
        xf.release_date = date;
        xf.coder = coder;
        xf.system = plat_txt;
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
            SetAttrs(status, UPA_Text_Contents, (ULONG) "out of memory", TAG_DONE);
            free(diz);
            return;
        }
    }
    rendered = rp_render_template(diz, &v, &errline, errmsg, sizeof(errmsg));
    free(diz);
    if (rendered == NULL) {
        SetAttrs(status, UPA_Text_Contents, (ULONG)errmsg, TAG_DONE);
        free(nfo);
        return;
    }
    /* Staged like the desktop: outer/ holds file_id.diz next to BASE/,
     * which holds BASE.NFO and the files. Then the archive, then the whole
     * outer drawer moves into PROGDIR:releases/BASE. */
    {
        char outer[288], inner[352], work[416], nfo_name[384], archive[384], *cmd, done[448];
        struct stat st;
        snprintf(outer, sizeof(outer), "T:rp-%s", base);
        snprintf(inner, sizeof(inner), "%s/%s", outer, base);
        if (rp_make_dirs(inner, errmsg, sizeof(errmsg)) != 0) goto build_fail;
        snprintf(work, sizeof(work), "%s/file_id.diz", outer);
        if (rp_write_file(work, rendered, strlen(rendered), errmsg, sizeof(errmsg)) != 0) goto build_fail;
        free(rendered);
        rendered = NULL;
        if (nfo != NULL) {
            snprintf(nfo_name, sizeof(nfo_name), "%s/%s.NFO", inner, base);
            if (rp_write_file(nfo_name, nfo, strlen(nfo), errmsg, sizeof(errmsg)) != 0) goto build_fail;
            free(nfo);
            nfo = NULL;
        }
        /* The files drawer, if one was picked. A picked drawer that is
         * itself called BASE is the release the user already assembled by
         * hand: its contents belong directly in the release drawer. */
        if (g_files[0] != '\0') {
            if (same_name_fold(base_name_of(g_files), base)) {
                DIR *scan = opendir(g_files);
                struct dirent *e;
                if (scan == NULL) {
                    snprintf(errmsg, sizeof(errmsg), "%s: could not be read", g_files);
                    goto build_fail;
                }
                while ((e = readdir(scan)) != NULL) {
                    char s[576], t[576];
                    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
                    snprintf(s, sizeof(s), "%s/%s", g_files, e->d_name);
                    snprintf(t, sizeof(t), "%s/%s", inner, e->d_name);
                    if (rp_copy_tree(s, t, errmsg, sizeof(errmsg)) != 0) {
                        closedir(scan);
                        goto build_fail;
                    }
                }
                closedir(scan);
            } else {
                char t[576];
                snprintf(t, sizeof(t), "%s/%s", inner, base_name_of(g_files));
                if (rp_copy_tree(g_files, t, errmsg, sizeof(errmsg)) != 0) goto build_fail;
            }
        }
        if (rp_make_dirs("PROGDIR:releases", errmsg, sizeof(errmsg)) != 0) goto build_fail;
        snprintf(archive, sizeof(archive), "PROGDIR:releases/%s", packed);
        if (stat(archive, &st) == 0) {
            snprintf(errmsg, sizeof(errmsg), "%s already exists - move it away first", packed);
            goto build_fail;
        }
        {
            char finaldir[384];
            snprintf(finaldir, sizeof(finaldir), "PROGDIR:releases/%s", base);
            if (stat(finaldir, &st) == 0) {
                snprintf(errmsg, sizeof(errmsg), "%s already exists in PROGDIR:releases", base);
                goto build_fail;
            }
        }
        {
            /* PROGDIR: belongs to each process: the archiver would resolve it to its own drawer. */
            char abs_archive[384];
            BPTR rl = Lock((STRPTR) "PROGDIR:releases", ACCESS_READ);
            if (rl == 0 || !NameFromLock(rl, (STRPTR)abs_archive, sizeof(abs_archive) - strlen(packed) - 1)) {
                if (rl != 0) UnLock(rl);
                snprintf(errmsg, sizeof(errmsg), "PROGDIR:releases: could not be resolved");
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
        if (rp_run_command(outer, cmd, errmsg, sizeof(errmsg)) != 0) {
            free(cmd);
            goto build_fail;
        }
        free(cmd);
        /* The whole outer drawer (DIZ next to BASE/) becomes
         * PROGDIR:releases/BASE, exactly like the desktop finalize. */
        {
            char finaldir[384];
            snprintf(finaldir, sizeof(finaldir), "PROGDIR:releases/%s", base);
            if (rp_move_dir(outer, finaldir, errmsg, sizeof(errmsg)) != 0) goto build_fail;
        }
        snprintf(done, sizeof(done), "Built %s", archive);
        SetAttrs(status, UPA_Text_Contents, (ULONG)done, TAG_DONE);
        goto build_end;
    build_fail:
        SetAttrs(status, UPA_Text_Contents, (ULONG)errmsg, TAG_DONE);
    build_end:
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

/* Publish / Revoke / Door repository. Synchronous: the status line says
 * "Working..." and MUI gets one input pump to draw it, then the call blocks the
 * window until the server answers (no cancel; the transport has its own limits). */
static void shell_online(Object *app, Object *status, int action, const rp_group *groups, Object *group_cycle,
                         Object *name_field, Object *code_field, Object *platform_field, Object *desc_field,
                         Object *format_cycle) {
    ULONG gactive = 0, active = 0, sigs = 0, pa = 0;
    const rp_group *g;
    char code[40], base[64], packed[72], archive[160], diz[160], msg[400], pub_creators[256];
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
    if (action == ID_REVOKE && (code[0] == '\0' || !shell_confirm_revoke(code))) {
        SetAttrs(status, UPA_Text_Contents, (ULONG)(code[0] ? "Revoke cancelled." : "Enter the code of the release to revoke."), TAG_DONE);
        return;
    }
    SetAttrs(status, UPA_Text_Contents, (ULONG) "Working...", TAG_DONE);
    DoMethod(app, MUIM_Application_NewInput, (ULONG)&sigs);
    if (action == ID_PUBLISH) {
        rp_online_publish_args a;
        if (shell_roster_read(g, msg, sizeof(msg)) != 0) {
            SetAttrs(status, UPA_Text_Contents, (ULONG)msg, TAG_DONE);
            return;
        }
        memset(&a, 0, sizeof a);
        a.members = g_rows_members; /* n_members 0 = the group's own list */
        a.n_members = g_nrows_members;
        a.boards = g_rows_boards;
        a.n_boards = g_nrows_boards;
        a.embeds = g_rows_embeds;
        a.n_embeds = g_nrows_embeds;
        GetAttr(UPA_Cycle_Active, platform_field, &pa);
        a.group = g;
        a.code = code;
        a.release_name = shell_field_text(name_field);
        a.platform_tag = pa < 4 ? g_platform_tags[pa] : "";
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
        rp_online_publish(&a, msg, sizeof(msg));
    } else if (action == ID_REVOKE) {
        rp_online_revoke(g, code, rp_http_amiga, NULL, msg, sizeof(msg));
    } else {
        rp_online_door(g, archive, seed, rp_http_amiga, NULL, msg, sizeof(msg));
    }
    SetAttrs(status, UPA_Text_Contents, (ULONG)msg, TAG_DONE);
}

/* Aminet upload: the readme comes from the form (Aminet tab) and the group, the archive is the one
 * Build wrote. Synchronous like shell_online. */
static void shell_aminet(Object *app, Object *status, const rp_group *groups, Object *group_cycle,
                         Object *code_field, Object *desc_field, Object *format_cycle, Object *type_field,
                         Object *arch_field, Object *uname_field, Object *email_field) {
    ULONG gactive = 0, active = 0, sigs = 0;
    const rp_group *g;
    char code[40], base[64], packed[72], archive[160], creators[256], msg[400];
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
    in.short_text = shell_field_text(desc_field);
    in.type = shell_type_dir(type_field);
    in.architecture = shell_field_text(arch_field);
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
    Object *group_cycle, *name_field, *code_field, *platform_field, *desc_field, *format_cycle;
    Object *files_text, *pick_btn, *build_btn, *status;
    Object *cover_btn, *publish_btn, *revoke_btn, *door_btn, *cover_text;
    Object *aminet_btn, *type_field, *arch_field, *uname_field, *email_field;
    Object *tabs, *pages, *preview_btn, *quit_btn, *flatten_btn, *preview_list, *warn_text, *warn_row;
    ULONG sigs = 0, is_open = 0;
    static const char *const formats[] = { "LHA", "ZIP", NULL };
    char ids[RP_MAX_SHELL_GROUPS][RP_ID_MAX + 1];
    char errmsg[256], broken[1024];
    int n, i;

    MUIMasterBase = OpenLibrary((CONST_STRPTR) "muimaster.library", 19);
    if (MUIMasterBase == NULL) {
        shell_requester("muimaster.library 19 (MUI 3.8) is not installed.\nInstall MUI 3.8, then start Release Packer again.");
        return 20;
    }
    n = rp_list_groups("PROGDIR:groups", ids, RP_MAX_SHELL_GROUPS, errmsg, sizeof(errmsg));
    if (n < 0) {
        shell_requester(errmsg);
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    if (n == 0) {
        shell_requester("No groups found.\nPut your group drawers in PROGDIR:groups, then start again.");
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    /* One bad drawer never hides the others; the good ones still open. */
    broken[0] = '\0';
    g_ngroups = 0;
    for (i = 0; i < n; i++) {
        if (rp_load_group("PROGDIR:groups", ids[i], &g_groups[g_ngroups], errmsg, sizeof(errmsg)) != 0) {
            strncat(broken, errmsg, sizeof(broken) - strlen(broken) - 1);
            strncat(broken, "\n", sizeof(broken) - strlen(broken) - 1);
            continue;
        }
        g_group_labels[g_ngroups] = g_groups[g_ngroups].label;
        g_ngroups++;
    }
    g_group_labels[g_ngroups] = NULL;
    if (g_ngroups == 0) {
        shell_requester(broken[0] ? broken : "No usable groups.");
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    group_cycle = MUI_NewObject((char *)"UP-Cycle.mcc", UPA_Cycle_Entries, (ULONG)g_group_labels, TAG_DONE);
    name_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Release name", TAG_DONE);
    {   /* longest code any group allows; the exact length per group is still checked at Build */
        int i, maxcode = 1;
        for (i = 0; i < g_ngroups; i++) if (g_groups[i].code_length > maxcode) maxcode = g_groups[i].code_length;
        code_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "e.g. ABC", MUIA_String_MaxLen, (ULONG)maxcode + 1, TAG_DONE);
    }
    platform_field = MUI_NewObject((char *)"UP-Cycle.mcc", UPA_Cycle_Entries, (ULONG)g_platform_labels, TAG_DONE);
    desc_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "One-line description", TAG_DONE);
    format_cycle =
        MUI_NewObject((char *)"UP-Cycle.mcc", UPA_Cycle_Entries, (ULONG)formats, TAG_DONE);
    files_text = shell_label("(no files drawer picked)", UPV_Text_Tone_Muted);
    pick_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Pick...", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    build_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Build", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, UPA_Button_Role,
                              (ULONG)UPV_Button_Role_Accent, TAG_DONE);
    preview_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Preview", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    quit_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Role, (ULONG)UPV_Button_Role_Ghost, UPA_Button_Icon, (ULONG) "x",
                             UPA_Button_IconOnly, (ULONG)TRUE, UPA_Button_Flat, (ULONG)TRUE, UPA_Button_FixedWidth, 24UL,
                             UPA_Button_FixedHeight, 24UL, TAG_DONE);
    cover_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Pick cover...", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    publish_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Publish", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, UPA_Button_Role,
                                (ULONG)UPV_Button_Role_Accent, TAG_DONE);
    revoke_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Revoke", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    door_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Door repo", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    aminet_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Aminet", UPA_Button_Align, (ULONG)UPV_Button_Align_Centre, TAG_DONE);
    shell_type_labels_init();
    shell_category_labels_init();
    g_category_cycle = MUI_NewObject((char *)"UP-Cycle.mcc", UPA_Cycle_Entries, (ULONG)g_category_labels, TAG_DONE);
    type_field = MUI_NewObject((char *)"UP-Cycle.mcc", UPA_Cycle_Entries, (ULONG)g_type_labels, TAG_DONE);
    arch_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "e.g. m68k-amigaos", TAG_DONE);
    uname_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Your name", TAG_DONE);
    email_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Your email address", TAG_DONE);
    g_authors_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Role: Name, Name; Role: Name", MUIA_String_MaxLen, 255UL, TAG_DONE);
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
    status = shell_label(broken[0] ? broken : "Fill in the release, then open the Publish tab and press Build.", UPV_Text_Tone_Muted);
    if (group_cycle == NULL || name_field == NULL || code_field == NULL || platform_field == NULL ||
        desc_field == NULL || format_cycle == NULL || files_text == NULL || pick_btn == NULL ||
        build_btn == NULL || status == NULL || preview_btn == NULL || preview_list == NULL || warn_row == NULL ||
        cover_btn == NULL || publish_btn == NULL || revoke_btn == NULL || door_btn == NULL || cover_text == NULL ||
        aminet_btn == NULL || type_field == NULL || arch_field == NULL || uname_field == NULL || email_field == NULL || g_notes_field == NULL || g_news_field == NULL || g_members_field == NULL || g_boards_field == NULL || g_embeds_field == NULL || g_authors_field == NULL || g_category_cycle == NULL) {
        shell_requester("The window objects could not be created.\nFree some memory and check the UP-MCC install, then start again.");
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    {
        /* Tabs like the desktop wizard, over the steps the Amiga has data for; one page shows at a time, so no scrolling */
        Object *page_release = shell_card("Release", shell_form(row_label("Group"), group_cycle, row_label("Name"), name_field,
                                                                  row_label("Code"), code_field, row_label("Platform"), platform_field,
                                                                  row_label("Format"), format_cycle, row_label("Description"), desc_field,
                                                                  row_label("Authors"), g_authors_field, row_label("Category"), g_category_cycle,
                                                                  NULL, NULL), NULL, NULL, NULL, NULL, NULL, NULL);
        Object *page_notes = shell_card("Notes and news", shell_form(row_label("Notes"), g_notes_field, row_label("News"), g_news_field,
                                                                     NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), NULL, NULL, NULL, NULL, NULL, NULL);
        Object *page_roster = shell_card("Members, boards and media", shell_form(row_label("Members"), g_members_field, row_label("Boards"), g_boards_field,
                                                                                 row_label("Media embeds"), g_embeds_field,
                                                                                 NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), NULL, NULL, NULL, NULL, NULL, NULL);
        Object *page_files = shell_card("Files to include", shell_row(row_label("Drawer"), files_text),
                                        shell_row(pick_btn, preview_btn), warn_row, preview_list, NULL, NULL, NULL);
        Object *page_publish = shell_card("Build and publish", shell_row(build_btn, NULL),
                                          shell_row(row_label("Cover"), cover_text),
                                          shell_row(cover_btn, publish_btn), shell_row(revoke_btn, door_btn), shell_row(aminet_btn, NULL), NULL, NULL);
        Object *page_aminet = shell_card("Aminet upload", shell_form(row_label("Directory"), type_field, row_label("Architecture"), arch_field,
                                                                     row_label("Uploader"), uname_field, row_label("Email"), email_field,
                                                                     NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), NULL, NULL, NULL, NULL, NULL, NULL);
        static const char *const tab_captions[] = { "Release", "Notes", "Roster", "Files", "Publish", "Aminet", NULL };
        Object *content;
        tabs = MUI_NewObject((char *)"UP-Tabs.mcc", UPA_Tabs_Captions, (ULONG)tab_captions, TAG_DONE);
        pages = MUI_NewObject((char *)MUIC_Group, MUIA_Group_PageMode, (ULONG)TRUE, MUIA_FillArea, (ULONG)FALSE,
                              MUIA_Group_Child, (ULONG)top_page(page_release), MUIA_Group_Child, (ULONG)top_page(page_notes), MUIA_Group_Child, (ULONG)top_page(page_roster), MUIA_Group_Child, (ULONG)top_page(page_files),
                              MUIA_Group_Child, (ULONG)top_page(page_publish), MUIA_Group_Child, (ULONG)top_page(page_aminet), TAG_DONE);
        content = (tabs && pages) ? MUI_NewObject((char *)MUIC_Group, MUIA_FillArea, (ULONG)FALSE, MUIA_UserData, (ULONG)UPV_Panel_KeepSpacing, MUIA_Group_Spacing, 0UL, MUIA_Group_Child, (ULONG)shell_row(tabs, quit_btn),
                                                  MUIA_Group_Child, (ULONG)pages, MUIA_Group_Child, (ULONG)shell_gap(0, SHELL_PAD * 2),
                                                  MUIA_Group_Child, (ULONG)MUI_NewObject((char *)MUIC_Group, MUIA_Group_Horiz, (ULONG)TRUE, MUIA_FillArea, (ULONG)FALSE, MUIA_UserData, (ULONG)UPV_Panel_KeepSpacing, MUIA_Group_Spacing, 0UL,
                                                                          MUIA_Group_Child, (ULONG)shell_gap(SHELL_PAD * 2, 0), MUIA_Group_Child, (ULONG)status, TAG_DONE),
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
    DoMethod(pick_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_PICK);
    DoMethod(preview_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_PREVIEW);
    DoMethod(flatten_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_FLATTEN);
    DoMethod(cover_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_COVER);
    DoMethod(publish_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_PUBLISH);
    DoMethod(revoke_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_REVOKE);
    DoMethod(aminet_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_AMINET);
    DoMethod(platform_field, MUIM_Notify, UPA_Cycle_Active, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_PLATFORM);
    DoMethod(door_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_DOOR);
    DoMethod(g_category_cycle, MUIM_Notify, UPA_Cycle_Active, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_PLATFORM);
    DoMethod(group_cycle, MUIM_Notify, UPA_Cycle_Active, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_GROUP);
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
    DoMethod(tabs, MUIM_Notify, UPA_Tabs_Active, MUIV_EveryTime, (ULONG)pages, 3UL, MUIM_Set, MUIA_Group_ActivePage,
             MUIV_TriggerValue);
    SetAttrs(warn_row, MUIA_ShowMe, FALSE, TAG_DONE);
    {
        rp_aminet_prefs pr;
        if (rp_aminet_prefs_load(RP_AMINET_PREFS_PATH, &pr) == 0) {
            shell_set_type(type_field, pr.directory);
            shell_set_field(arch_field, pr.architecture);
            shell_set_field(uname_field, pr.uploader);
            shell_set_field(email_field, pr.email);
        }
    }
    if (shell_type_dir(type_field)[0] == '\0') shell_guess_type(type_field, platform_field);
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Open, win, &is_open);
    if (!is_open) {
        shell_requester("The window did not open.\nThe screen or window could not be opened. Close other programs to free memory, then start again.");
        MUI_DisposeObject(app);
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    SetAttrs(preview_list, UPA_List_Count, 0UL, TAG_DONE);
    shell_seed_status(status, &g_groups[0]);
    for (;;) {
        ULONG id = DoMethod(app, MUIM_Application_NewInput, (ULONG)&sigs);
        if (id == (ULONG)MUIV_Application_ReturnID_Quit || (SetSignal(0, 0) & SIGBREAKF_CTRL_C) != 0) break;
        if (id == ID_BUILD) {
            shell_build(status, g_groups, group_cycle, name_field, code_field, platform_field, desc_field, format_cycle);
        }
        if (id == ID_PLATFORM) shell_guess_type(type_field, platform_field);
        if (id == ID_GROUP) {
            ULONG ga = 0;
            GetAttr(UPA_Cycle_Active, group_cycle, &ga);
            if (ga < (ULONG)g_ngroups) shell_seed_status(status, &g_groups[ga]);
        }
        if (id == ID_CODE) shell_code_check(status, g_groups, group_cycle, code_field);
        if (id == ID_NAME) shell_suggest_code(name_field, code_field, group_cycle);
        if (id == ID_COVER && shell_pick_cover()) SetAttrs(cover_text, UPA_Text_Contents, (ULONG)g_cover, TAG_DONE);
        if (id == ID_PUBLISH || id == ID_REVOKE || id == ID_DOOR) {
            shell_online(app, status, (int)id, g_groups, group_cycle, name_field, code_field, platform_field, desc_field,
                         format_cycle);
        }
        if (id == ID_AMINET) {
            shell_aminet(app, status, g_groups, group_cycle, code_field, desc_field, format_cycle, type_field, arch_field,
                         uname_field, email_field);
        }
        if (id == ID_PICK && shell_pick_drawer()) {
            SetAttrs(files_text, UPA_Text_Contents, (ULONG)g_files, TAG_DONE);
            id = ID_PREVIEW;
        }
        if (id == ID_FLATTEN && g_prev.nnested > 0) {
            strncpy(g_files, g_prev.nested[0].flatten_to, sizeof(g_files) - 1);
            g_files[sizeof(g_files) - 1] = '\0';
            SetAttrs(files_text, UPA_Text_Contents, (ULONG)g_files, TAG_DONE);
            id = ID_PREVIEW;
        }
        if (id == ID_PREVIEW) {
            shell_refresh_preview(preview_list, warn_row, warn_text, g_groups, group_cycle, code_field, status);
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
