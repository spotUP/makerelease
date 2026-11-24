/* The group editor window of the MUI shell: edits the group drawers
 * (profile.json, file_id.diz, release.nfo) through profile_save.h. All the
 * parsing and validation sits in editor_logic.c and profile_save.c; this file
 * only builds the window and moves text between widgets and rp_group. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <dos/dos.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <libraries/asl.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/asl.h>

#include <mcc/up.h>

#include "editor_logic.h"
#include "groups.h"
#include "profile_save.h"
#include "ui_editor.h"

/* Same IntuitionBase arrangement as ui_amiga.c: the shell owns the global. */
#define IntuitionBase shell_IntuitionBase
extern struct IntuitionBase *IntuitionBase;

#ifndef MUIA_Window_Sleep
#define MUIA_Window_Sleep 0x8042e7d7UL
#endif
#ifndef MUIA_Disabled
#define MUIA_Disabled 0x80423661UL
#endif

#define ED_MAX_GROUPS 32
#define ED_DIZ_MAX 8192
#define ED_NFO_MAX 16384
#define ED_MEMBERS_MAX 5000

#define EID_CLOSE 101
#define EID_SELECT 102
#define EID_NEW 103
#define EID_DUPLICATE 104
#define EID_DELETE 105
#define EID_SAVE 106
#define EID_RELOAD 107
#define EID_IMPORT 108
#define EID_OK 109
#define EID_CANCEL 110
#define EID_DIRTY 111
#define EID_SHOWKEY 112

static const char *const ed_renderer_labels[] = {"Up Rough", "X Innovations", "Placeholder", NULL};
static const char *const ed_backend_labels[] = {"none", "doorserver", NULL};
static const char *const ed_tab_captions[] = {"Group", "Templates", NULL};

/* Big state is static: the shell runs on a 48 KB stack and rp_group is about 4.5 KB. */
static rp_group g_cur, g_scratch;
static char g_ids[ED_MAX_GROUPS][RP_ID_MAX + 1];
static char g_row_text[ED_MAX_GROUPS][RP_LABEL_MAX + RP_ID_MAX + 16];
static int g_nlist;
static const char *g_dir;
static char g_status_buf[512];
static char g_members_buf[ED_MEMBERS_MAX + 1];
static int g_is_new, g_diz_locked, g_nfo_locked, g_changed;
static int g_dirty, g_row = -1; /* unsaved edits; the list row the form shows (-1 = new group) */
static Object *g_app;
static char g_asl_drawer[512], g_asl_file[128]; /* the template requester remembers where it was */

static Object *g_win, *g_list, *g_status, *g_tabs, *g_pages;
static Object *g_f_label, *g_f_id, *g_f_url, *g_f_prefix, *g_f_code, *g_c_renderer, *g_c_backend, *g_f_base, *g_f_key, *g_f_key_plain;
static Object *g_k_showkey; /* the key shows as dots in g_f_key; ticking Show swaps in g_f_key_plain (MUIA_String_Secret is creation-only in MUI) */
static Object *g_a_members, *g_a_diz, *g_a_nfo, *g_k_nfo_remove;
static struct Hook g_row_hook;

ULONG rp_editor_row_c(UP_ListCell *cell);
ULONG rp_editor_row_c(UP_ListCell *cell) {
    if (cell->row < 0 || cell->row >= g_nlist) return (ULONG) "";
    return (ULONG)g_row_text[cell->row];
}
extern void rp_editor_row_tramp(void);
__asm__(".text\n.even\n.globl _rp_editor_row_tramp\n_rp_editor_row_tramp:\n"
        "\tmove.l %a1,-(%sp)\n\tjsr _rp_editor_row_c\n\taddq.l #4,%sp\n\trts\n");

static void ed_status(const char *text) {
    strncpy(g_status_buf, text, sizeof(g_status_buf) - 1);
    g_status_buf[sizeof(g_status_buf) - 1] = '\0';
    SetAttrs(g_status, UPA_Text_Contents, (ULONG)g_status_buf, TAG_DONE);
}

/* EasyRequest with the given gadgets; returns 1 for the first, 0 for the last, 2 for the second of three. */
static int ed_choice(const char *text, const char *gadgets) {
    struct EasyStruct es;
    struct IntuitionBase *prev = IntuitionBase;
    struct IntuitionBase *ib = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR) "intuition.library", 36);
    LONG r;
    if (ib == NULL) return 0;
    IntuitionBase = ib;
    es.es_StructSize = sizeof es;
    es.es_Flags = 0;
    es.es_Title = (UBYTE *)"Edit groups";
    es.es_TextFormat = (UBYTE *)"%s";
    es.es_GadgetFormat = (UBYTE *)gadgets;
    r = EasyRequest(NULL, &es, NULL, (APTR)text);
    CloseLibrary((struct Library *)ib);
    IntuitionBase = prev;
    return (int)r;
}

/* Programmatic fills fire the edit notifications too: drain the queued ids so only real edits set the flag. */
static void ed_clean(void) {
    ULONG sigs = 0, id, other = 0;
    while ((id = DoMethod(g_app, MUIM_Application_NewInput, (ULONG)&sigs)) != 0) {
        if (id != EID_DIRTY && id != EID_SELECT) other = id;
    }
    if (other != 0) DoMethod(g_app, MUIM_Application_ReturnID, other);
    g_dirty = 0;
}

static int ed_confirm_discard(void) {
    return !g_dirty || ed_choice("Discard the unsaved changes?", "Discard|Cancel") == 1;
}

static Object *ed_button(const char *caption, ULONG role) {
    return MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG)caption, UPA_Button_Align, (ULONG)UPV_Button_Align_Centre,
                         UPA_Button_Role, role, MUIA_Weight, 1UL, TAG_DONE);
}

static void ed_notify(Object *app, Object *btn, ULONG id) {
    DoMethod(btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, id);
}

/* The next return id; Quit and Ctrl-C count as close. */
static ULONG ed_next(Object *app) {
    ULONG sigs = 0, id = DoMethod(app, MUIM_Application_NewInput, (ULONG)&sigs);
    if (id == (ULONG)MUIV_Application_ReturnID_Quit || (SetSignal(0, 0) & SIGBREAKF_CTRL_C) != 0) return EID_CLOSE;
    if (id == 0 && sigs != 0) Wait(sigs | SIGBREAKF_CTRL_C);
    return id;
}

static const char *ed_area_text(Object *area) {
    ULONG p = 0;
    GetAttr(UPA_TextArea_Text, area, &p);
    return p != 0 ? (const char *)p : "";
}

/* A template file into a text area. Returns 1 when it is longer than the area holds (the area then shows a cut copy), 0 when it fits, -1 on a read error. */
static int ed_load_template(Object *area, const char *id, const char *file, size_t max, char *err, size_t errcap) {
    char path[RP_ID_MAX + 160];
    char *txt = NULL;
    int locked = 0;
    snprintf(path, sizeof path, "%s/%s/%s", g_dir, id, file);
    if (rp_read_template(path, 1, &txt, err, errcap) != 0) return -1;
    if (txt != NULL && strlen(txt) > max) locked = 1;
    SetAttrs(area, UPA_TextArea_Text, (ULONG)(txt != NULL ? txt : ""), TAG_DONE);
    free(txt);
    return locked;
}

/* The server key text of whichever key field is showing. */
static const char *ed_key_text(void) {
    ULONG on = 0;
    GetAttr(MUIA_Selected, g_k_showkey, &on);
    return shell_field_text(on ? g_f_key_plain : g_f_key);
}

/* Show ticked: the plain field takes over (and the masked one hides), carrying the text across. */
static void ed_apply_showkey(void) {
    ULONG on = 0;
    GetAttr(MUIA_Selected, g_k_showkey, &on);
    shell_set_field(on ? g_f_key_plain : g_f_key, shell_field_text(on ? g_f_key : g_f_key_plain));
    SetAttrs(g_f_key, MUIA_ShowMe, (ULONG)!on, TAG_DONE);
    SetAttrs(g_f_key_plain, MUIA_ShowMe, (ULONG)on, TAG_DONE);
}

static void ed_fill_form(const rp_group *g) {
    char num[16];
    shell_set_field(g_f_label, g->label);
    shell_set_field(g_f_id, g->id);
    shell_set_field(g_f_url, g->url);
    shell_set_field(g_f_prefix, g->file_prefix);
    snprintf(num, sizeof num, "%d", g->code_length);
    shell_set_field(g_f_code, g->code_length > 0 ? num : "");
    SetAttrs(g_c_renderer, UPA_Cycle_Active, (ULONG)g->renderer, TAG_DONE);
    SetAttrs(g_c_backend, UPA_Cycle_Active, (ULONG)(g->backend_doorserver ? 1 : 0), TAG_DONE);
    shell_set_field(g_f_base, g->server_base);
    shell_set_field(g_f_key, g->server_key);
    shell_set_field(g_f_key_plain, g->server_key);
    SetAttrs(g_k_showkey, MUIA_Selected, (ULONG)FALSE, TAG_DONE);
    SetAttrs(g_f_key, MUIA_ShowMe, (ULONG)TRUE, TAG_DONE);
    SetAttrs(g_f_key_plain, MUIA_ShowMe, (ULONG)FALSE, TAG_DONE);
    g_members_buf[0] = '\0';
    rp_editor_members_format(g->members, g->nmembers, g_members_buf, sizeof g_members_buf);
    SetAttrs(g_a_members, UPA_TextArea_Text, (ULONG)g_members_buf, TAG_DONE);
    SetAttrs(g_k_nfo_remove, MUIA_Selected, (ULONG)FALSE, TAG_DONE);
}

static void ed_set_mode_new(int is_new) {
    g_is_new = is_new;
    SetAttrs(g_f_id, MUIA_Disabled, (ULONG)(is_new ? FALSE : TRUE), TAG_DONE);
}

/* Rebuilds the list rows from the group directory; select_id (or none) becomes the active row. Returns that row or -1. */
static int ed_refresh_list(const char *select_id) {
    char err[256];
    int i, sel = -1;
    g_nlist = rp_list_groups(g_dir, g_ids, ED_MAX_GROUPS, err, sizeof err);
    if (g_nlist < 0) {
        g_nlist = 0;
        ed_status(err);
    }
    for (i = 0; i < g_nlist; i++) {
        if (rp_load_group(g_dir, g_ids[i], &g_scratch, err, sizeof err) == 0)
            snprintf(g_row_text[i], sizeof g_row_text[i], "%s (%s)", g_scratch.label, g_ids[i]);
        else
            snprintf(g_row_text[i], sizeof g_row_text[i], "%s (unreadable)", g_ids[i]);
        if (select_id != NULL && strcmp(select_id, g_ids[i]) == 0) sel = i;
    }
    SetAttrs(g_list, UPA_List_Count, (ULONG)g_nlist, UPA_List_Active, (ULONG)sel, TAG_DONE);
    return sel;
}

/* Loads group row into the form, discarding unsaved edits. */
static void ed_select(int row) {
    char err[320], note[400];
    int d, n;
    if (row < 0 || row >= g_nlist) return;
    if (rp_load_group(g_dir, g_ids[row], &g_cur, err, sizeof err) != 0) {
        ed_status(err);
        return;
    }
    ed_set_mode_new(0);
    ed_fill_form(&g_cur);
    d = ed_load_template(g_a_diz, g_ids[row], "file_id.diz", ED_DIZ_MAX, err, sizeof err);
    n = ed_load_template(g_a_nfo, g_ids[row], "release.nfo", ED_NFO_MAX, err, sizeof err);
    if (d < 0 || n < 0) {
        ed_status(err);
        return;
    }
    g_diz_locked = d > 0;
    g_nfo_locked = n > 0;
    g_row = row;
    ed_clean();
    snprintf(note, sizeof note, "Editing %s.%s%s", g_ids[row], g_diz_locked ? " file_id.diz is too long for this editor and is kept as it is." : "",
             g_nfo_locked ? " release.nfo is too long for this editor and is kept as it is." : "");
    ed_status(note);
}

static void ed_new(void) {
    if (!ed_confirm_discard()) return;
    memset(&g_cur, 0, sizeof g_cur);
    SetAttrs(g_list, UPA_List_Active, (ULONG)-1, TAG_DONE);
    ed_set_mode_new(1);
    ed_fill_form(&g_cur);
    shell_set_field(g_f_code, "");
    SetAttrs(g_a_diz, UPA_TextArea_Text, (ULONG) "", TAG_DONE);
    SetAttrs(g_a_nfo, UPA_TextArea_Text, (ULONG) "", TAG_DONE);
    g_diz_locked = g_nfo_locked = 0;
    g_row = -1;
    ed_clean();
    ed_status("New group: fill the fields, import a file_id.diz on the Templates tab, then Save.");
}

/* Form into g_cur; 0, or -1 with the reason in err. */
static int ed_collect(char *err, size_t errcap) {
    size_t n = 0;
    ULONG a = 0;
    rp_group *g = &g_cur;
    if (rp_editor_set_field(g->label, sizeof g->label, shell_field_text(g_f_label), "Label", err, errcap) != 0) return -1;
    if (g->label[0] == '\0') {
        snprintf(err, errcap, "Label is empty.");
        return -1;
    }
    if (g_is_new) {
        char id[RP_ID_MAX + 1];
        if (rp_editor_set_field(id, sizeof id, shell_field_text(g_f_id), "Id", err, errcap) != 0) return -1;
        if (rp_editor_check_id(id, err, errcap) != 0) return -1;
        strcpy(g->id, id);
    }
    if (rp_editor_set_field(g->url, sizeof g->url, shell_field_text(g_f_url), "Url", err, errcap) != 0) return -1;
    if (rp_editor_set_field(g->file_prefix, sizeof g->file_prefix, shell_field_text(g_f_prefix), "File prefix", err, errcap) != 0) return -1;
    if (rp_editor_code_length(shell_field_text(g_f_code), &g->code_length, err, errcap) != 0) return -1;
    GetAttr(UPA_Cycle_Active, g_c_renderer, &a);
    g->renderer = a == 1 ? R_XINNOVATIONS : a == 2 ? R_PLACEHOLDER : R_UPROUGH;
    GetAttr(UPA_Cycle_Active, g_c_backend, &a);
    g->backend_doorserver = a == 1;
    if (rp_editor_set_field(g->server_base, sizeof g->server_base, shell_field_text(g_f_base), "Server base", err, errcap) != 0) return -1;
    if (rp_editor_set_field(g->server_key, sizeof g->server_key, ed_key_text(), "Server key", err, errcap) != 0) return -1;
    if (rp_editor_members_parse(ed_area_text(g_a_members), g->members, RP_MEMBERS_MAX, &n, err, errcap) != 0) return -1;
    g->nmembers = n;
    return 0;
}

static void ed_save(void) {
    char err[320], id[RP_ID_MAX + 1];
    const char *diz = ed_area_text(g_a_diz), *nfo = ed_area_text(g_a_nfo);
    ULONG remove = 0;
    if (ed_collect(err, sizeof err) != 0) {
        ed_status(err);
        return;
    }
    if (g_is_new && diz[0] == '\0') {
        ed_status("Import a file_id.diz on the Templates tab first: Build needs it.");
        return;
    }
    GetAttr(MUIA_Selected, g_k_nfo_remove, &remove);
    if (rp_group_save(g_dir, &g_cur, g_is_new, g_diz_locked ? NULL : diz, (g_nfo_locked || remove) ? NULL : nfo, remove ? 1 : 0, err, sizeof err) != 0) {
        ed_status(err);
        return;
    }
    g_changed = 1;
    strcpy(id, g_cur.id);
    ed_select(ed_refresh_list(id));
    snprintf(err, sizeof err, "Saved %s.", id);
    ed_status(err);
}

/* A small window with one string field; returns 1 with the text in out, or 0 on Cancel. */
static int ed_ask(Object *app, const char *title, const char *prompt, char *out, size_t cap) {
    Object *win, *field, *ok, *cancel;
    ULONG id, opened = 0;
    int result = 0;
    field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG)prompt, MUIA_String_MaxLen, (ULONG)RP_ID_MAX + 1, TAG_DONE);
    ok = ed_button("OK", UPV_Button_Role_Accent);
    cancel = ed_button("Cancel", UPV_Button_Role_Normal);
    if (field == NULL || ok == NULL || cancel == NULL) {
        if (field != NULL) MUI_DisposeObject(field);
        if (ok != NULL) MUI_DisposeObject(ok);
        if (cancel != NULL) MUI_DisposeObject(cancel);
        return 0;
    }
    win = MUI_NewObject((char *)MUIC_Window, MUIA_Window_Title, (ULONG)title, MUIA_Window_ID, 0x45444944UL,
                        MUIA_Window_LeftEdge, (ULONG)MUIV_Window_LeftEdge_Centered, MUIA_Window_TopEdge, (ULONG)MUIV_Window_TopEdge_Centered,
                        MUIA_Window_Width, (ULONG)MUIV_Window_Width_Screen(40),
                        MUIA_Window_RootObject,
                        (ULONG)MUI_NewObject((char *)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Surface,
                                             MUIA_Group_Child, (ULONG)shell_card(prompt, field, shell_row(ok, cancel), NULL, NULL, NULL, NULL, NULL),
                                             TAG_DONE),
                        TAG_DONE);
    if (win == NULL) {
        MUI_DisposeObject(field);
        MUI_DisposeObject(ok);
        MUI_DisposeObject(cancel);
        return 0;
    }
    DoMethod(app, OM_ADDMEMBER, (ULONG)win);
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)EID_CANCEL);
    ed_notify(app, ok, EID_OK);
    ed_notify(app, cancel, EID_CANCEL);
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Open, win, &opened);
    if (opened) {
        for (;;) {
            id = ed_next(app);
            if (id == EID_OK) {
                result = 1;
                break;
            }
            if (id == EID_CANCEL || id == EID_CLOSE) break;
        }
        if (result) {
            strncpy(out, shell_field_text(field), cap - 1);
            out[cap - 1] = '\0';
        }
        SetAttrs(win, MUIA_Window_Open, FALSE, TAG_DONE);
    }
    DoMethod(app, OM_REMMEMBER, (ULONG)win);
    MUI_DisposeObject(win);
    return result;
}

static int ed_pick_file(char *out, size_t cap) {
    struct FileRequester *req = AllocAslRequestTags(ASL_FileRequest, ASLFR_TitleText, (ULONG) "Import a template", ASLFR_RejectIcons, TRUE,
                                                    ASLFR_InitialDrawer, (ULONG)g_asl_drawer, ASLFR_InitialFile, (ULONG)g_asl_file, TAG_DONE);
    int ok = 0;
    if (req == NULL) return 0;
    if (AslRequest(req, NULL) && req->rf_File != NULL && req->rf_File[0] != '\0') {
        strncpy(g_asl_drawer, (const char *)req->rf_Dir, sizeof g_asl_drawer - 1);
        strncpy(g_asl_file, (const char *)req->rf_File, sizeof g_asl_file - 1);
        strncpy(out, (const char *)req->rf_Dir, cap - 1);
        out[cap - 1] = '\0';
        ok = AddPart((STRPTR)out, req->rf_File, (ULONG)cap) ? 1 : 0;
    }
    FreeAslRequest(req);
    return ok;
}

static void ed_import(void) {
    char path[512], err[320], *txt = NULL;
    int which = ed_choice("Import the file into which template?", "file_id.diz|release.nfo|Cancel");
    Object *area = which == 1 ? g_a_diz : g_a_nfo;
    size_t max = which == 1 ? ED_DIZ_MAX : ED_NFO_MAX;
    if (which != 1 && which != 2) return;
    if (!ed_pick_file(path, sizeof path)) return;
    if (rp_read_template(path, 0, &txt, err, sizeof err) != 0) {
        ed_status(err);
        return;
    }
    if (txt != NULL && strlen(txt) > max) {
        ed_status("That file is longer than this editor holds. Nothing was imported.");
        free(txt);
        return;
    }
    SetAttrs(area, UPA_TextArea_Text, (ULONG)(txt != NULL ? txt : ""), TAG_DONE);
    free(txt);
    if (which == 1) g_diz_locked = 0;
    else g_nfo_locked = 0;
    ed_status("Template imported. Press Save to write it.");
}

static void ed_duplicate(Object *app) {
    char newid[RP_ID_MAX + 1], err[320];
    ULONG row = (ULONG)-1;
    GetAttr(UPA_List_Active, g_list, &row);
    if ((LONG)row < 0 || (LONG)row >= g_nlist || g_is_new) {
        ed_status("Select a saved group to duplicate.");
        return;
    }
    if (!ed_confirm_discard()) return;
    if (!ed_ask(app, "Duplicate group", "Id of the new group", newid, sizeof newid)) return;
    if (rp_editor_check_id(newid, err, sizeof err) != 0 || rp_group_duplicate(g_dir, g_ids[row], newid, err, sizeof err) != 0) {
        ed_status(err);
        return;
    }
    g_changed = 1;
    ed_select(ed_refresh_list(newid));
    snprintf(err, sizeof err, "Duplicated into %s.", newid);
    ed_status(err);
}

static void ed_delete(void) {
    char err[360], id[RP_ID_MAX + 1];
    ULONG row = (ULONG)-1;
    GetAttr(UPA_List_Active, g_list, &row);
    if ((LONG)row < 0 || (LONG)row >= g_nlist || g_is_new) {
        ed_status("Select a saved group to delete.");
        return;
    }
    strcpy(id, g_ids[row]);
    snprintf(err, sizeof err, "Delete the group %s and all its files?", id);
    if (ed_choice(err, "Delete|Cancel") != 1) return;
    if (rp_group_delete(g_dir, id, err, sizeof err) != 0) {
        ed_status(err);
        return;
    }
    g_changed = 1;
    ed_refresh_list(NULL);
    if (g_nlist > 0) {
        SetAttrs(g_list, UPA_List_Active, 0UL, TAG_DONE);
        ed_select(0);
    }
    snprintf(err, sizeof err, "Deleted %s.", id);
    ed_status(err);
}

static void ed_reload(void) {
    ULONG row = (ULONG)-1;
    char id[RP_ID_MAX + 1];
    GetAttr(UPA_List_Active, g_list, &row);
    id[0] = '\0';
    if (!g_is_new && (LONG)row >= 0 && (LONG)row < g_nlist) strcpy(id, g_ids[row]);
    if (id[0] != '\0' && !ed_confirm_discard()) return;
    ed_select(ed_refresh_list(id[0] ? id : NULL));
    if (id[0] == '\0') ed_status("Reloaded the list.");
}

int rp_editor_run(Object *app, Object *parent_win, const char *dir) {
    Object *new_btn, *dup_btn, *del_btn, *reload_btn, *save_btn, *close_btn, *import_btn;
    Object *page_group, *page_tpl, *left, *right, *bottom, *content;
    ULONG opened = 0, id;
    g_dir = dir;
    g_app = app;
    g_dirty = 0;
    g_row = -1;
    g_changed = 0;
    g_win = NULL;
    *(void **)&g_row_hook.h_Entry = (void *)rp_editor_row_tramp;
    g_list = MUI_NewObject((char *)"UP-List.mcc", UPA_List_RowHook, (ULONG)&g_row_hook, UPA_List_MaxRows, 10UL, UPA_List_Count, 6UL, TAG_DONE);
    /* weight outweighs the buttons beside it, so a long message gets the row instead of running under Save */
    g_status = MUI_NewObject((char *)"UP-Text.mcc", UPA_Text_Contents, (ULONG) "Pick a group, or press New.", UPA_Text_Tone, (ULONG)UPV_Text_Tone_Muted,
                             MUIA_Weight, 1000UL, TAG_DONE);
    g_f_label = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Shown in the Group list", MUIA_String_MaxLen, (ULONG)RP_LABEL_MAX + 1, TAG_DONE);
    g_f_id = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Drawer name, letters and digits", MUIA_String_MaxLen, (ULONG)RP_ID_MAX + 1, TAG_DONE);
    g_f_url = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Group web address", MUIA_String_MaxLen, (ULONG)RP_URL_MAX + 1, TAG_DONE);
    g_f_prefix = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "File name prefix", MUIA_String_MaxLen, (ULONG)RP_PREFIX_MAX + 1, TAG_DONE);
    g_f_code = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "1 to 32", MUIA_String_MaxLen, 3UL, TAG_DONE);
    g_c_renderer = MUI_NewObject((char *)"UP-Cycle.mcc", UPA_Cycle_Entries, (ULONG)ed_renderer_labels, TAG_DONE);
    g_c_backend = MUI_NewObject((char *)"UP-Cycle.mcc", UPA_Cycle_Entries, (ULONG)ed_backend_labels, TAG_DONE);
    g_f_base = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Door server base address", MUIA_String_MaxLen, (ULONG)RP_BASE_MAX + 1, TAG_DONE);
    g_f_key = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Door server key", MUIA_String_MaxLen, (ULONG)RP_KEY_MAX + 1,
                            MUIA_String_Secret, (ULONG)TRUE, TAG_DONE);
    g_f_key_plain = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Door server key", MUIA_String_MaxLen, (ULONG)RP_KEY_MAX + 1, TAG_DONE);
    g_k_showkey = MUI_NewObject((char *)"UP-Checkmark.mcc", UPA_Checkmark_Label, (ULONG) "Show", TAG_DONE);
    g_a_members = MUI_NewObject((char *)"UP-Textarea.mcc", UPA_TextArea_MaxLen, (ULONG)ED_MEMBERS_MAX, UPA_TextArea_Placeholder, (ULONG) "Nickname | role, one member per line", TAG_DONE);
    g_a_diz = MUI_NewObject((char *)"UP-Textarea.mcc", UPA_TextArea_MaxLen, (ULONG)ED_DIZ_MAX, UPA_TextArea_Placeholder, (ULONG) "file_id.diz template", TAG_DONE);
    g_a_nfo = MUI_NewObject((char *)"UP-Textarea.mcc", UPA_TextArea_MaxLen, (ULONG)ED_NFO_MAX, UPA_TextArea_Placeholder, (ULONG) "release.nfo template (optional)", TAG_DONE);
    g_k_nfo_remove = MUI_NewObject((char *)"UP-Checkmark.mcc", UPA_Checkmark_Label, (ULONG) "Remove NFO template", TAG_DONE);
    new_btn = ed_button("New", UPV_Button_Role_Normal);
    dup_btn = ed_button("Duplicate", UPV_Button_Role_Normal);
    del_btn = ed_button("Delete", UPV_Button_Role_Normal);
    reload_btn = ed_button("Reload", UPV_Button_Role_Normal);
    import_btn = ed_button("Import template", UPV_Button_Role_Normal);
    save_btn = ed_button("Save", UPV_Button_Role_Accent);
    close_btn = ed_button("Close", UPV_Button_Role_Normal);
    g_tabs = MUI_NewObject((char *)"UP-Tabs.mcc", UPA_Tabs_Captions, (ULONG)ed_tab_captions, TAG_DONE);
    if (g_list == NULL || g_status == NULL || g_f_label == NULL || g_f_id == NULL || g_f_url == NULL || g_f_prefix == NULL || g_f_code == NULL ||
        g_c_renderer == NULL || g_c_backend == NULL || g_f_base == NULL || g_f_key == NULL || g_f_key_plain == NULL || g_k_showkey == NULL || g_a_members == NULL || g_a_diz == NULL || g_a_nfo == NULL ||
        g_k_nfo_remove == NULL || new_btn == NULL || dup_btn == NULL || del_btn == NULL || reload_btn == NULL || import_btn == NULL || save_btn == NULL ||
        close_btn == NULL || g_tabs == NULL) {
        shell_requester("The group editor objects could not be created.\nFree some memory and check the UP-MCC install.");
        return 0;
    }
    SetAttrs(g_f_key_plain, MUIA_ShowMe, (ULONG)FALSE, TAG_DONE);
    page_group = shell_card("Group", shell_form(row_label("Label"), g_f_label, row_label("Id"), g_f_id, row_label("Url"), g_f_url,
                                                row_label("File prefix"), g_f_prefix, row_label("Code length"), g_f_code,
                                                row_label("Renderer"), g_c_renderer, row_label("Backend"), g_c_backend,
                                                row_label("Server base"), g_f_base, row_label("Server key"),
                                                MUI_NewObject((char *)MUIC_Group, MUIA_Group_Horiz, (ULONG)TRUE, MUIA_FillArea, (ULONG)FALSE, MUIA_Group_Child, (ULONG)g_f_key,
                                                              MUIA_Group_Child, (ULONG)g_f_key_plain, MUIA_Group_Child, (ULONG)g_k_showkey, TAG_DONE),
                                                area_label("Members"), g_a_members),
                            NULL, NULL, NULL, NULL, NULL, NULL);
    page_tpl = shell_card("Templates", shell_form(area_label("file_id.diz"), g_a_diz, area_label("release.nfo"), g_a_nfo, row_label("NFO template"), g_k_nfo_remove,
                                                  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
                          shell_row(import_btn, NULL), NULL, NULL, NULL, NULL, NULL);
    g_pages = MUI_NewObject((char *)MUIC_Group, MUIA_Group_PageMode, (ULONG)TRUE, MUIA_FillArea, (ULONG)FALSE,
                            MUIA_Group_Child, (ULONG)top_page(page_group), MUIA_Group_Child, (ULONG)top_page(page_tpl), TAG_DONE);
    left = shell_card("Groups", g_list, shell_row(new_btn, dup_btn), shell_row(del_btn, reload_btn), NULL, NULL, NULL, NULL);
    right = MUI_NewObject((char *)MUIC_Group, MUIA_FillArea, (ULONG)FALSE, MUIA_Group_Spacing, 0UL, MUIA_Weight, 200UL,
                          MUIA_Group_Child, (ULONG)g_tabs, MUIA_Group_Child, (ULONG)g_pages, TAG_DONE);
    bottom = MUI_NewObject((char *)MUIC_Group, MUIA_Group_Horiz, (ULONG)TRUE, MUIA_FillArea, (ULONG)FALSE,
                           MUIA_Group_Child, (ULONG)g_status, MUIA_Group_Child, (ULONG)save_btn, MUIA_Group_Child, (ULONG)close_btn, TAG_DONE);
    content = MUI_NewObject((char *)MUIC_Group, MUIA_FillArea, (ULONG)FALSE,
                            MUIA_Group_Child, (ULONG)MUI_NewObject((char *)MUIC_Group, MUIA_Group_Horiz, (ULONG)TRUE, MUIA_FillArea, (ULONG)FALSE,
                                                                   MUIA_Group_Child, (ULONG)MUI_NewObject((char *)MUIC_Group, MUIA_FillArea, (ULONG)FALSE, MUIA_Weight, 60UL,
                                                                                                          MUIA_Group_Child, (ULONG)left, TAG_DONE),
                                                                   MUIA_Group_Child, (ULONG)right, TAG_DONE),
                            MUIA_Group_Child, (ULONG)shell_gap(0, 4), MUIA_Group_Child, (ULONG)bottom, TAG_DONE);
    g_win = MUI_NewObject((char *)MUIC_Window, MUIA_Window_Title, (ULONG) "Edit groups", MUIA_Window_ID, 0x52504544UL,
                          MUIA_Window_Width, (ULONG)MUIV_Window_Width_Screen(85), MUIA_Window_LeftEdge, (ULONG)MUIV_Window_LeftEdge_Centered,
                          MUIA_Window_TopEdge, (ULONG)MUIV_Window_TopEdge_Centered,
                          MUIA_Window_RootObject,
                          (ULONG)MUI_NewObject((char *)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Surface, MUIA_Group_Child, (ULONG)content, TAG_DONE),
                          TAG_DONE);
    if (g_win == NULL) {
        shell_requester("The group editor window could not be created.\nFree some memory and try again.");
        return 0;
    }
    DoMethod(app, OM_ADDMEMBER, (ULONG)g_win);
    DoMethod(g_win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)EID_CLOSE);
    DoMethod(g_tabs, MUIM_Notify, UPA_Tabs_Active, MUIV_EveryTime, (ULONG)g_pages, 3UL, MUIM_Set, MUIA_Group_ActivePage, MUIV_TriggerValue);
    DoMethod(g_list, MUIM_Notify, UPA_List_Active, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)EID_SELECT);
    {
        Object *fields[] = {g_f_label, g_f_id, g_f_url, g_f_prefix, g_f_code, g_f_base, g_f_key, g_f_key_plain};
        Object *areas[] = {g_a_members, g_a_diz, g_a_nfo};
        size_t i;
        for (i = 0; i < sizeof fields / sizeof fields[0]; i++) {
            ULONG str = 0;
            GetAttr(UPA_Field_String, fields[i], &str);
            if (str != 0) DoMethod((Object *)str, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)EID_DIRTY);
        }
        for (i = 0; i < sizeof areas / sizeof areas[0]; i++)
            DoMethod(areas[i], MUIM_Notify, UPA_TextArea_Text, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)EID_DIRTY);
        DoMethod(g_c_renderer, MUIM_Notify, UPA_Cycle_Active, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)EID_DIRTY);
        DoMethod(g_c_backend, MUIM_Notify, UPA_Cycle_Active, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)EID_DIRTY);
        DoMethod(g_k_showkey, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)EID_SHOWKEY);
        DoMethod(g_k_nfo_remove, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)EID_DIRTY);
    }
    ed_notify(app, new_btn, EID_NEW);
    ed_notify(app, dup_btn, EID_DUPLICATE);
    ed_notify(app, del_btn, EID_DELETE);
    ed_notify(app, reload_btn, EID_RELOAD);
    ed_notify(app, import_btn, EID_IMPORT);
    ed_notify(app, save_btn, EID_SAVE);
    ed_notify(app, close_btn, EID_CLOSE);
    SetAttrs(parent_win, MUIA_Window_Sleep, TRUE, TAG_DONE);
    SetAttrs(g_win, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Open, g_win, &opened);
    if (!opened) {
        shell_requester("The group editor window did not open.\nClose other programs to free memory, then try again.");
    } else {
        ed_set_mode_new(0);
        ed_refresh_list(NULL);
        if (g_nlist > 0) {
            SetAttrs(g_list, UPA_List_Active, 0UL, TAG_DONE);
            ed_select(0);
        }
        for (;;) {
            id = ed_next(app);
            if (id == EID_CLOSE) break;
            if (id == EID_SELECT) {
                ULONG row = (ULONG)-1;
                GetAttr(UPA_List_Active, g_list, &row);
                if ((LONG)row >= 0 && (int)row != g_row) {
                    if (ed_confirm_discard()) ed_select((int)row);
                    else SetAttrs(g_list, UPA_List_Active, (ULONG)g_row, TAG_DONE);
                }
            } else if (id == EID_NEW) ed_new();
            else if (id == EID_DUPLICATE) ed_duplicate(app);
            else if (id == EID_DELETE) ed_delete();
            else if (id == EID_SAVE) ed_save();
            else if (id == EID_RELOAD) ed_reload();
            else if (id == EID_IMPORT) ed_import();
            else if (id == EID_DIRTY) g_dirty = 1;
            else if (id == EID_SHOWKEY) {
                int was_dirty = g_dirty; /* the text copy fires the edit notifications, but Show is not an edit */
                ed_apply_showkey();
                ed_clean();
                g_dirty = was_dirty;
            }
        }
        SetAttrs(g_win, MUIA_Window_Open, FALSE, TAG_DONE);
    }
    SetAttrs(parent_win, MUIA_Window_Sleep, FALSE, TAG_DONE);
    DoMethod(app, OM_REMMEMBER, (ULONG)g_win);
    MUI_DisposeObject(g_win);
    g_win = NULL;
    return g_changed;
}
