# Ledger 2026-11-23: up-mcc class set + tab fix

Branch `more-classes` in amiga/vendor/up-mcc (no push). Merged: Checkmark, Radio, Popstring, Poplist, Numericbutton, Scale, Colorfield, Palette, Image, Menu (head c112cfd). None compiled for m68k or seen on a screen yet.

- [ ] Merge `cls-scr` (Scrollgroup 0x0E, Virtgroup 0x10) when agent `scr` reports; expect additive conflicts (base 1019060).
- [ ] m68k builds queued through buildq: scr, col, img, num. Fix compile errors per class. Test link failure (sel_mui_GfxBase / MUIMasterBase) is pre-existing, unchecked against base.
- [ ] go.sh (scratchpad): build up-mcc Button/Tabs/Panel + packer, install on rig 3, pixel check x=300,600 y100-200. Then ask user to test: tabs touch card, no dark block right of tabs, button faces without hover, width ~560.
- [ ] Remove app workarounds `top_page`, `VertSpacing 0` in ui_amiga.c if the tab fix works.
- [ ] Ask user: push more-classes to up-mcc and repin submodule (origin/main 0ac5741 has other fixes). Decide vendor/amissl-sdk.
- [ ] Update "eleven" counts in dist/ and demo/updemo.c; add demo entries for new classes.
- [ ] Adopt new classes in packer: Platform as Poplist, Checkmark/Radio where needed.
- [ ] Cleanup: worktrees /private/tmp/claude-501/upmcc-wt-{sel,pop,scr,num,col,img} and branches cls-*; build/rig3, build/upmcc, DH0:Packer/packer-cli2 on rig.
- [ ] A4 remaining: cover upload, Aminet FTP, live publish test (GET only). Truncated placeholders, Platform dropdown crash test, real groups, Code input spec.

## Ledger update 2026-11-23 (afternoon)
- [x] cls-scr merged into up-mcc more-classes (1e30285). 12 new classes on more-classes.
- [x] Tab strip fix: UPA_Text_FillHeight (843ed42). Rig 3 pixel check PASS (x=300/600 identical 141418 y121-154). User test on rig 3 still open.
- Rig 3 dir was deleted by something (build/rig-gc.log?); reprovisioned by agent. Runtime `assign LIBS: MUI:Libs ADD` needed (not in User-Startup). AmiSSL not installed on new disk.
- [ ] Agents running: all-classes m68k build+install+demo+counts (ad1eee...), host test link failure vs base (ab8548...).
- [ ] After: remove app workarounds (top_page, VertSpacing 0) in ui_amiga.c; Platform as Poplist/Cycle; commit outer repo (ui_amiga.c, CMakeLists, new online sources by name) + submodule pin; push question for up-mcc more-classes (upstream origin/main 0ac5741 has other fixes).
- [ ] A4 remaining: cover upload, Aminet FTP, live publish test (GET-safe), AmiSSL reinstall on rig 3.

## Ledger update 2026-11-23 (packer notes and settings)
- [x] Online layer (bfbbada), Aminet upload (fef049f, c39dd41, 7fc0ea8), code suggestion from release name (ce0e709). Unverified on screen.
- [x] Release notes: single-line "Notes" field in the Release tab (no multi-line UP class exists), fed to Build (both NFOs), Publish (releaseNotes) and the Aminet readme. Compiles, host tests pass; not seen on a screen.
- [x] Aminet Directory/Architecture/Uploader/Email persist in PROGDIR:aminet.prefs (rp_aminet_prefs_load/save in aminet.c, saved on the Aminet button, loaded at start; host test in test_aminet.c). No prior settings mechanism existed (groups are drawers only).
- [ ] Multi-line notes needs an UP-MCC multi-line text class; then swap the field.
