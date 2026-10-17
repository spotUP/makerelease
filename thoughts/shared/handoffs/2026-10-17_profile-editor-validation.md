---
date: 2026-10-17
topic: In-app group profile editor, manual validation pending
tags: [profile-editor, validation, handoff]
status: draft
---

# Handoff: profile editor manual validation

## Task(s)
- Ledger item V in `thoughts/spot/todos.md`, section "In-app group profile editor": six manual tests of the in-app group editor. NOT started; spot deferred testing on 2026-10-17.
- Everything else in that section (R, P, I, M) is done and merged: origin/main 7ecdf05, later commits up to ccc764d.

## Critical References
- Plan: `thoughts/shared/plans/2026-10-17-profile-editor.md` (manual test list at lines ~55-58).
- Validation: `app/src/main/profiles.ts` (`saveProfile` ~line 115, `deleteProfile` ~line 145, last-group refusal line 151).
- Combination rule: `app/src/shared/profile.ts:136-138`.
- Profiles folder: `~/Library/Application Support/makerelease/profiles/` (currently `uprough`, `xinnovations`).

## Recent Changes
- None to code this session. No known failures, so nothing to fix yet.
- Backup of the whole profiles folder (taken 2026-10-17, before any test): `thoughts/spot/backups/profiles-2026-10-17/` (git-ignored). Verified identical to the live folder at time of writing.
- Dev app (`cd app && npm start`, sandbox disabled) built and launched with no errors or warnings in the log.

## Learnings
- No `profilesDir` override exists (`app/src/main/index.ts:47` uses `app.getPath('userData')/profiles`), so manual tests touch the real profiles. Hence the full-folder backup.
- Test 6 (delete last group) requires deleting real groups down to one. Deletes go to the macOS Trash (`shell.trashItem`); restore from the backup afterwards anyway.
- Sample templates for the New test: `app/test/fixtures/EX-TEST1.NFO` and `app/test/fixtures/EX-TEST1.DIZ`.

## Artifacts
- Research: `thoughts/shared/research/2026-10-17_profile-editor.md`
- Plan: `thoughts/shared/plans/2026-10-17-profile-editor.md`
- Backup: `thoughts/spot/backups/profiles-2026-10-17/`

## Next Steps
1. Launch: `cd app && npm start` (sandbox disabled).
2. spot runs the six tests; expected results:
   1. New `testgroup`, import EX-TEST1.NFO as release.nfo and EX-TEST1.DIZ as file_id.diz, Save: appears in dropdown with the imported templates.
   2. Edit Up Rough, change one harmless field, Save: change survives reload.
   3. Edit `testgroup`, renderer placeholder + backend releases-doorserver, Save: error `can be "releases-doorserver" only with renderer "uprough" or "xinnovations"`, nothing written.
   4. Duplicate `testgroup` as `testgroup-copy`: same fields and templates.
   5. Delete `testgroup-copy`: folder lands in macOS Trash.
   6. Delete down to one group, try deleting it: refused with `The last group cannot be deleted`.
3. Record each result in ledger item V.
4. Any failure: load skill fixing-bugs, fix root cause with a regression test.
5. Restore profiles from `thoughts/spot/backups/profiles-2026-10-17/` (show diff first, ask before overwriting), then delete `testgroup` leftovers.

## Other Notes
- Aminet upload is still untested live (see memory); unrelated to this task.
