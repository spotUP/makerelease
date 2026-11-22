---
date: 2026-11-22
topic: Editable group templates shipped, release zips rebuilt; ledger empty
tags: [profile-editor, templates, release, handoff]
status: done
---

# Handoff: editable group templates and release zips

## Task(s)
- Aminet: first live upload (OPMUI, 2026-11-20) confirmed approved and public at https://aminet.net/util/misc/up-opmui.lha. Upload path proven.
- Ledger item V (six manual group editor tests from `2026-10-17_profile-editor-validation.md`): spot reports all passed 2026-11-22 on the LOCAL dev build. That handoff is now `status: done`.
- spot asked during V: "for new groups can we load a default template they can edit?" Done in commit `884f88a` (pushed to origin/main). Hand-tested by spot ("Templates editable").
- Rebuilt installers: public and private (mac/win/linux) copied to `~/Desktop/makerelease-public` and `~/Desktop/makerelease-private`.
- Rebuilt the three `~/Desktop/Up_Rough_Release_Maker_-_{macOS,Windows,Linux}.zip` from the PRIVATE build (spot's choice). They contain the bundled groups and write key: do not share publicly.

## Critical References
- `thoughts/spot/todos.md`: every item checked; section "In-app group profile editor (2026-11-22)" records V and the template change.
- Template editor: `app/src/renderer/profile-editor.ts` (`renderTemplateText`, `readFields` maps an empty NFO box to `nfo: null`), `app/src/renderer/index.html` (`#pe-diz-text`, `#pe-nfo-text`), `app/src/renderer/style.css` (`textarea.template-text`, profile dialog widened to 820px).
- New group defaults: `newProfileDraft` in `app/src/main/ipc-handlers.ts` uses `EXAMPLE_NFO_TEMPLATE` / `EXAMPLE_DIZ_TEMPLATE` from `app/src/main/example-profile.ts`.

## Recent Changes
- `884f88a` Edit group templates in the group editor: DIZ/NFO text boxes (TopazPlus, no wrap); Import loads a file into the box; empty NFO = no NFO (replaces the Remove NFO button).
- Profiles backup taken before the tests: `thoughts/spot/backups/profiles-2026-11-22/` (git-ignored). Live profiles were identical to it afterwards; nothing restored.

## Learnings
- `app/release/` is shared by the public and private builds. The private build runs last and overwrites `release/mac`, `release/mac-arm64`, `linux-unpacked`, `win-unpacked` with apps that carry `Resources/private-profiles/`. To zip a public build, rebuild public alone first.
- The removed Remove NFO button had no test; the change in `readFields` (empty box to null) is untested by unit tests, only by hand.

## Artifacts
- Commit `884f88a` on origin/main.
- `~/Desktop/makerelease-public/`, `~/Desktop/makerelease-private/`, `~/Desktop/Up_Rough_Release_Maker_-_*.zip`.

## Next Steps
- Nothing open in the ledger. Packaged builds were not launched; if spot reports a problem with an installer, start there.
- Next work is whatever spot asks for.
