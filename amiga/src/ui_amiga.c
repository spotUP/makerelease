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
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>

#include <mcc/up.h>

#include <libraries/asl.h>
#include <proto/asl.h>

#include "archive.h"
#include "archive_preview.h"
#include "date.h"
#include "groups.h"
#include "naming.h"
#include "nfo_uprough.h"
#include "nfo_x.h"
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

/* A row of one or two widgets; a NULL second half leaves a full-width row. */
static Object *shell_row(Object *a, Object *b) {
    if (b == NULL) {
        return MUI_NewObject((char *)MUIC_Group, MUIA_Group_Horiz, (ULONG)TRUE, MUIA_FillArea, (ULONG)FALSE,
                             MUIA_Group_Child, (ULONG)a, TAG_DONE);
    }
    return MUI_NewObject((char *)MUIC_Group, MUIA_Group_Horiz, (ULONG)TRUE, MUIA_FillArea, (ULONG)FALSE,
                         MUIA_Group_Child, (ULONG)a, MUIA_Group_Child, (ULONG)b, TAG_DONE);
}

/* A raised card: a heading line, then the widgets (NULLs skipped). */
static Object *shell_card(const char *title, Object *a, Object *b, Object *c, Object *d, Object *e, Object *f) {
    struct TagItem t[16];
    Object *kids[7];
    int i, n = 0;
    kids[0] = shell_label(title, UPV_Text_Tone_Title);
    kids[1] = a;
    kids[2] = b;
    kids[3] = c;
    kids[4] = d;
    kids[5] = e;
    kids[6] = f;
    t[n].ti_Tag = UPA_Panel_Role;
    t[n++].ti_Data = (ULONG)UPV_Panel_Role_Raised;
    for (i = 0; i < 7; i++) {
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

#define RP_MAX_FILES_PATH 511

static char g_files[RP_MAX_FILES_PATH + 1];

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

static const char *shell_field_text(Object *field) {
    ULONG ptr = 0;
    STRPTR txt = NULL;
    GetAttr(UPA_Field_String, field, &ptr);
    if (ptr != 0) GetAttr(MUIA_String_Contents, (Object *)ptr, (ULONG *)&txt);
    return txt ? (const char *)txt : "";
}

static void shell_refresh_preview(Object *list, Object *warn_row, Object *warn_text, const rp_group *groups,
                                  Object *group_cycle, Object *code_field, Object *status) {
    ULONG gactive = 0;
    char code[40], base[64], errmsg[256];
    const char *paths[1];
    rp_preview_free(&g_prev);
    SetAttrs(warn_row, MUIA_ShowMe, FALSE, TAG_DONE);
    if (g_files[0] != '\0') {
        GetAttr(UPA_Cycle_Active, group_cycle, &gactive);
        rp_upper(shell_field_text(code_field), code, sizeof(code));
        rp_base_name(groups[gactive].file_prefix, code, base, sizeof(base));
        paths[0] = g_files;
        if (rp_preview_archive(base, paths, 1, &g_prev, errmsg, sizeof(errmsg)) != 0) {
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

static void shell_build(Object *status, const rp_group *groups, Object *group_cycle, Object *name_field,
                        Object *code_field, Object *platform_field, Object *desc_field, Object *format_cycle) {
    /* The inner string gadgets hold the text; UPA_Field_String gives them. */
    ULONG name_ptr = 0, code_ptr = 0, plat_ptr = 0, desc_ptr = 0, gactive = 0, active = 0;
    STRPTR name_txt, code_txt, plat_txt, desc_txt;
    const rp_group *g;
    char path[256], *diz = NULL, *nfo = NULL, code[40], base[64], packed[72], date[16];
    char membuf[RP_MEMBERS_MAX][RP_NICK_MAX + RP_ROLE_MAX + 4];
    const char *memberlines[RP_MEMBERS_MAX];
    rp_scalar scalars[11];
    rp_block blocks[4];
    rp_values v;
    struct DateStamp ds;
    static const char *const no_lines;
    size_t i, width;
    int errline = 0;
    char errmsg[256], *rendered;

    GetAttr(UPA_Field_String, name_field, &name_ptr);
    GetAttr(UPA_Field_String, code_field, &code_ptr);
    GetAttr(UPA_Field_String, platform_field, &plat_ptr);
    GetAttr(UPA_Field_String, desc_field, &desc_ptr);
    name_txt = (STRPTR) "";
    code_txt = (STRPTR) "";
    plat_txt = (STRPTR) "";
    desc_txt = (STRPTR) "";
    if (name_ptr != 0) GetAttr(MUIA_String_Contents, (Object *)name_ptr, (ULONG *)&name_txt);
    if (code_ptr != 0) GetAttr(MUIA_String_Contents, (Object *)code_ptr, (ULONG *)&code_txt);
    if (plat_ptr != 0) GetAttr(MUIA_String_Contents, (Object *)plat_ptr, (ULONG *)&plat_txt);
    if (desc_ptr != 0) GetAttr(MUIA_String_Contents, (Object *)desc_ptr, (ULONG *)&desc_txt);
    if (name_txt == NULL) name_txt = (STRPTR) "";
    if (code_txt == NULL) code_txt = (STRPTR) "";
    if (plat_txt == NULL) plat_txt = (STRPTR) "";
    if (desc_txt == NULL) desc_txt = (STRPTR) "";
    GetAttr(UPA_Cycle_Active, group_cycle, &gactive);
    GetAttr(UPA_Cycle_Active, format_cycle, &active);
    g = &groups[gactive];
    DateStamp(&ds);
    rp_iso_date((long)ds.ds_Days + 2922, date, sizeof(date));
    rp_upper(code_txt, code, sizeof(code));
    rp_base_name(g->file_prefix, code, base, sizeof(base));
    snprintf(packed, sizeof(packed), "%s.%s", base, active ? "ZIP" : "LHA");
    snprintf(path, sizeof(path), "PROGDIR:groups/%s/file_id.diz", g->id);
    if (rp_read_template(path, 0, &diz, errmsg, sizeof(errmsg)) != 0) {
        SetAttrs(status, UPA_Text_Contents, (ULONG)errmsg, TAG_DONE);
        return;
    }
    width = 0;
    for (i = 0; i < g->nmembers; i++) {
        size_t nl = strlen(g->members[i].nickname);
        if (nl > width) width = nl;
    }
    for (i = 0; i < g->nmembers; i++) {
        snprintf(membuf[i], sizeof(membuf[i]), "%-*s  %s", (int)(width + 2), g->members[i].nickname,
                 g->members[i].role);
        memberlines[i] = membuf[i];
    }
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
    scalars[8].value = "?";
    scalars[9].name = "description";
    scalars[9].value = desc_txt;
    scalars[10].name = "creators";
    scalars[10].value = "";
    blocks[0].name = "notes";
    blocks[0].lines = &no_lines;
    blocks[0].nlines = 0;
    blocks[1].name = "news";
    blocks[1].lines = &no_lines;
    blocks[1].nlines = 0;
    blocks[2].name = "members";
    blocks[2].lines = memberlines;
    blocks[2].nlines = g->nmembers;
    blocks[3].name = "credits";
    blocks[3].lines = &no_lines;
    blocks[3].nlines = 0;
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
        hist.number = "?";
        hist.filenames = lower;
        hist.title = name_txt;
        hist.category = "";
        hist.creators = "";
        nf.release_name = name_txt;
        nf.platform = plat_txt;
        nf.code = code;
        nf.format_lha = active ? 0 : 1;
        nf.release_date = date;
        nf.notes = "";
        nf.news = "";
        nf.members = g->members;
        nf.nmembers = g->nmembers;
        nfo = rp_render_nfo_uprough(g->file_prefix, &hist, 1, &nf, errmsg, sizeof(errmsg));
        if (nfo == NULL) {
            SetAttrs(status, UPA_Text_Contents, (ULONG)(errmsg[0] ? errmsg : "out of memory"), TAG_DONE);
            free(diz);
            return;
        }
    } else {
        /* /X: the framed drawing. The boards and release list come from the
         * catalog (phase 2), so they are empty here; the rest is the form. */
        rp_nfo_x_form xf;
        memset(&xf, 0, sizeof xf);
        xf.release_name = name_txt;
        xf.release_date = date;
        xf.coder = "";
        xf.system = plat_txt;
        xf.design = "";
        xf.notes = "";
        xf.news = "";
        xf.members = g->members;
        xf.nmembers = g->nmembers;
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

/* Event ids above MUIV_Application_ReturnID_Quit are ours. */
#define RP_MAX_SHELL_GROUPS 16

static rp_group g_groups[RP_MAX_SHELL_GROUPS];
static const char *g_group_labels[RP_MAX_SHELL_GROUPS + 1];
static int g_ngroups;

static int shell_main(void) {
    Object *app = NULL, *win = NULL;
    Object *group_cycle, *name_field, *code_field, *platform_field, *desc_field, *format_cycle;
    Object *files_text, *pick_btn, *build_btn, *status;
    Object *preview_btn, *flatten_btn, *preview_list, *warn_text, *warn_row;
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
    code_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Code", TAG_DONE);
    platform_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "Platform, e.g. A1200", TAG_DONE);
    desc_field = MUI_NewObject((char *)"UP-Field.mcc", UPA_Field_Placeholder, (ULONG) "One-line description", TAG_DONE);
    format_cycle =
        MUI_NewObject((char *)"UP-Cycle.mcc", UPA_Cycle_Entries, (ULONG)formats, TAG_DONE);
    files_text = shell_label("(no files drawer picked)", UPV_Text_Tone_Muted);
    pick_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Pick...", TAG_DONE);
    build_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Build", UPA_Button_Role,
                              (ULONG)UPV_Button_Role_Accent, TAG_DONE);
    preview_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Preview", TAG_DONE);
    flatten_btn = MUI_NewObject((char *)"UP-Button.mcc", UPA_Button_Caption, (ULONG) "Remove extra nested folder", TAG_DONE);
    warn_text = shell_label("", UPV_Text_Tone_Normal);
    *(void **)&g_preview_hook.h_Entry = (void *)rp_preview_row_tramp;
    preview_list = MUI_NewObject((char *)"UP-List.mcc", UPA_List_RowHook, (ULONG)&g_preview_hook, UPA_List_NoSelect,
                                 (ULONG)TRUE, UPA_List_MaxRows, 8UL, TAG_DONE);
    warn_row = (warn_text && flatten_btn) ? shell_row(warn_text, flatten_btn) : NULL;
    status = shell_label(broken[0] ? broken : "Fill in the release, then press Build.", UPV_Text_Tone_Muted);
    if (group_cycle == NULL || name_field == NULL || code_field == NULL || platform_field == NULL ||
        desc_field == NULL || format_cycle == NULL || files_text == NULL || pick_btn == NULL ||
        build_btn == NULL || status == NULL || preview_btn == NULL || preview_list == NULL || warn_row == NULL) {
        shell_requester("The window objects could not be created.\nFree some memory and check the UP-MCC install, then start again.");
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    win = MUI_NewObject((char *)MUIC_Window, MUIA_Window_Title, (ULONG) "Release Packer", MUIA_Window_ID, 0x524C5350,
                        MUIA_Window_RootObject,
                        (ULONG)MUI_NewObject((char *)"UP-Panel.mcc", UPA_Panel_Role, (ULONG)UPV_Panel_Role_Panel,
                                             MUIA_Group_Child,
                                             (ULONG)MUI_NewObject((char *)MUIC_Group, MUIA_Group_Horiz, (ULONG)TRUE, MUIA_FillArea,
                                                                  (ULONG)FALSE, /* side by side: stacked, the cards outgrow an 800x600 screen */
                                             MUIA_Group_Child,
                                             (ULONG)shell_card("Release", shell_row(shell_label("Group", UPV_Text_Tone_Normal), group_cycle),
                                                              shell_row(shell_label("Name", UPV_Text_Tone_Normal), name_field),
                                                              shell_row(shell_label("Code", UPV_Text_Tone_Normal), code_field),
                                                              shell_row(shell_label("Platform", UPV_Text_Tone_Normal), platform_field),
                                                              NULL, NULL),
                                             MUIA_Group_Child,
                                             (ULONG)shell_card("Pack", shell_row(shell_label("Format", UPV_Text_Tone_Normal), format_cycle),
                                                              shell_row(shell_label("Description", UPV_Text_Tone_Normal), desc_field),
                                                              shell_row(shell_label("Files", UPV_Text_Tone_Normal), files_text),
                                                              shell_row(pick_btn, preview_btn), shell_row(build_btn, NULL), NULL),
                                             TAG_DONE),
                                             MUIA_Group_Child, (ULONG)shell_card("Archive preview", warn_row, preview_list, NULL, NULL, NULL, NULL),
                                             MUIA_Group_Child, (ULONG)status, TAG_DONE),
                        TAG_DONE);
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
    DoMethod(build_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_BUILD);
    DoMethod(pick_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_PICK);
    DoMethod(preview_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_PREVIEW);
    DoMethod(flatten_btn, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2UL, MUIM_Application_ReturnID, (ULONG)ID_FLATTEN);
    SetAttrs(warn_row, MUIA_ShowMe, FALSE, TAG_DONE);
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    GetAttr(MUIA_Window_Open, win, &is_open);
    if (!is_open) {
        shell_requester("The window did not open.\nThe screen or window could not be opened. Close other programs to free memory, then start again.");
        MUI_DisposeObject(app);
        CloseLibrary(MUIMasterBase);
        return 20;
    }
    for (;;) {
        ULONG id = DoMethod(app, MUIM_Application_NewInput, (ULONG)&sigs);
        if (id == (ULONG)MUIV_Application_ReturnID_Quit || (SetSignal(0, 0) & SIGBREAKF_CTRL_C) != 0) break;
        if (id == ID_BUILD) {
            shell_build(status, g_groups, group_cycle, name_field, code_field, platform_field, desc_field, format_cycle);
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
