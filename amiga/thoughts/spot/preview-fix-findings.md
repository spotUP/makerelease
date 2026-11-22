# Preview fix findings 2026-11-22

Changes (src/ui_amiga.c, tests/test_archive_preview.c):
1. shell_refresh_preview always calls rp_preview_archive; npaths is 0 when no drawer picked, so file_id.diz, <REL>/, <REL>/<REL>.NFO list.
2. UP-List height: aalist_askminmax derives DefHeight from the row count at creation (0 -> 1 row, ~29 px). Fix: create the list with UPA_List_Count 6 (hook returns "" past g_prev), set count 0 right after the window opens. MaxRows 8 stays.
Host: new npaths 0 check; ctest 10/10 pass. Amiga cross build OK.
Rig 3 (already running from the earlier opens run, so reused, not restarted; purpose text on it is the old one; rig was fresh, MCC set + groups demoplace/testup + packer-cli reinstalled): window opens 517x560 at 141,20 on 800x600 (usable ~573); list 405x134 at start; click Preview (561,218) with no drawer -> ui list value "3". Row text not in the ui tree (unverified visually). Window is 105 px taller than the old 455: does not fit a 640x256 screen (not required).
Rig stopped with --keep. Scratch left: scratchpad logs only (<1 MB).
