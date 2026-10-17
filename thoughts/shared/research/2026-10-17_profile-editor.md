---
date: 2026-10-17
topic: Existing code touched by the in-app group profile editor
status: complete
---

Paths relative to app/. Line refs from the tree at aa7ff9f.

## 1. Schema and validation (src/shared/profile.ts)
- GroupProfile :17-35: id (folder name, [a-z0-9-]+), label (req, non-empty), url (req, non-empty), filePrefix (opt, default ""), codeLength (opt, default 5), renderer, backend (default 'none'), server? (ServerConfig), members (default []).
- ServerConfig :11-14: baseUrl, writeKey. MemberEntry (src/shared/types.ts:4-7): nickname, role (both strings).
- RendererKind = DivisionId | 'placeholder' :9; values 'uprough'|'xinnovations'|'placeholder' (RENDERERS :56). BackendKind 'none'|'releases-doorserver' (:12, :57).
- ProfileSummary :38-47 (id,label,url,filePrefix,codeLength,renderer,backend; NO server/members). ProfileListEntry :50 = summary | {id,error}.
- Constants: TEMPLATE_FILES {nfo:'release.nfo', diz:'file_id.diz'} :52, PROFILE_FILE 'profile.json' :54, isProfileId :60, summarize :64.
- parseProfile(raw, id, file) :89-175. Error format: `${file}: "${field}" <problem>`. Rules:
  - id bad: `${file}: the folder name "${id}" must use only a-z, 0-9 and "-"` (:93)
  - not object: `must be a JSON object`
  - label/url: `is missing` / `must be a string` / `must not be empty` (trim)
  - filePrefix: must not match /[\\/:*?"<>|\s]/ -> `must not contain spaces or any of \ / : * ? " < > |`
  - codeLength: integer 1..32 -> `must be a whole number from 1 to 32`
  - renderer: one of the three, else `must be one of "uprough", "xinnovations", "placeholder"`; backend likewise.
  - backend 'releases-doorserver': renderer must NOT be 'placeholder' (`can be "releases-doorserver" only with renderer "uprough" or "xinnovations"`); server object required; server.baseUrl non-empty string (trailing slashes stripped, :143); server.writeKey string, default "" (empty allowed; configWarning at runtime, backend-doorserver.ts:~57).
  - backend not doorserver and server present: `"server" is allowed only when backend is "releases-doorserver"`.
  - members: list of {nickname:string, role:string}; no empty/dupe check at parse. Output keeps only those two keys.
  - Unknown extra keys are silently ignored (dropped on round trip).
- NO serialiser exists. Only precedent: example-profile.ts:9-21 `JSON.stringify(obj, null, 2) + '\n'` with key order label,url,filePrefix,codeLength,renderer,backend,members. Editor needs a new GroupProfile->JSON (omit `id`, it is the folder name; omit `server` unless doorserver).
- No client-side id uniqueness/validation beyond isProfileId (exported, usable in renderer: shared module).

## 2. Storage (src/main/profiles.ts, example-profile.ts)
- Layout `<userData>/profiles/<id>/{profile.json, file_id.diz, release.nfo?}` (index.ts:47 `path.join(app.getPath('userData'),'profiles')`; RULES.md:43).
- loadProfile :26-46: isProfileId check before any fs access; readFile utf8; strips leading BOM (:39) before JSON.parse; wraps errors with file path.
- listProfiles :53-72: readdir, directories not starting with ".", sorted by id; broken profiles returned as {id,error}; missing profilesDir returns [].
- readTemplates :79-94: reads with fs.readFile -> latin1, normaliseLineEndings (CRLF/CR -> LF, shared/placeholder-template.ts:120). release.nfo optional (ENOENT -> null), file_id.diz REQUIRED (throws "could not be read"). No BOM stripping for templates (a UTF-8 BOM would appear as 3 latin1 chars). Only used when renderer === 'placeholder' (ipc-handlers.ts:131, :153); code renderers ignore templates.
- ensureExampleProfile :100-104: only when profilesDir does not exist; writes profile.json utf8, NFO/DIZ via writeLatin1 (encoding.ts:46, which calls assertLatin1Safe :30 and rejects chars > U+00FF). Reuse writeLatin1 for imports/saves of templates.
- File modes: NONE set anywhere (grep for 0o6/chmod across src and docs: no hits). writeKey is plaintext in profile.json at default umask (RULES.md:48 says it lives only in userData). Editor must decide on 0600 for profile.json when it holds a key (new behaviour).
- fs helpers (main/fs-helpers.ts): pathExists, ensureDir, removePath (rm -r force), moveDir, copyRecursive (fs.cp recursive, usable for Duplicate), isDirectory. No atomic-write helper.
- Build output is keyed on profile (outputRootFor) so deleting a profile does not touch releases; profile deletion = removePath(profilesDir/id) after isProfileId check.

## 3. IPC surface
- preload/preload.ts:15-39 builds `api: MakereleaseApi` via ipcRenderer.invoke(channel,...args); MakereleaseApi interface at shared/types.ts:226-245. Profile channels: listProfiles (no args -> ProfileListEntry[]), openProfilesFolder, loadSeedState(profileId). Others: pickFilesAndDirs, pickImage, checkCodeAvailability, buildRelease, previewOutputs, publishToWeb, uploadToAminet, deleteRelease. A new channel needs: ipc-handlers.ts, preload.ts, MakereleaseApi.
- Handlers in main/ipc-handlers.ts, `registerIpcHandlers(profilesDir, outputRootFor)` (:~70). listProfiles :76-79 maps via summarize (editor needs full GroupProfile incl. server/members: a new `getProfile`/`readProfile` channel, since the summary omits them). openProfilesFolder :81-86 (ensureDir then shell.openPath, throws on problem). loadSeedState :88-92 loads from disk every call (comment :66-70: no caching).
- Existing dialog.showOpenDialog usages: pickFilesAndDirs :94-100 (returns string[] or []), pickImage :163-169 (filters Images, returns string|null). Pattern to copy for a template picker (filters name 'Templates', extensions ['nfo','diz','txt'] etc; main reads the file, renderer never gets a path).
- Tests:
  - test/unit/ipc-handlers.spec.ts: vi.mock('electron'), mocks '../../src/main/profiles' (loadProfile built from PROFILE_JSON fixtures through real parseProfile; listProfiles vi.fn), mocks fs-helpers, releases-client etc; calls `registerIpcHandlers('/profiles', outputRootFor)` and invokes captured handlers. Any new profiles.ts export used by handlers must be added to the mock factory (:34-45) or the suite breaks.
  - test/unit/reach-profiles.spec.ts (54 lines): electron mocked as {ipcMain.handle -> handlers map, dialog.showOpenDialog, shell.openPath}; REAL fs in mkdtemp userData; calls ensureExampleProfile + registerIpcHandlers (same two calls index.ts makes), then `handlers.listProfiles(null)` etc. Proof of run = sentinel value only present on disk (filePrefix 'SENTINEL-') seen via loadSeedState and listProfiles. A save-profile reach test follows the same shape: handler save -> assert bytes on disk -> listProfiles/loadSeedState reflect it.
  - Also test/unit/profile.spec.ts, profiles.spec.ts (parse and disk unit tests).

## 4. Renderer
- index.html:15-19: hint paragraph, `<select id="profile-select">`, buttons #open-profiles-btn "Open profiles folder", #reload-profiles-btn "Reload profiles", `<div id="profile-status" class="status">`.
- main.ts: elements :28-31; PROFILE_STORAGE_KEY 'profile-id' localStorage :411-419 (try/catch); loadProfiles :421-456 (listProfiles, usable = non-error; builds options by label; broken ones shown in profile-status with class "status warn" as `Profile "<id>" not loaded: <error>`; empty state text 'No group profiles yet. Open the profiles folder and add one.'; selects remembered id else first). change handler :458-465 stores id then `void init()`. open button :467-474. reload button :476 -> loadProfiles() (does NOT call init explicitly: it relies on loadProfiles' tail, verify at :450-452 when reusing).
- init() :534-~575: calls resetPublishState() (:~537, built-release reset), `const isLatest = initSwitches.begin()` (LatestOnly, renderer/latest-only.ts:6-12), awaits loadSeedState(profileSelect.value), `if (!isLatest()) return` before applying seed (applyProfile :478, memberlist.seed, boards, etc).
- built-release.ts:19-23 builtReleaseFor(built, currentProfileId): null unless built.profileId matches. After an edit/delete of the active profile the build record must be dropped (resetPublishState does that on init; `built` itself reset at the same place) and a Delete of the active profile must fall back to another id.
- Modal/dialog pattern: NONE in the renderer (no <dialog>, no modal CSS). Only `window.confirm` (main.ts:889, delete release). No prompt(). Inline sections with `hidden` attribute / `[hidden]` CSS (#publish-section[hidden] style.css:140ish) and dynamic tables (memberlist-table.ts, boards-table.ts, credits-table.ts, media-embeds-table.ts each a class with seed/addRow/getX). The editor could be a new section or <dialog>; MemberlistTable (renderer/memberlist-table.ts:3-52) is directly reusable for the members field BUT getMembers() trims, drops empty nicknames and SORTS by nickname (localeCompare), which would reorder the profile's members on save.
- Renderer builds are bundled from renderer/main.ts; renderer imports shared/profile.ts types (type-only import at main.ts:2); isProfileId is a value export safe for the renderer.

## 5. Styling (renderer/style.css, 149 lines)
- Tokens :8-14 only: --bg #1a1a1a, --fg #e0e0e0, --accent #d98c3f, --border #3a3a3a, --danger #c0392b. Many hardcoded literals remain (#111 input bg, #9a9a9a hint, #4caf50 ok, #333) - no tokens for them.
- Elements: label block 0.85rem; input[text|email], textarea, select 100% width, #111 bg; textarea.monospace; table styles; `.hint` (0.8rem grey); `button` accent bg with margin-top .5rem; `button.danger`; `.status`, `.status.warn` (danger colour), `.status.ok` (green); `.aminet-grid` 2-column grid; `.media-embed-row` flex row. Body max-width 720px. Font TopazPlus only used for DIZ banner preview.
- Buttons in the profile row are plain `button` (no class).

## 6. Members constraints
- MemberEntry {nickname, role} strings. Profile.members is only the seed for backend 'none' (backend-none.ts:11-16 returns profile.members); with 'releases-doorserver' the server's seed.members win (backend-doorserver.ts:49) and profile.members is ignored.
- Renderers: nfo-x-innovations.ts rosterBlock :96 groups by role.trim() in first-seen order, prints spaced-caps role label and nicknames separated by " . " wrapped at FIELD_WIDTH-1; empty role becomes a group with empty label. nfo-generator.ts renderMemberBlock :30 pads nickname to max(MEMBER_LABEL_MIN_WIDTH, len+2) then role. Placeholder renderer {{members}} uses the same list. No validation of length/duplicates anywhere; later NFO output goes through Latin-1 safety (assertLatin1Safe), so nicknames > U+00FF would fail at build. Renderer UI: creators autocomplete reads memberlist (main.ts:100, only when visibility.creatorsAutocomplete).

## 7. Caches
- None. grep for cache/memo in src: no profile caches. Main loads from disk on every handler call (ipc-handlers.ts:66-70). Only renderer state: `currentProfile` (main.ts:122, ProfileSummary set by applyProfile), the profile dropdown options, localStorage 'profile-id', `built` record (built-release.ts), form fields seeded by init(). After save: call loadProfiles() and, if the edited id is selected, init() (a plain loadProfiles re-selects the same id but verify whether it triggers init: change event is not fired by setting .value programmatically). Template preview is scheduled via scheduleTemplatePreview() (main.ts, called in init).

## Gaps the editor must fill (no existing code)
serialiser; getProfile channel returning full GroupProfile; save/delete/duplicate channels with id validation and no-overwrite check; template import (picker + read + Latin-1 check + write); template read-for-edit; file mode policy for writeKey; modal/section UI; atomic write; handling of unknown JSON keys.
