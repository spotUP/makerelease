# Small-screen fit findings 2026-11-22

Problem: window 517x560 (commit 3470b94) cannot fit a 640x256 PAL screen. Even the old 455 px cannot: two stacked cards (Pack card ~6 rows) plus the preview card (6 list rows = 134 px) plus status exceed the ~245 px usable height, so shrinking alone is impossible without dropping the 6-row list.

Fix (src/ui_amiga.c): the three content blocks (Release/Pack cards side by side, Preview card, status) now sit in a stock MUI Scrollgroup (MUIA_Scrollgroup_FreeHoriz FALSE) with a Virtgroup as contents. UP-MCC has no scroll class; this is the stock MUI pattern. The window keeps its natural content size (517x560 on a tall screen) and MUI clamps it to the screen; on 640x256 the contents scroll vertically. Width 517 fits 640. The UP-List creation with UPA_List_Count 6 (list not one row high) and the no-drawer preview are untouched.

New window height: no fixed number; min(560, screen height) derived from MUI window clamping, so about 245 px on non-interlaced PAL (256 minus title bar).
Verification: Amiga cross build OK. UNVERIFIED on screen (no rig allowed): that the window clamps to the screen height and the scrollbar shows, and that UP-List inside a Virtgroup still gets its 6-row height. Test on a 640x256 screen.
