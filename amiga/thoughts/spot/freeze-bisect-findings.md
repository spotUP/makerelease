# Freeze bisect (2026-11-23, rig 3)

Result: NO UI variant stops the freeze. The cause is not in the uncommitted ui_amiga.c changes, and not Cycle.
Each variant: fresh rig 3 disk, up-mcc 2.0 (c556119) + 12 MUI libs + themes + Inter + groups demoplace/testup installed, Run >NIL: <NIL: packer-cli, 10 s, ping => ConnectionRefused (frozen) in every case.

| Variant (built from working tree via src/ui_amiga.c edit, restored after) | Result |
| all four uncommitted UI suspects removed at once (Online card, Borderless attrs, AutoBars, UPA_Panel_Popup, filler Rectangle, Platform Cycle -> Field, X button) | FREEZE |
| HEAD a94a2ea ui_amiga.c unmodified (git show HEAD) | FREEZE |
| HEAD with both UP-Cycle.mcc creations replaced by UP-Field | FREEZE |

So: HEAD UI + current up-mcc (c556119, slim class libs + up-mcc.library 2.0) freezes; with no groups no MUI objects are built and the Amiga survives. The freeze is therefore in the UP-MCC side (or the window/MUI open with any of these classes), not in a single UI attribute. Single-removal variants (online, border, bars, popup) were built but not run, because the all-off result makes them moot.

Hypothesis: commit 13b4621/dc93e21/d3bcc7a (class libs moved behind up-mcc.library: draw, theme, log, fonts, icons via shims in lib/classlib_shims.c, lib/classlib_draw_shims.c; library version 2). A shim call from a class (UP_Log/UP_Draw*/UP_IconBody/UP_FontFiles) that never returns or spins when UpMccBase is unset or the vector table does not match (library built v2, class libs expect different LVO) would freeze at first MUIM_Setup/Draw, i.e. at window open, not in OM_NEW. Not verified.
Compare: old up-mcc that ran with HEAD UI = commit before d3bcc7a.

Next step (not done): bisect up-mcc, not the UI. Rebuild up-mcc at 1b65a7d (before d3bcc7a), d3bcc7a, dc93e21, 13b4621 and install those; or run scratch-rig/t_classes.c per class and a window-open test with only UP-Button. Also check the serial log (build/rig3/serial.log) and wasabid for the last output before the hang.
State left: rig 3 frozen (fs-uae busy), working tree ui_amiga.c restored byte-identical. Scratch binaries in the session scratchpad (~100 KB each).
