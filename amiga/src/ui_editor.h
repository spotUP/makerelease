#ifndef RP_UI_EDITOR_H
#define RP_UI_EDITOR_H

/* The group editor window (Amiga MUI shell). Needs the app object of the
 * main window and the main window (put to sleep while the editor is open).
 * dir is the group directory ("PROGDIR:groups"). Returns when the editor
 * closes; 1 when anything was saved, duplicated or deleted (the caller
 * reloads its groups), else 0. */
#ifdef __cplusplus
extern "C" {
#endif

/* Defined in ui_amiga.c, shared with the editor so both windows look the same. */
void shell_requester(const char *text);
Object *shell_label(const char *text, ULONG tone);
Object *row_label(const char *text);
Object *area_label(const char *text);
Object *shell_row(Object *a, Object *b);
void shell_show(Object *obj, int show); /* ShowMe, then repaint the nearest UP-Panel (see ui_amiga.c) */
Object *shell_form(Object *l1, Object *f1, Object *l2, Object *f2, Object *l3, Object *f3, Object *l4, Object *f4,
                   Object *l5, Object *f5, Object *l6, Object *f6, Object *l7, Object *f7, Object *l8, Object *f8,
                   Object *l9, Object *f9, Object *l10, Object *f10);
Object *shell_gap(ULONG w, ULONG h);
Object *top_page(Object *card);
Object *shell_card(const char *title, Object *a, Object *b, Object *c, Object *d, Object *e, Object *f, Object *g);
const char *shell_field_text(Object *field);
void shell_set_field(Object *field, const char *text);

int rp_editor_run(Object *app, Object *parent_win, const char *dir);

#ifdef __cplusplus
}
#endif

#endif
