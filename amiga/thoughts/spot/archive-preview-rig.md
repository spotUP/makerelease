# Archive preview rig check, 2026-11-22 (commit 6445078, packer-cli md5 b44b7cd919dc7d9c85ead870dec51d80)

Rig 2 (--purpose "makerelease amiga: archive preview rig check"), MUI:Libs/mui UP-MCC installed
from MusicPlayer/build/amiga/plugins/shared/mcc (22 Nov 16:32; first attempt used rig-saved 21 Nov set,
same result). Workbench screen 800x600. groups/files/nest/outer/inner/a.txt created under DH0:Packer.

## Result: FAIL at check 1
`Run >NIL: <NIL: DH0:Packer/packer-cli` -> requester "Release Packer" (338x88, only an Ok button),
text via tesseract on a grab of the requester: "The window did not open. Use a screen of at least
640x256 pixels, then start again." That is ui_amiga.c:597, MUIA_Window_Open readback false after
SetAttrs(win, MUIA_Window_Open, TRUE). The app and all objects were created (no "objects could not
be created" requester), so UP-List with RowHook/NoSelect/MaxRows was created without freeze; the
window refuses to open on an 800x600 screen, so it is not screen size. packer-cli stays alive
(ps: wait, stack free 38798 of 49152) until the requester is dismissed.
Checks 2-5 not reachable. Snoop showed only "events lost", no failing DOS call.
Not bisected (needs a source edit): suspect window content minimum size from the new card
(UP-List MaxRows 8 / hidden warn_row) or UP-List minimum size in the installed mcc.

## Update 2026-11-22: window-open FAIL fixed (rig 2, md5 of fixed packer-cli not recorded; built from working tree)
Cause: window height, not any single widget. Bisect: HEAD minus the "Archive preview" card opens at 289x570 on the
800x600 screen (usable ~573 px), so the stacked Release + Pack cards already fill the screen; adding the card
exceeds it and MUIA_Window_Open stays false. Fix (ui_amiga.c): Release and Pack cards side by side in a horizontal
group; window now 517x455 (493 with the warning row). UP-List with RowHook/NoSelect/MaxRows, hidden warn_row
(MUIA_ShowMe) and the Preview button were all fine.
Rig text evidence (wasabi ui list): Pick... opens ASL; drawer DH0:Packer/files/nest -> Files label set, warning
"Extra nested folder: DH0:Packer/files/nest only wraps inner" shown, UP-List value 7; click "Remove extra nested
folder" -> Files label DH0:Packer/files/nest/outer/inner, list value 5, window grows to 493 px. Not checked:
list row text (not in ui tree), the Preview button itself, Build. Side note: "No groups found" requester appeared
once on the first launch (groups uploaded flat per file; unverified cause).
