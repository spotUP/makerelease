# Opens rig findings 2026-11-22 (rig 3, packer-cli copy md5 d7d3825728a410768ce92eaae4985d0e, copied from build/amiga at start)

Rig: vtcon repo /Users/spot/Code/UP-Term/vtcon, `UPTERM_RIG=3 python3 tools/rig/rig.py start --max --purpose "amiga session: preview/Build/groups UI check"` (about 1 min). Wasabi port 1236.
UP-MCC (12 .mcc + up-mcc.library from MusicPlayer/build/amiga/plugins/shared/mcc) and DH0:Packer reinstalled.

## c) "No groups found" requester: PASS (requester shown); app exits after Ok
Empty DH0:Packer/groups. Requester "Release Packer": "No groups found. Put your group drawers in PROGDIR:groups, then start again." (OCR of grab; ui list shows only an Ok button).
After Ok: `wasabi ps packer` = 0 tasks, window gone. By design (ui_amiga.c:506-509 returns 20); Workbench/DOpus stay usable. If "app stays usable" meant the packer window, the code does not do that.

## a) Preview button alone: FAIL
Groups uploaded (demoplace, testup), Name "Rig Test", Code "rt1", Platform "A1200", no files drawer. Clicked Preview (button at 520,255, 82x30).
UP-List stays `value "0"`, no rows. Cause: shell_refresh_preview (ui_amiga.c:179-199) calls rp_preview_archive only when g_files[0] != '\0'; otherwise the list count is g_prev.nentries = 0. So file_id.diz, <REL>/, <REL>/<REL>.NFO never show without a Pick. rp_preview_archive always adds those 3 rows first (archive_preview.c:136-139), so calling it with npaths 0 when no drawer is picked would list them. Not fixed (report only). Row text with a drawer picked: not checked (rows are not in the ui tree).
Also: the list is 29 px high (about 2 rows visible) in the 517x455 window.

## b) Build: PASS
Status line: "Built PROGDIR:releases/DM-RT1.LHA". DH0:Packer/releases: DM-RT1.LHA 194 bytes, DM-RT1/{file_id.diz 34, DM-RT1/DM-RT1.NFO 69}. `lha v`: 2 files 103 bytes.
