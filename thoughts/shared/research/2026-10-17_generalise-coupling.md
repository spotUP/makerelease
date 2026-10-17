---
date: 2026-10-17
topic: Coupling of the release maker to Up Rough and to releases-doorserver (inventory for generalisation)
tags: [research, makerelease, coupling, releases-client, divisions, nfo, aminet]
status: final
---

# Release maker: coupling inventory

Scope: /Users/spot/Code/UpRough/makerelease/app (src, scripts, test). Ignored: node_modules, dist, config.generated.ts. Paths below are relative to app/. "Unconfirmed" marks anything not read directly (the server source `releases-doorserver` was NOT read; server behaviour is inferred from client code and comments).

## 1. Data flow: form -> IPC -> build -> publish

### 1.1 Form (renderer)
- Form markup: src/renderer/index.html (`#release-form`, line 12). Script: src/renderer/main.ts (705 lines).
- Startup `init()` (main.ts:370-400) calls `window.api.loadSeedState(currentDivision)` (main.ts:375). Result seeds boards (383), members (384), `nextReleaseNumber` (385), DIZ banner (386), warnings (390).
- Changing the division radio (`input[name="division"]`, main.ts:365-371) reloads the seed via `init()`.
- Code field (`#code`, maxlength 5): after 5 chars a 300 ms timer calls `checkCodeAvailability(code)` (main.ts:420-435); text is hardcoded "UP-<CODE> already exists on disk" (main.ts:432) although the answer comes from the SERVER (see 2).
- Submit builds `formData: ReleaseFormData` (main.ts:486-517) and calls `window.api.buildRelease(formData, dryRun)` (main.ts:524). On success it stores `lastBuiltCode`, `lastBuiltArchivePath`, `lastEmbedImagePaths` (main.ts:545-547), shows the publish section.
- Publish button -> `window.api.publishToWeb(code, archivePath, imagePath, embedImagePaths, aminetRequest())` (main.ts:591-596).
- Revoke button -> `window.api.deleteRelease(code)` (main.ts:662).

### 1.2 IPC channels
Bridge: src/preload/preload.ts:13-32 (`contextBridge.exposeInMainWorld('api', ...)`). Interface `MakereleaseApi`: src/shared/types.ts:206-220. Handlers: src/main/ipc-handlers.ts (`registerIpcHandlers`, line 71). Registered in src/main/index.ts:60.

| Channel | preload | handler | Args -> return (types.ts) |
|---|---|---|---|
| loadSeedState | preload.ts:14 | ipc-handlers.ts:72 | `DivisionId` -> `SeedState` (types.ts:73) |
| pickFilesAndDirs | preload.ts:15 | ipc-handlers.ts:104 | none -> `string[]` (native dialog) |
| checkCodeAvailability | preload.ts:16 | ipc-handlers.ts:112 | `string` -> `{exists:boolean}` |
| buildRelease | preload.ts:18 | ipc-handlers.ts:116 | `ReleaseFormData` (types.ts:33), `dryRun:boolean` -> `BuildResult` (types.ts:114) |
| pickImage | preload.ts:20 | ipc-handlers.ts:190 | none -> `string\|null` (png/jpg/jpeg/gif) |
| publishToWeb | preload.ts:21 | ipc-handlers.ts:199 | `code, archivePath, imagePath, embedImagePaths[], AminetUploadRequest\|null` -> `PublishToWebOutcome` (types.ts:174) |
| deleteRelease | preload.ts:29 | ipc-handlers.ts:237 | `code` -> `DeleteReleaseOutcome` (types.ts:202) |

Modeled failures are returned as `{ok:false,...}` not thrown (ipc-handlers.ts:218-232, 251-256); `buildRelease` and `loadSeedState` and `checkCodeAvailability` DO throw (rejections reach the renderer try/catch).

### 1.3 Build (`buildRelease`, ipc-handlers.ts:116-188), in order
1. `outputRoot` = `makereleaseRoot` (real) or `makereleaseRoot/dry-run` (`DRY_RUN_DIR`, line 69). `makereleaseRoot` comes from index.ts:16-19: dev = 4 levels up from `__dirname` (repo root, which is why `UP-FB006`, `UP-HYPOC` sit in the repo root); packaged = `resolvePackagedOutputRoot` (output-root.ts:17) -> `<documents|home|userData>/Up Rough Releases`.
2. Dry run: delete previous `UP-<CODE>` dir and `UP-<CODE>.<fmt>` in dry-run dir (123-124). Real: `assertLocalCodeAvailable` (42-57) refuses if `UP-<CODE>`, `.LHA` or `.ZIP` exists in root.
3. Numbering/history:
   - Dry run: `fetchSeed(division)`; `releaseNumber = seed.nextReleaseNumber`; synthesises a history entry with filenames `up-<code>.<fmt>` (136-148).
   - Real: `publishRelease(cfg, form)` = POST /api/releases (150); server assigns `releaseNumber`, returns `history`.
4. Division 'xinnovations' only: `fetchManifest(division)` + `listArtFromBanner` builds `releaseList` (158-167).
5. `generateOutputs(history, releaseNumber, form, releaseList)` (nfo-generator.ts:134): DIZ always `renderDiz` (toAsciiSafe + trim); NFO: `renderXInnovationsNfo` if `form.division==='xinnovations'` (141) else `renderNfo` (62-108, built from constants.ts strings and form text; history block from server rows).
6. `stageRelease` (staging.ts:8): mkdtemp `makerelease-*` in OS tmp; `UP-<CODE>/file_id.diz`, `UP-<CODE>/UP-<CODE>/UP-<CODE>.NFO`; both written Latin-1 via `writeLatin1` (encoding.ts:46, throws on codepoint > 0xFF); copies `form.selectedPaths` into the inner dir; a selected folder named `UP-<CODE>` is flattened (staging.ts:23).
7. `packArchive` (archiver.ts:16): shells out to `lha a` or `zip -r` with cwd=outer dir, archive `<root>/UP-<CODE>.LHA|ZIP`, members `file_id.diz` + `UP-<CODE>`. `finalizeRelease` (33) moves the staged dir to `<root>/UP-<CODE>`.
8. If not dry run and `divisionOf(division).submitsToDoorRepo` (true only for xinnovations): `submitDoor(archivePath)` (door-repo-client.ts:23) -> result attached to `BuildResult.doorSubmission`; failure never throws (ipc-handlers.ts:179-180).
9. Returns `{archivePath, releaseNumber, isDryRun, doorSubmission?}`.

### 1.4 Publish (`publishToWeb`, ipc-handlers.ts:199-235)
1. `publishToWebClient` -> multipart POST /api/releases/:code/publish with `archive`, `image` (cover), `embedImage`* (releases-client.ts:179-215).
2. Only if that succeeded and an Aminet request exists: `uploadToAminet` (aminet.ts:48): normalises readme, validates (`aminetFileNameProblems`, `aminetReadmeProblems` from shared/aminet-readme.ts), writes `<stem>.readme` next to the archive (Latin-1), then two `curl --upload-file` calls to `ftp://main.aminet.net/new/` as `anonymous:<email>` (archive first, readme second). Never throws.
3. Renderer shows `wordpressPostUrl` + `archiveUrl` + Aminet note (main.ts:599-603).

## 2. Server coupling

Base URL: `config.ts:26` `DEFAULT_BASE_URL = 'https://releases.uprough.net'`; override env `RELEASES_API_BASE_URL` (config.ts:44). Comments in config.ts:18-23 say builds already distributed are pinned to the www.uprough.net origin, which must keep working. All client paths are `/api/releases/...` (releases-client.ts appends them). Error strings hardcode "releases.uprough.net" at releases-client.ts:76, 81, 151, 155, 168, 172, 203, 208.

### 2.1 releases-client.ts calls
| Function | Method + endpoint | Auth | Request | Response (type) | What the app does |
|---|---|---|---|---|---|
| `fetchSeed` (87) | GET `/api/releases/seed?division=<id>` | none | query only | `SeedResponse` (41): members, boards?, nextReleaseNumber, history[], currentDizText\|null, revision, drafts? | Seeds the form; `describeStaleDrafts(drafts)` -> warning; dry run reuses `nextReleaseNumber` + `history`. `revision` is passed through to SeedState but no consumer found in src (grep `revision`: types.ts:80, ipc-handlers.ts:91, releases-client.ts:48,55 only). |
| `fetchNumberingCheck` (228) | GET `/api/releases/numbering-check` | none | - | `{checked, ok?, summary?, error?}` | Run in parallel with seed; failure swallowed (`.catch(()=>null)`, ipc-handlers.ts:78); if `checked && ok===false` -> `numberingWarning` text ("Do not build until reconciled"). Compares catalogue vs WordPress site www.uprough.net (comment 225). |
| `fetchManifest` (106) | GET `/api/releases/manifest?division=<id>` | none | - | `{history: ManifestEntry[]}` (releaseNumber string, filenames, title, dizText) | /X only: each previous release's DIZ art becomes its own banner in the NFO. |
| `checkCodeExists` (116) | GET `/api/releases/:code/exists` | none | - | `{exists}` | IPC `checkCodeAvailability`; renderer warns while typing. |
| `publishRelease` (142) | POST `/api/releases` | header `X-Release-Key` | JSON from `toPublishBody` (121-140): code, format, releaseName, platformTag, releaseNotes, news, dizText (trimmed), entryTitle, entryCategory, entryCreators, division, boards?, members, description, credits, mediaEmbeds | `PublishResponse`: releaseNumber, revision, history[] | Called by real build. Per comments (ipc-handlers.ts:33-36) it "atomically assigns a number AND replaces the entire member roster". Creates a DRAFT. Note: `releaseDate`, `coder`, `system`, `design` are NOT in the body. |
| `deleteRelease` (160) | DELETE `/api/releases/:code` | `X-Release-Key` | - | `DeleteResponse` (59): deleted, code, releaseNumber, wasPublished, wasDraft, wordpressPostUrl, archiveRemoved, wordpressPostTrashed, wordpressPostError | Revoke. 409 = "a newer release exists" (main.ts:688). Revoke text in renderer confirm() mentions WordPress trash and "only works if it's still the most recently created release". |
| `publishToWeb` (179) | POST `/api/releases/:code/publish` | `X-Release-Key` | multipart: archive, image, embedImage* (positional match to stored image-type embeds, comment 189) | `PublishToWebResult`: archiveUrl, wordpressPostUrl | Error body may carry `reason` (`already-published`, `publish-in-progress`, `number-taken`) and `wordpressPostUrl` on 409; mapped to `ReleasesApiError` and returned as `{ok:false,...}`. |

Numbering "MAX+1" is NOT computed in the app: the app only displays `nextReleaseNumber` (server) and uses the number returned by POST. The "MAX+1" rule itself lives in the server (UNCONFIRMED: server not read). Related client-visible server rules: revoke only for the newest release; revoking a draft "hands the number back" (types.ts:182-186, revoke-message.ts:12-15); abandoned drafts keep number+code forever (stale-drafts.ts:3-10).

`checkCodeAvailability`: server-side check of code uniqueness in the catalogue, in addition to the local filesystem check in `assertLocalCodeAvailable` (ipc-handlers.ts:42). The server's code-exists answer is shown by the renderer as "already exists on disk" (main.ts:432) (message wording mismatch).

### 2.2 door-repo-client.ts
- `submitDoor(archivePath, url = DOOR_REPO_SUBMISSIONS_URL)` (23): POST multipart `file` to `https://doors.uprough.net/api/door-repo/submissions` (hardcoded const, line 20). No auth ("No curation key is baked into the app", comment 8-12). Server validates extension, magic bytes, 8 MB cap, sha256 vs catalogue and queue; 409 on duplicate. Response `{message?, error?, id?}` -> `DoorSubmissionResult` (`ok`, `message`, `id?`). Used only when `Division.submitsToDoorRepo` (xinnovations).

### 2.3 RELEASES_WRITE_KEY
- Build-time: scripts/inject-release-key.js reads `process.env.RELEASES_WRITE_KEY`, writes `src/main/config.generated.ts` (`INJECTED_RELEASES_WRITE_KEY: string|null`); runs on `postinstall` and `dist` (package.json scripts). File is gitignored (comment line 9-10; gitignore itself not read: UNCONFIRMED).
- Runtime resolution: config.ts:40-48 `loadReleasesClientConfig`: injected key ?? `env.RELEASES_WRITE_KEY` ?? placeholder `REPLACE_ME_BEFORE_DISTRIBUTING` (server answers 401). `isPlaceholderKey` -> `SeedState.configWarning` (ipc-handlers.ts:94-100). Loaded once at index.ts:59 and passed to `registerIpcHandlers`.
- Used: header `X-Release-Key` on POST /api/releases (releases-client.ts:147), DELETE (165), POST publish (199). GET endpoints and the door repo are unauthenticated. Key is baked into the distributed app (so effectively shared by every member who gets a build).

## 3. Group identity coupling

### 3.1 divisions.ts (src/shared/divisions.ts, 122 lines)
- `DivisionId = 'uprough' | 'xinnovations'` (11), `DEFAULT_DIVISION = 'uprough'` (13). `Division` fields (15-44): `id`, `label`, `defaultDizBanner`, `editableDizRows[{row,label,placeholder}]`, `submitsToDoorRepo`, `inheritsBannerArt`. Records at 86-103. Helpers: `formatReleaseNumber` (111; xinnovations = 3-wide with 'o' for zero, "Must agree with the releases server's formatReleaseNumber"), `isDivisionId` (116, hardcodes both ids), `divisionOf` (120). Banner constants `UP_ROUGH_BANNER` (53) and `X_INNOVATIONS_BANNER` (~66-82), labelled art for the group.
- Consumers (grep of `divisions`, DIVISIONS, divisionOf, `.division`, `'xinnovations'`):
  - renderer/main.ts:15 import; 106, 112, 121, 139, 164, 168 (banner/rows/inheritance); 367 `isDivisionId`; 378 `xFieldsSection.hidden = currentDivision !== 'xinnovations'`; 490 `currentDivision === 'xinnovations'` (adds x fields to formData).
  - main/ipc-handlers.ts:22,72,136,159-162,179 (seed division, xinnovations manifest branch, `formatReleaseNumber`, `submitsToDoorRepo`).
  - main/releases-client.ts:4,89,108,133 (division query param + body field).
  - main/nfo-generator.ts:141 (`=== 'xinnovations'` selects NFO renderer).
  - main/nfo-x-innovations.ts:2,216-218 (`DIVISIONS.xinnovations` banner + name row).
  - shared/diz-release-line.ts:1,6 (`DIVISIONS.uprough.editableDizRows[0].row` as default row).
  - shared/aminet-readme.ts:2,118,120,132 (`division.label` in Author and "Released by <label> - https://www.uprough.net").
  - preload/preload.ts:2, shared/types.ts:1,35 (types).
  - renderer/index.html:16-17 radio buttons (value hardcoded `uprough`/`xinnovations`, labels "Up Rough", "Up Rough /X Innovations").
  - Server: the `division` column and accepted ids (divisions.ts:8-9 comment).

### 3.2 Hardcoded strings
- File/folder prefix `UP-`: archiver.ts:23,26,28,34,36; staging.ts:11,12,16,23; ipc-handlers.ts:45-47,52,123,124,173 (+ comment 37); nfo-generator.ts:65 (`(UP-<CODE>.<fmt>)` in the NFO "Release named" line); main.ts:193 (`up-<code>.<fmt>` preview, lowercase), 432 (warning); ipc-handlers.ts:143 (dry-run filenames `up-<code>.<fmt>`, comment says "matches catalog.ts's filenamesFor", i.e. a SERVER file); index.html:22 label "becomes UP-<CODE>". Aminet file names are derived from the archive base name (aminet-readme.ts:83), so they inherit `up-<code>`.
- Code length 5: index.html:23 (`maxlength="5"`), 192 revoke input, main.ts:428 (`code.length !== 5`).
- Output root name `'Up Rough Releases'`: output-root.ts:8 (only for packaged builds). Dev root: index.ts:17 (repo root).
- Window/app names: index.html:6,10 ("Up Rough Release Maker"), index.ts:53 (dialog title), package.json `name/description/author/homepage (www.uprough.net)/build.appId net.uprough.makerelease/productName/linux.maintainer`.
- NFO strings (constants.ts): `MEMBERS_HEADING` "active up rough heroes" (11 lines in: line 10), `ONLINE_LINE` and `FOOTER_LINE` with `www.uprough.net` (11-12), `BANNER` ASCII logo (14-26, Up Rough logo; unconfirmed that it is the group's logo but it is the plain-NFO banner), heading strings `[ r e l e a s e   n o t e s ]`, `[ n e w s ]`, `r e l e a s e d   s o   f a r` (7-10). Layout constants (indent 8, wrap 68, label width 12/20) lines 1-5.
- /X NFO: src/main/x-innovations-template.ts is GENERATED from test/fixtures/UP-MD15.NFO by scripts/make-x-template.py (header lines 1, python docstring); the template is a single big string const (13 lines). nfo-x-innovations.ts (322 lines): frame width FIELD_WIDTH 59, gutters `   | %% |`, roster/boards/releaseList/notes regions. `X_INNOVATIONS_BANNER` in divisions.ts has the line "uP rOUGH /X iNNOVATiONS pRESENTS:" (81).
- Aminet: `Released by ${division.label} - https://www.uprough.net` (aminet-readme.ts:132); `AMINET_UPLOAD_URL = ftp://main.aminet.net/new/` (aminet.ts:15) (generic, not group specific); type list and architectures (aminet-readme.ts:37-, 25-) generic Aminet data; `Distribution: Aminet` (generic).
- Aminet remembered uploader prefs: localStorage key `'aminet-uploader'` (main.ts:209). This is the ONLY localStorage use found (grep `localStorage`: main.ts:209-237).
- Publish heading "11. Publish to uprough.net" (index.html:154); Aminet checkbox text mentions ftp (158). Release-name placeholders in index.html (30-36, 60) are Fleisch Bitte examples.
- Category list `<select id="entry-category">` hardcoded in index.html:65-77 (Demo, Intro, Musicdisk, Music, Graphics, ASCII, Tool, Door, Video, Science, Other) (member of the catalogue vocabulary; the server stores it as free text, UNCONFIRMED whether it validates).
- Revoke/WordPress wording: renderer/revoke-message.ts:12-29 (WordPress post trashed, wp-admin 30 days, server logs); main.ts:650-654 (confirm text); index.html:189-194 section. The app assumes the publish target is a WordPress site fronted by the releases server (fields `wordpressPostUrl`, `wordpressPostTrashed`, `wordpressPostError`; types.ts:158-163, 187-196, releases-client.ts:23,65-69).
- SeedState comments: types.ts:90 ("www.uprough.net"), 75, 82-86, 103 ("Evoke" example), stale-drafts.ts:3-10 ("Evoke 2026", 217/216).
- Formats: `ReleaseFormat = 'LHA'|'ZIP'` (types.ts:16); index.html:142-143 radios; archiver.ts:22-29 switch (lha/zip binaries via external-tool.ts, install hints archiver.ts:6-10). `DRY_RUN_DIR='dry-run'` ipc-handlers.ts:69.
- Encoding: encoding.ts (49 lines): `toAsciiSafe` (transliteration table 3-7, used for DIZ and Aminet readme), `assertLatin1Safe`/`writeLatin1` (NFO/DIZ/readme are written as Latin-1 bytes). Not group specific (Amiga/DOS-scene convention; DIZ forced ASCII-safe, NFO allowed up to 0xFF).
- Fonts: src/renderer/fonts/TopazPlus_a1200_v1.0.ttf, @font-face in style.css:1-4, used at style.css:72 for the DIZ/NFO previews. build.renderer script copies fonts (package.json). Not group specific.
- Test fixture: test/fixtures/UP-MD15.NFO + UP-MD15-FILE_ID.DIZ (a real /X release) is the golden source for x-innovations (spec files, make-x-template.py:11).
- Release-line banner: `DIZ` banner rows editable only at row 8 (divisions.ts:91,99, diz-release-line.ts:6); the DIZ art frame has fixed columns (DIZ_COLS referenced in nfo-generator.ts:110 comment).

## 4. Renderer: fields and where data comes from

Form sections in index.html (ids), with source of data:

| # | Section / fields | Up Rough specific? | Data source |
|---|---|---|---|
| - | Division radio (`division`, 14-18) | Yes: two hardcoded divisions | local constants (divisions.ts) |
| 1 | Filename code (`#code`, 5 chars; becomes UP-CODE) | Yes (prefix) | local input; availability from SERVER `/exists` |
| 2 | Release name, platform tag, description, release notes (`#release-name`, `#platform-tag`, `#description-text`, `#release-notes-text`) | Generic fields; `releaseNotes`, `description` both sent to server | local input |
| 3 | News (`#news-text`) | Group-news block in NFO (generic concept, NFO heading text is Up Rough's) | local input |
| 4 | Members table (`#members-tbody`, `MemberlistTable`, memberlist-table.ts 52 lines) | Yes: roster is the group's; NFO prints it under "active up rough heroes"; POST /api/releases REPLACES the server roster | SERVER seed `members` (main.ts:384), edited locally, sent back at build |
| 5 | History entry: title (`#entry-title`), release date, category select, release number preview | Category list hardcoded; number preview uses `up-<code>.<fmt>` (main.ts:193) | `nextReleaseNumber` from SERVER seed; history list from SERVER |
| 5b | /X details (`#x-fields-section`, hidden unless xinnovations): x-release-date, system; coder/design derived from credits via regexes `CODER_ROLES`, `DESIGN_ROLES` (credits-creators.ts:53-54) | Yes | local |
| 5c | Boards table (`#boards-tbody`, `BoardsTable`, boards-table.ts 78 lines; fields name, sysop, hq, status; types.ts:66) | /X only ("Up Rough has none") | SERVER seed `boards`; sent in build only if non-empty (main.ts:498; an absent list means "leave the stored list alone") |
| 6 | Credits table (`#credits-tbody`, `CreditsTable` credits-table.ts 83 lines) + creators autocomplete (creators-autocomplete.ts 125 lines, fed by `memberlist.getMembers()`, main.ts:77-79); `creatorsFromCredits` derives the NFO creator line (credits-creators.ts:14) | Autocomplete data = server roster | local rows; names suggested from SERVER roster |
| 7 | Media embeds (`#media-embeds-container`, `MediaEmbedsTable`, media-embeds-table.ts 131 lines): youtube (caption+URL) or image (caption + local file path) | Targets the WordPress post (heading text: "in the WordPress post") | local; image paths kept only in renderer (`lastEmbedImagePaths`), uploaded at publish |
| 8 | DIZ release line(s) (`#diz-release-lines`, `#diz-banner-preview`) built from `Division.editableDizRows` and `applyReleaseLines` | Yes: banner art per division | `currentDizText` from SERVER seed (the previous release's art) for divisions that inherit; else local `defaultDizBanner` |
| 9 | Files to include (native picker) | no | local fs |
| 10 | Archive format (LHA/ZIP) | no (but fixed pair) | local |
| - | Dry run checkbox | no | local; skips server writes but still calls GET seed |
| 11 | Publish: cover image picker, Aminet block (checkbox, short, type+datalist, architecture+datalist, uploader name/email, readme preview, problems list), Publish button | Heading says "uprough.net"; the publish step requires the SERVER | local + localStorage `aminet-uploader` |
| - | Revoke a release (`#revoke-code`, 5 chars) | WordPress wording | SERVER DELETE |

SeedState warnings shown in build status (main.ts:390): numberingWarning (server comparison with WordPress), staleDraftWarning (server drafts list), configWarning (placeholder key).

## 5. Tests (app/test/unit, vitest via `npm test`; no network, all server calls are mocked)

| Spec | Covers | Kind |
|---|---|---|
| aminet-readme.spec.ts (118) | renderAminetReadme, aminetReadmeProblems, aminetFileNames | pure |
| aminet.spec.ts (125) | uploadToAminet with child_process + fs/promises mocked | mocks, no server |
| archiver.spec.ts (74) | packArchive command lines (child_process mocked) | mocks |
| boards-table.spec.ts (34) | `boardsFromRows` | pure |
| credits-creators.spec.ts (95) | `creatorsFromCredits`, `creditNamesForRole` | pure |
| divisions.spec.ts (64) | DIVISIONS records, `divisionOf`, `isDivisionId`, release-line slots | pure (identity: asserts Up Rough and /X data) |
| diz-release-line.spec.ts (163) | slot finding, centering, applying lines against DIVISIONS banners | pure (identity data) |
| door-repo-client.spec.ts (69) | `submitDoor` with global.fetch mocked, temp file | fetch mock (server shape) |
| encoding.spec.ts (64) | `toAsciiSafe`, `assertLatin1Safe`, `writeLatin1` (fs mocked) | pure/mock |
| ipc-handlers.spec.ts (606) | handlers with electron, releases-client, nfo-generator, staging etc. mocked; build flow, dry run, door submit, publish outcomes, revoke | server-shape mocks (fetchSeed/publishRelease/...); the biggest server-fixture dependency |
| nfo-generator.spec.ts (143) | member/history/NFO/DIZ rendering, `generateOutputs` | pure |
| nfo-x-innovations.spec.ts (221) | /X frame, wrapNotes, rosterBlock etc.; reads test/fixtures/UP-MD15.NFO | pure + fixture file |
| output-root.spec.ts (45) | `resolvePackagedOutputRoot` fallbacks | pure |
| release-date.spec.ts (18) | `todayIso` | pure |
| releases-client.spec.ts (381) | every client function with global.fetch mocked (URLs, headers, body, errors); writes temp archive/image files | fetch mock = server contract |
| revoke-message.spec.ts (62) | `describeRevoke` | pure |
| staging.spec.ts (93) | `stageRelease` on a real temp dir (UP-<CODE> layout, Latin-1) | fs (real tmp) |
| stale-drafts.spec.ts (43) | `describeStaleDrafts` | pure |
| x-golden.spec.ts (137) | /X NFO rendered against the real UP-MD15 NFO and DIZ fixtures | fixture-bound |

Fixtures: test/fixtures/UP-MD15.NFO (263 lines), UP-MD15-FILE_ID.DIZ. No spec calls a live server. No renderer DOM tests (only extracted pure helpers). No test covers `index.ts`, `config.ts`, `inject-release-key.js`, `main.ts` or `external-tool.ts` (grep of spec imports; unconfirmed for config.ts by name only).

## 6. Table

Kind: server = depends on releases-doorserver / doors server / WordPress behaviour; identity = Up Rough name, prefix, art, URL; art = scene art or fixed layout; none = generic.

| File | Coupled | Kind | Evidence |
|---|---|---|---|
| src/main/config.ts | yes | server | config.ts:26 base URL, 40-48 key |
| src/main/config.generated.ts | yes (ignored) | server | key injection |
| scripts/inject-release-key.js | yes | server | RELEASES_WRITE_KEY, :TARGET |
| src/main/releases-client.ts | yes | server | endpoints 91,112,117,145,163,197,229; "releases.uprough.net" strings 76 etc. |
| src/main/door-repo-client.ts | yes | server | const URL line 20 |
| src/main/ipc-handlers.ts | yes | server + identity | calls server 77,113,150,162,210,239; `UP-` 45-47,123,173; `up-` 143; xinnovations 159 |
| src/main/index.ts | yes | identity | dialog title 53; dev root 17 |
| src/main/output-root.ts | yes | identity | FOLDER_NAME line 8 |
| src/main/archiver.ts | yes | identity | `UP-` 23,26,28,34,36 |
| src/main/staging.ts | yes | identity | `UP-` 11,12,16,23 |
| src/main/nfo-generator.ts | yes | identity + art | `(UP-<CODE>...)` 65; xinnovations branch 141; constants imports 3-16 |
| src/main/nfo-x-innovations.ts | yes | art + identity | DIVISIONS.xinnovations 216; fixed frame 59 cols |
| src/main/x-innovations-template.ts | yes | art | generated from UP-MD15.NFO |
| src/main/stale-drafts.ts | yes | server | consumes `DraftEntry` from releases-client (1,3-10) |
| src/main/aminet.ts | partly | none (target Aminet) | URL line 15; no group strings |
| src/main/encoding.ts | no | none | pure transliteration/Latin-1 |
| src/main/external-tool.ts, fs-helpers.ts | no | none | spawn helpers / fs |
| src/preload/preload.ts | yes | server (shape) | channels = server-backed ops; DivisionId import line 2 |
| src/shared/types.ts | yes | server + identity | `SeedState` (73-106), `DeleteReleaseResult` (178-197), WordPress fields, `division` line 35, "www.uprough.net" comment 90 |
| src/shared/divisions.ts | yes | identity + art | DivisionId 11, records 86-103, banners 53-82 |
| src/shared/constants.ts | yes | identity + art | headings 7-11, URLs 11-12, BANNER 14-26 |
| src/shared/aminet-readme.ts | partly | identity | line 132 URL + division.label; rest generic Aminet rules |
| src/shared/diz-release-line.ts | yes | identity + art | DIVISIONS.uprough line 6; DIZ frame columns |
| src/shared/text-wrap.ts | no | none | pure wrap |
| src/shared/release-date.ts | no | none | todayIso |
| src/renderer/index.html | yes | identity + server | title 6,10; radios 16-17; "UP-<CODE>" 22; category list 65-77; "Publish to uprough.net" 154 |
| src/renderer/main.ts | yes | server + identity | api calls 375,430,524,591,662; xinnovations 378,490; `UP-` 432; `up-` 193; localStorage 209; length 5 at 428 |
| src/renderer/memberlist-table.ts | yes | server (data) | roster seeded by server |
| src/renderer/boards-table.ts | yes | server (data) | boards seeded by server |
| src/renderer/credits-table.ts | no | none | local rows |
| src/renderer/creators-autocomplete.ts | yes | server (data) | suggestions from server roster |
| src/renderer/credits-creators.ts | partly | none | role regexes for /X header fields |
| src/renderer/media-embeds-table.ts | yes | server (WordPress) | youtube/image embeds for the post |
| src/renderer/revoke-message.ts | yes | server | WordPress/archive wording lines 12-29 |
| src/renderer/style.css, fonts/ | no | none (Topaz font) | style.css:1-4,72 |
| package.json | yes | identity | appId, productName, author, homepage, maintainer |
| test/fixtures/UP-MD15.* | yes | art + identity | real /X release |
| test specs | see section 5 | - | - |

## Surprises
- Release numbering, code uniqueness, the member roster, boards, and the previous DIZ art are all STORED on the server; the app has no local catalogue. A dry run still needs the server (GET /seed, plus GET /manifest for /X).
- `POST /api/releases` at build time creates a server-side draft and overwrites the roster; publish is a second, later call. The app has no offline mode.
- The `UP-` prefix is hardcoded in 5 source files and in the user-visible NFO line; the dry-run `up-<code>` filename convention copies a server function (`filenamesFor`) with only a comment tying them.
- `revision` from the server is passed through but never used by the app.
- Build `releaseDate`, `coder`, `system`, `design` are used only by the local NFO, never sent to the server.
