---
date: 2026-10-17
topic: In-app group profile editor
status: approved-scope
research: thoughts/shared/research/2026-10-17_profile-editor.md
---

# In-app group profile editor

## Goal
A group creates, edits, duplicates and deletes its profile entirely inside makerelease. No text editor. Templates (ASCII art drawn in PabloDraw/Moebius) come in through Import buttons. "Open profiles folder" and "Reload profiles" stay as advanced controls.

## Decisions (closed; spot chose "full group editor" 2026-10-17, the rest are implementation defaults)
1. **Single source of validation:** the editor never re-implements `parseProfile` rules. Main serialises the edited profile, runs `parseProfile(JSON.parse(text), id, file)` and refuses the write on error; the error string goes back to the dialog. The renderer only does field typing (number input for codeLength).
2. **Serialiser** `serializeProfile(profile: GroupProfile): string` in `app/src/shared/profile.ts`: fixed key order (label, url, filePrefix, codeLength, renderer, backend, server?, members), `JSON.stringify(obj, null, 2) + '\n'`, no `id` key (id is the folder name). `example-profile.ts` builds `EXAMPLE_PROFILE_JSON` through it (reuse, one format).
3. **Id = folder name, fixed after creation.** New/Duplicate: id field prefilled with a slug of the label (`slugifyProfileId(label)`: lower-case, non `[a-z0-9]` runs to `-`, trimmed, `isProfileId` checked, numeric suffix `-2`, `-3` on collision). Edit: id shown read-only. Renaming a folder = Duplicate then Delete.
4. **Delete** moves the folder to the system Trash (`shell.trashItem`), recoverable. Renderer confirms with `window.confirm` (existing pattern, main.ts:889). Refused when it is the only profile left (would trigger nothing useful: `ensureExampleProfile` only runs when the folder is absent).
5. **Writes are atomic and private:** write `profile.json.tmp`, `fs.rename` over `profile.json`, mode `0o600` (it can hold `server.writeKey`). Templates written with `writeLatin1`. Same for the example profile on first launch.
6. **Templates staged in the dialog.** Import returns the decoded text (latin1, `normaliseLineEndings`, same decode as `readTemplates`; extract `readTemplateFile(file, optional)` in profiles.ts and use it in both). Save sends `{ nfo: string | null, diz: string }`. NFO has a Remove button (NFO is optional for placeholder profiles). Template controls show only when renderer is `placeholder`; code renderers keep their files untouched (`templates` omitted from the save request).
7. **New group** starts as renderer `placeholder`, backend `none`, codeLength 5, filePrefix "", templates = `EXAMPLE_NFO_TEMPLATE` / `EXAMPLE_DIZ_TEMPLATE` so it builds immediately.
8. **Duplicate** copies all fields + templates (reads them via `readTemplates`), label gets " copy", id gets a fresh slug.
9. **Every profile.json field is in the form:** label, url, filePrefix, codeLength, renderer (select: placeholder / uprough / xinnovations), backend (select: none / releases-doorserver), server.baseUrl + server.writeKey (password input, Show toggle) shown only for doorserver, members (row editor: nickname, role, add, remove, move up/down; order preserved, blank-nickname rows dropped; note "The server's member list replaces this one" when backend is doorserver).
10. **Dialog** = native `<dialog id="profile-dialog">` with `showModal()`; Save / Cancel; error line uses `.status.warn`. Labels in full English words. No emojis. Styling uses existing tokens (`--bg --fg --accent --border --danger`); new colours become tokens on `:root`.
11. **After save/delete:** renderer calls the existing reload path (the Reload profiles handler), selects the saved id (localStorage `profile-id`), and the existing `init()` runs `resetPublishState()`; a deleted selected profile falls back to the first entry.
12. **Hint text** (index.html:15) becomes: "Which group this release goes out under. Use New, Edit, Duplicate or Delete to manage groups and their templates."

## Phase 1 — main + shared (no UI)
Files:
- `app/src/shared/profile.ts`: `serializeProfile`, `slugifyProfileId(label, taken: string[])`. Types `ProfileEditRequest { id: string; isNew: boolean; profile: Omit<GroupProfile,'id'>; templates?: { nfo: string | null; diz: string } }` and `ProfileTemplatesText` in shared types.
- `app/src/main/profiles.ts`: `readTemplateFile` extraction; `saveProfile(profilesDir, req)` (isNew + existing folder -> error "A group with id X already exists"; !isNew + missing folder -> error; validate via parseProfile; atomic 0600 write; templates: write diz, write or `fs.rm({force:true})` nfo); `deleteProfile(profilesDir, id, trash: (p) => Promise<void>)` (isProfileId check, refuses last profile).
- `app/src/main/example-profile.ts` + `ensureExampleProfile`: use serializeProfile, 0600.
- IPC (`ipc-handlers.ts`, `preload.ts`, `MakereleaseApi` in `shared/types.ts`): `loadProfileForEdit(id) -> { profile: GroupProfile; templates: {nfo, diz} }`, `saveProfile(req) -> { ok: true } | { ok: false; error: string }`, `deleteProfile(id) -> same`, `importTemplate() -> { text: string; name: string } | null` (`dialog.showOpenDialog`, filters NFO/DIZ/TXT/all, pattern of pickImage at ipc-handlers.ts:163), `newProfileDraft() -> {profile, templates}` (decision 7, id slug "new-group").
- Mock factory in `test/unit/ipc-handlers.spec.ts:34-45` gets the new exports.

Tests (`app/test/unit/`):
- `profile.spec.ts`: serializeProfile round-trips through parseProfile for example, uprough-shaped (server, members) and minimal profiles; key order fixed; slugify cases (spaces, punctuation, empty -> "group", collision suffix).
- `profiles.spec.ts`: saveProfile new/existing/collision/missing; invalid profile (doorserver + placeholder) rejected and nothing written; file mode 0600; nfo null removes release.nfo; templates omitted leaves files byte-identical; deleteProfile calls trash once, refuses last profile, refuses bad id.
- `ipc-handlers.spec.ts`: each new channel delegates and maps errors to `{ok:false,error}`.
Success: `cd app && npm run typecheck && npm test` green.

## Phase 2 — renderer
Files: `app/src/renderer/index.html` (buttons New / Edit / Duplicate / Delete next to the select; `<dialog>` markup; hint text), `app/src/renderer/profile-editor.ts` (new module: dialog state, form <-> profile mapping, member rows, template import/remove, Save/Cancel; pure `formToRequest`/`profileToForm` functions exported for tests), `app/src/renderer/main.ts` (wire buttons, reuse the reload path, select saved id), `app/src/renderer/style.css` (dialog, member rows, tokens).
Tests: `profile-editor.spec.ts` for `formToRequest`/`profileToForm` (order kept, blank rows dropped, server only for doorserver, codeLength number, templates omitted for code renderers).
Success: typecheck + tests green; `npm run build` green.

## Phase 3 — reachability (one test)
`app/test/unit/reach-profile-editor.spec.ts`: same entry setup as `reach-profiles.spec.ts` (real temp userData, the two startup calls of `index.ts`, real handlers). Drives `newProfileDraft` -> `saveProfile` with label `SENTINEL Group`, filePrefix `SENTINEL-`, an imported DIZ text containing `SENTINEL-DIZ` -> `listProfiles` contains it -> `loadSeedState('sentinel-group')` returns that prefix -> `previewOutputs` DIZ contains `SENTINEL-DIZ` -> `deleteProfile` calls the trash spy exactly once with the folder path -> `listProfiles` no longer contains it.

## Automated verification
- `cd app && npm run typecheck && npm test && npm run build`
- `grep -rn "profile.json" app/src/renderer/index.html` returns only the advanced-controls context (hint no longer tells users to edit JSON).

## Manual verification (spot)
- New: make a group, import a DIZ and NFO, save; dropdown selects it; preview shows the art; build works.
- Edit Up Rough: change nothing, save; `git diff --no-index` of profile.json before/after shows only key order/format (back it up first).
- Doorserver fields show only for releases-doorserver; write key hidden until Show.
- Duplicate then Delete: folder lands in Trash; deleting the last group is refused.
- Invalid combo (placeholder + releases-doorserver): Save shows the parseProfile error, nothing written.

## Implement deviations
- `app/src/shared/profile.ts` now imports `./divisions.js` (was `./divisions`): the renderer imports `slugifyProfileId` from it for the live id suggestion and Duplicate, and the browser's ES module loader needs the extension. TypeScript for the main process resolves it unchanged.
- `loadProfileForEdit` returns `{ nfo: null, diz: '' }` for code-renderer profiles (they keep no template files in the folder); `newProfileDraft` takes no argument and slugs "new group" against the existing folders.
- `ProfileEditOutcome`, `ProfileEditState` added to shared types beside `ProfileEditRequest` and `ProfileTemplatesText`.
- The reachability test's trash spy removes the folder, so the final `listProfiles` assertion is real.
- Phase 2 and 3 verification of the dialog itself is by form-mapping unit tests only; no browser run (manual verification stays with spot).
