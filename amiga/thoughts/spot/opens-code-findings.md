# Opens code findings 2026-11-22
1. ui_amiga.c window-open requester no longer cites 640x256 (99bb5b3).
2. Removed the two startup printf debug traces in ui_amiga.c (c12ed35). main.c has no stdout use. Root cause of the Run redirect loss not found (libc stdout under Run with <NIL:); untested on a rig. Diagnostics now have no stdout path.
3. vendor/up-mcc configures and builds cross for 68k with cmake/amiga-68020.cmake, target up_mcc_dist (dist/UP-MCC/Libs/mui). Steps in README. Rig install not scripted (no script referenced MusicPlayer; only docs/handovers).
