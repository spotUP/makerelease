# A3 archive preview (2026-11-22)

- src/archive_preview.[ch]: port of previewArchive (entries + nested chain, case-insensitive sort, collapse of a folder named like the release).
- tests/test_archive_preview.c: the three TS spec cases; ctest 10/10 on host.
- ui_amiga.c: Preview button, UP-List (RowHook) "Archive preview", warning row (UP-Text + "Remove extra nested folder" button, MUIA_ShowMe toggled). Preview refreshes after Pick and after flatten. Cross build compiles clean.
- Unverified on the rig: UP-List creation with RowHook (hook entry is an asm trampoline moving a1 to the stack; symbol has a leading underscore), MUIA_ShowMe relayout, UP-List NoSelect/MaxRows, window height with the extra card on a 640x256 screen.
- Only one selected path exists in the Amiga UI (g_files), so nested warning handles the first nested entry only.
