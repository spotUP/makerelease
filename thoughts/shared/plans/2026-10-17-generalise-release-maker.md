---
date: 2026-10-17
topic: generalising makerelease so any scene group can use it with its own NFO and DIZ
tags: [makerelease, profiles, placeholder-templates, aminet, public-release]
status: draft
---

# makerelease for any group: profiles, a placeholder renderer, no server by default

Research index: `thoughts/shared/research/2026-10-17_generalise-release-maker.md`
(coupling inventory and template mechanics are linked from there).
Ledger: `thoughts/spot/todos.md`.

## Decisions (all closed; do not reopen during implement)

Taken earlier (ledger):

1. Generalise in place. Up Rough is the first group profile. The repo goes
   public under a new name.
2. No catalogue and no numbering by default. A generic group publishes
   nowhere but Aminet, numbers nothing, needs no database. The release number
   is typed by hand when the template has a slot for it. The
   releases-doorserver backend exists only for the Up Rough and /X profiles.
3. MIT licence.
4. Up Rough and /X stay code renderers. Generic groups get placeholder
   templates (one NFO, one DIZ). One renderer interface, two code renderers
   plus one placeholder renderer. No directive language.

Taken 2026-10-17 in the plan phase (with spot):

5. **Placeholders.** Syntax `{{name}}`, lower case, same braces as the /X
   template. Minimal set: `group`, `url`, `code`, `filename`, `format`,
   `release_name`, `platform`, `date`, `release_number`, `description`,
   `creators`, `notes`, `news`, `members`, `credits`. The last four are block
   markers: the marker stands alone on its line, every value line is emitted
   with the marker line's prefix and suffix, and an empty value removes the
   line. One line per member (`nickname` padded to the longest nickname plus
   two, then `role`) and one line per credit (`role: name`, the same line the
   Aminet readme prints). An unknown name fails the build and names the line.
   Two width rules and nothing else: `{{date}}` is replaced by the value as
   is; `{{date      }}` (spaces inside the braces) is a fixed-width field,
   padded with spaces to the marker's full width, and a value that is longer
   than the field fails the build with both lengths. The width rule is what
   keeps a framed NFO or a 44-column DIZ intact without a directive language.
6. **Profiles** live in `<userData>/profiles/<id>/` as `profile.json` plus,
   for the placeholder renderer, the two template files next to it. A
   dropdown replaces the division radio. "Open profiles folder" and "Reload
   profiles" buttons sit beside it. On first launch with no profiles folder
   the app writes an `example` profile. Up Rough's write key lives in its own
   `profile.json`; `scripts/inject-release-key.js` and
   `src/main/config.generated.ts` are deleted. One public build serves
   everyone.
7. **Name.** Product name `makerelease`, repo `spotUP/makerelease`, appId
   `net.uprough.makerelease` unchanged.
8. **Output root** is per profile: packaged `<documents>/<label> Releases/`
   (fallbacks home, then userData, as today), dev `<repo>/releases-out/<id>/`.
   It is resolved per build, not at startup, because the profile is chosen in
   the window.
9. **Hidden under `backend: none`**: boards, media embeds, creators
   autocomplete, the release-history entry fields that only feed the server
   (category, title), the release-number preview, the Publish-to-website
   section, and the Revoke section. Shown for everyone: code, release name,
   platform, date, description, release notes, news, members, credits, files,
   format, dry run, Build, and Aminet upload (which moves out of the website
   publish section). The member list for a generic group is seeded from
   `profile.json` and edited per release; edits are not written back.
10. **Public hygiene**: a new public repository `spotUP/makerelease` with a
    single-commit history made from the scrubbed tree. The private repo stays
    as the archive. `thoughts/shared` goes public after a grep for hosts,
    backup paths and nginx paths; `thoughts/spot/` is already ignored.

## What stays byte-identical

- The Up Rough NFO and DIZ for any form input (phase 0 freezes a golden
  fixture from the current code before anything moves).
- The /X NFO (`test/unit/x-golden.spec.ts` keeps passing unchanged).
- The archive layout `<prefix><CODE>/file_id.diz` and
  `<prefix><CODE>/<prefix><CODE>/<prefix><CODE>.NFO`, Latin-1, LF.
- The releases-doorserver protocol: no server change in this plan.

## Model

### `app/src/shared/profile.ts` (new)

```ts
import { DivisionId, isDivisionId } from './divisions';
import { MemberEntry } from './types';

export type RendererKind = DivisionId | 'placeholder';
export type BackendKind = 'none' | 'releases-doorserver';

export interface ServerConfig {
  baseUrl: string;
  writeKey: string;
}

export interface GroupProfile {
  /** Folder name under profiles/; [a-z0-9-]+. */
  id: string;
  /** Group name as printed: "Up Rough", "Example Group". */
  label: string;
  /** Printed in the Aminet readme's "Released by" line. */
  url: string;
  /** Prefix of the archive, folder and NFO names: "UP-" for Up Rough, "" allowed. */
  filePrefix: string;
  /** Length of the code typed in the form; 5 for Up Rough. */
  codeLength: number;
  renderer: RendererKind;
  backend: BackendKind;
  /** Present exactly when backend is 'releases-doorserver'. */
  server?: ServerConfig;
  /** Seed for the member list when there is no server to seed it. */
  members: MemberEntry[];
}

/** What the renderer process needs to populate the dropdown and hide sections. */
export interface ProfileSummary {
  id: string;
  label: string;
  filePrefix: string;
  codeLength: number;
  renderer: RendererKind;
  backend: BackendKind;
}

export const TEMPLATE_FILES = { nfo: 'release.nfo', diz: 'file_id.diz' } as const;

export function summarize(profile: GroupProfile): ProfileSummary { ... }

/**
 * Hand-written guard, no schema library: every error names the file and the
 * field so a group editing JSON by hand sees exactly what to fix.
 */
export function parseProfile(raw: unknown, id: string, file: string): GroupProfile {
  // renderer must be 'uprough' | 'xinnovations' | 'placeholder'
  // backend 'releases-doorserver' requires renderer uprough|xinnovations and a server block
  // backend 'none' forbids a server block
  // filePrefix defaults to '', codeLength defaults to 5, members defaults to []
}
```

`divisionOf(renderer)` keeps serving the two code renderers; `Division` keeps
its DIZ banner, editable rows, `submitsToDoorRepo` and `inheritsBannerArt`.
Nothing in `divisions.ts` changes except the doc comment that now calls it the
record behind the two built-in renderers.

### Up Rough profiles on disk (reference, documented in README)

`profiles/uprough/profile.json`:

```json
{
  "label": "Up Rough",
  "url": "https://www.uprough.net",
  "filePrefix": "UP-",
  "codeLength": 5,
  "renderer": "uprough",
  "backend": "releases-doorserver",
  "server": { "baseUrl": "https://releases.uprough.net", "writeKey": "<key>" }
}
```

`profiles/xinnovations/profile.json` is the same with `"label": "Up Rough /X
Innovations"` and `"renderer": "xinnovations"`. The division sent to the
server is the renderer id, so there is no second field to keep in sync.

### Example profile written on first launch

`profiles/example/profile.json`:

```json
{
  "label": "Example Group",
  "url": "https://example.net",
  "filePrefix": "EX-",
  "codeLength": 5,
  "renderer": "placeholder",
  "backend": "none",
  "members": [{ "nickname": "coder", "role": "code" }]
}
```

`profiles/example/release.nfo` (every placeholder used once, plain frame):

```
            {{group}}  -  {{url}}

  Release ..: {{release_name}} ({{platform}})
  File .....: {{filename}}
  Number ...: {{release_number}}
  Date .....: {{date}}

  {{description}}

  -- notes -------------------------------------------------------------
  {{notes}}

  -- news --------------------------------------------------------------
  {{news}}

  -- members -----------------------------------------------------------
  {{members}}

  -- credits -----------------------------------------------------------
  {{credits}}

                 {{group}} - {{url}}
```

`profiles/example/file_id.diz` (44 columns, fixed-width fields):

```
 ____________________________________________
|                                            |
|  {{group                                }} |
|  {{release_name                         }} |
|  {{platform            }} {{date       }}  |
|____________________________________________|
```

The example files are TypeScript string constants in
`app/src/main/example-profile.ts`, written with `writeLatin1`, so the test
can assert the bytes.

### Placeholder values

| Name | Source |
|------|--------|
| `group`, `url` | `profile.label`, `profile.url` |
| `code` | `form.code` upper-cased |
| `filename` | `releaseBaseName(profile, code) + '.' + ext` (same function as the archiver) |
| `format` | `form.format` (`LHA` or `ZIP`) |
| `release_name`, `platform` | `form.releaseName`, `form.platformTag` |
| `date` | `form.releaseDate` (ISO, defaults to today in the form) |
| `release_number` | `form.releaseNumberText` (new, free text, generic only) |
| `description`, `creators` | `form.description`, `form.entryCreators` |
| `notes`, `news` | textareas split on `\n` after CRLF normalisation; trailing whitespace stripped per line |
| `members` | `form.members`, one line each, `nickname.padEnd(max + 2) + role` |
| `credits` | `form.credits`, one line each, `formatCreditLine` shared with the Aminet readme |

## Phases

Each phase ends with `cd app && npm run typecheck && npm test` green and a
commit. Manual checks are listed separately and are never ticked by the
implementer.

### Phase 0: freeze what must not move

Files: `app/test/fixtures/UP-GOLD.NFO` (new), `app/test/fixtures/UP-GOLD.DIZ`
(new), `app/test/unit/ur-golden.spec.ts` (new),
`app/test/unit/nfo-x-innovations.spec.ts` (edit), `app/src/main/nfo-x-innovations.ts`
(edit), `app/scripts/make-x-template.py` (edit), `app/src/main/x-innovations-template.ts`
(regenerate).

1. Write `ur-golden.spec.ts` with one fixed `ReleaseFormData` (two members,
   three credits, notes with a blank line, news, a history of three entries
   including one whose creators line needs the 68-column wrap, release number
   218, a DIZ banner with the release line on row 8) and a fixed history. It
   calls `generateOutputs(history, 218, form)` from the current code and
   compares `nfoText` and `dizText` with the fixtures as Latin-1 strings.
2. Produce the fixtures once from the current code, before any refactor:

   ```sh
   cd app && npx tsx -e "import('./test/unit/ur-golden.fixture.ts').then(m => m.writeFixtures())"
   ```

   `ur-golden.fixture.ts` holds the form and history and `writeFixtures()`;
   the spec imports the same objects. Commit the fixtures. From then on the
   spec is the parity proof for every later phase.
3. Regression test for the `String.replace` `$&` hazard: release notes
   containing `$& $1 $$` must appear literally in `renderXInnovationsNfo`
   output. Fails today on lines 305 and 309-320 of `nfo-x-innovations.ts`.
   Fix: every `.replace(marker, value)` becomes `.replace(marker, () => value)`.
   `x-golden.spec.ts` must still pass (the fixture carries no `$`).
4. Stale comment: `scripts/make-x-template.py` writes the header that names
   `make-x-template.ts`; change the string in the script, re-run
   `python3 -I scripts/make-x-template.py`, and the diff of
   `x-innovations-template.ts` must be that one line.

Automated: `npx vitest run test/unit/ur-golden.spec.ts test/unit/nfo-x-innovations.spec.ts test/unit/x-golden.spec.ts`.

Success: fixtures committed; `$&` test fails before the fix and passes after;
reverting the fix fails it again; template diff is one line.

### Phase 1: profiles replace divisions in the UI and the config

Files: `app/src/shared/profile.ts` (new), `app/src/main/profiles.ts` (new),
`app/src/main/example-profile.ts` (new), `app/src/shared/naming.ts` (new),
`app/src/main/config.ts` (delete), `app/src/main/config.generated.ts` (delete),
`app/scripts/inject-release-key.js` (delete), `app/package.json`,
`.gitignore`, `app/src/main/index.ts`, `app/src/main/ipc-handlers.ts`,
`app/src/main/releases-client.ts`, `app/src/main/output-root.ts`,
`app/src/main/staging.ts`, `app/src/main/archiver.ts`,
`app/src/main/nfo-generator.ts` (line 65 only), `app/src/shared/types.ts`,
`app/src/preload/preload.ts`, `app/src/renderer/index.html`,
`app/src/renderer/main.ts`, tests listed below.

1. `shared/naming.ts`:

   ```ts
   export function releaseBaseName(p: Pick<ProfileSummary, 'filePrefix'>, code: string): string {
     return `${p.filePrefix}${code.toUpperCase()}`;
   }
   export function archiveFileName(p, code, format: ReleaseFormat): string {
     return `${releaseBaseName(p, code)}.${format === 'LHA' ? 'LHA' : 'ZIP'}`;
   }
   ```

   Every `UP-${...}` in `archiver.ts`, `staging.ts`, `ipc-handlers.ts`,
   `nfo-generator.ts:65` and `renderer/main.ts:193,432` goes through these
   two. `nfo-generator.ts:65` passes `profile` through `renderNfo`; the UR
   golden spec pins the result with `filePrefix: 'UP-'`. The dry-run history
   entry's `filenames` becomes `archiveFileName(profile, code, format).toLowerCase()`.
2. `main/profiles.ts`:

   ```ts
   export async function listProfiles(profilesDir: string): Promise<GroupProfile[]>; // sorted by id
   export async function loadProfile(profilesDir: string, id: string): Promise<GroupProfile>;
   export async function readTemplates(profilesDir: string, id: string): Promise<{ nfo: string; diz: string }>; // Latin-1, placeholder renderer only
   export async function ensureExampleProfile(profilesDir: string): Promise<void>; // writes example/ only when profilesDir does not exist
   ```

   `profilesDir` is `path.join(app.getPath('userData'), 'profiles')`, passed
   in from `index.ts` (dev and packaged alike; dev keeps its own userData).
   A profile folder with an unreadable or invalid `profile.json` is reported
   in the list as `{ id, error }` and shown disabled in the dropdown; the
   rest still load.
3. Config: `ReleasesClientConfig` moves to `releases-client.ts` and is built
   from `profile.server` by `clientConfigFor(profile, env)`; env
   `RELEASES_WRITE_KEY` and `RELEASES_API_BASE_URL` still override for
   development against a local server. `configWarning` now fires when
   `server.writeKey` is empty. Delete `config.ts`, `config.generated.ts`,
   `inject-release-key.js`; drop `postinstall` and the `inject-release-key`
   step from `dist` in `package.json`; drop the `config.generated.ts` and
   `UP-*` lines from `.gitignore` and add `releases-out/`.
4. IPC: `loadSeedState(profileId)`, new `listProfiles()`, new
   `openProfilesFolder()` (`shell.openPath`). `SeedState` gains
   `profile: ProfileSummary` and `outputRoot: string`. `ReleaseFormData`
   gains `profileId: string` and `releaseNumberText?: string`; `division`
   stays for the two code renderers (set by main from the profile, no longer
   by the renderer). `registerIpcHandlers(profilesDir, env)` replaces
   `registerIpcHandlers(makereleaseRoot, releasesConfig)`; each handler loads
   the profile by id and derives the output root:

   ```ts
   function outputRootFor(profile: GroupProfile): string {
     return app.isPackaged
       ? resolvePackagedOutputRoot((n) => app.getPath(n), `${profile.label} Releases`)
       : path.join(repoRoot(), 'releases-out', profile.id);
   }
   ```

   `resolvePackagedOutputRoot(getPath, folderName)` takes the folder name as
   a parameter; `output-root.spec.ts` updates its expectations.
5. Renderer: the Division section becomes

   ```html
   <section>
     <h2>Group</h2>
     <select id="profile-select"></select>
     <button type="button" id="open-profiles-btn">Open profiles folder</button>
     <button type="button" id="reload-profiles-btn">Reload profiles</button>
     <div id="profile-status" class="status"></div>
   </section>
   ```

   `main.ts` loads the list, restores the last choice from localStorage
   (`profile-id`), calls `init()` on change, and uses `seed.profile` for
   `divisionOf` lookups (`currentDivision` becomes
   `seed.profile.renderer` when it is a `DivisionId`). The code input's
   `maxlength` and the "becomes UP-<CODE>" hint come from `codeLength` and
   `filePrefix`.
6. Startup: `index.ts` calls `ensureExampleProfile(profilesDir)` before
   `createWindow()`; the error dialog title becomes `makerelease`.

Tests (automated): `profile.spec.ts` (parse: every validation error, defaults),
`profiles.spec.ts` (tmp dir: list, invalid profile reported not thrown,
example written only on first run, templates read as Latin-1), `naming.spec.ts`,
`output-root.spec.ts` (folder name parameter), `ipc-handlers.spec.ts`
(handlers take `profileId`; `loadSeedState` for the uprough profile still
calls `fetchSeed` with division `uprough`; `buildRelease` with prefix `EX-`
stages `EX-<CODE>`), `releases-client.spec.ts` (`clientConfigFor`: env
override wins over profile key, empty key flags warning), `ur-golden.spec.ts`
and `x-golden.spec.ts` unchanged and green.

Reachability (one per feature): `test/unit/reach-profiles.spec.ts` drives the
real `listProfiles` IPC handler (electron mocked as in `ipc-handlers.spec.ts`,
`app.getPath('userData')` pointing at a tmp dir, nothing else mocked) and
asserts the example profile exists on disk afterwards and the handler returns
it with `renderer: 'placeholder'`. Sentinel: the example template's byte
`EX-` prefix read back from `file_id.diz`.

Manual (LOCAL, `cd app && npm start`): dropdown lists `example` on first
launch; "Open profiles folder" opens
`~/Library/Application Support/makerelease/profiles`; after adding
`uprough/profile.json` with the real key and pressing Reload, the Up Rough
choice loads members and the next release number as before. PASS: same seed
as the current build shows. FAIL: warning "no write key" with a key present.

Success: no `UP-` literal outside `test/` and the Up Rough profile reference
in README; `grep -rn "UP-" app/src` returns only `naming.spec`-free code paths
(count verified from the grep, expected 0 in `app/src`).

### Phase 2: backend seam and section visibility

Files: `app/src/main/backend.ts` (new), `app/src/main/backend-doorserver.ts`
(new), `app/src/main/backend-none.ts` (new), `app/src/main/ipc-handlers.ts`,
`app/src/shared/section-visibility.ts` (new), `app/src/shared/aminet-readme.ts`,
`app/src/renderer/main.ts`, `app/src/renderer/index.html`, tests.

1. `backend.ts`:

   ```ts
   export interface ReleaseBackend {
     seed(): Promise<Omit<SeedState, 'profile' | 'outputRoot'>>;
     codeExists(code: string): Promise<boolean>;
     /** Real build: reserve a number and return the history the NFO prints. */
     create(form: ReleaseFormData): Promise<{ releaseNumber: number; history: ReleaseHistoryEntry[] }>;
     /** Dry run: same shape, nothing reserved. */
     preview(form: ReleaseFormData): Promise<{ releaseNumber: number; history: ReleaseHistoryEntry[] }>;
     releaseList(form: ReleaseFormData, releaseNumber: number): Promise<ReadonlyArray<{ displayNumber: string; art: string }>>;
     publishToWeb(...): Promise<PublishToWebResult>;
     deleteRelease(code: string): Promise<DeleteReleaseResult>;
     afterBuild(archivePath: string): Promise<DoorSubmissionOutcome | undefined>;
   }
   export function backendFor(profile: GroupProfile, env: NodeJS.ProcessEnv): ReleaseBackend;
   ```

   `backend-doorserver.ts` is today's handler bodies moved behind the
   interface (seed plus numbering check plus stale drafts; manifest fetch for
   `releaseList`; door submission in `afterBuild` when `submitsToDoorRepo`).
   `backend-none.ts`: `seed` returns `profile.members`, no boards, history
   `[]`, `nextReleaseNumber: 0`, `currentDizText: ''`, `revision: 0`;
   `codeExists` false; `create` and `preview` return
   `{ releaseNumber: 0, history: [] }`; `releaseList` `[]`; `publishToWeb`
   and `deleteRelease` throw `Error('This group has no release server')`;
   `afterBuild` undefined. `ipc-handlers.ts` becomes glue: load profile,
   pick backend, stage, pack, finalise.
2. Aminet moves out of the website publish: a new IPC `uploadToAminet(request)`
   runs `uploadToAminet` on its own; `publishToWeb` loses its `aminet`
   argument. For the doorserver profiles the renderer calls both, website
   first, Aminet only after the website succeeded (today's order, now in the
   renderer). `renderAminetReadme` takes `group: { label: string; url: string }`
   instead of `division`; line 132 prints `Released by ${label} - ${url}`.
   `formatCreditLine(c)` is exported from `aminet-readme.ts` for phase 3.
3. `section-visibility.ts` (pure, shared):

   ```ts
   export interface SectionVisibility {
     xFields: boolean; boards: boolean; historyEntry: boolean; releaseNumberPreview: boolean;
     releaseNumberText: boolean; mediaEmbeds: boolean; creatorsAutocomplete: boolean;
     dizReleaseLine: boolean; dizPreviewOnly: boolean; publishWebsite: boolean; revoke: boolean; aminet: boolean;
   }
   export function visibleSections(p: ProfileSummary): SectionVisibility {
     const server = p.backend === 'releases-doorserver';
     return {
       xFields: p.renderer === 'xinnovations', boards: p.renderer === 'xinnovations',
       historyEntry: server, releaseNumberPreview: server, releaseNumberText: !server,
       mediaEmbeds: server, creatorsAutocomplete: server,
       dizReleaseLine: p.renderer !== 'placeholder', dizPreviewOnly: p.renderer === 'placeholder',
       publishWebsite: server, revoke: server, aminet: true,
     };
   }
   ```

   `main.ts` applies it in `init()` by toggling `hidden` on section ids;
   `index.html` gains ids on every section it toggles (`section-history-entry`,
   `section-media-embeds`, `section-diz-line`, `section-aminet`,
   `section-release-number`) and the Aminet block becomes its own section
   with its own Upload button, shown after a successful real build. The
   "11. Publish to uprough.net" heading becomes "Publish to the group website".
   Numbered headings are renumbered by the renderer at init (a hidden section
   must not leave a gap), via a small `numberHeadings()` in `main.ts`.
4. Form: a text input `release-number-text` ("Release number, as it should
   print") in the hidden-by-default section; for generic profiles the Code
   section hint reads `becomes <prefix><CODE>`.

Tests (automated): `backend-none.spec.ts` (every method), `backend-doorserver.spec.ts`
(moved cases from `ipc-handlers.spec.ts`: stale drafts, numbering warning,
door submission only for real builds), `section-visibility.spec.ts` (the three
profiles: uprough, xinnovations, placeholder+none; a table test),
`aminet-readme.spec.ts` (group label and URL in the footer), `ipc-handlers.spec.ts`
(glue only: `uploadToAminet` handler calls `uploadToAminet` with the request,
`publishToWeb` no longer takes it).

Reachability: `test/unit/reach-backend-none.spec.ts` drives the real
`loadSeedState` and `buildRelease` handlers with an example profile in a tmp
userData dir, `global.fetch` replaced by a `vi.fn()` that throws, archiver's
`runArchiver` mocked (external tool). Asserts fetch call count 0, the
staged `EX-<CODE>/file_id.diz` exists, and `releaseNumber` is 0.

Manual (LOCAL): with the example profile, the form shows no boards, media,
history-entry, website publish or revoke sections and numbering is
contiguous; with Up Rough they all return. PASS: Build with dry run writes
`releases-out/example/dry-run/EX-TEST1/`.

Success: `ipc-handlers.ts` imports nothing from `releases-client.ts` or
`door-repo-client.ts` (verified by grep of its import block).

### Phase 3: the placeholder renderer

Files: `app/src/shared/placeholder-template.ts` (new),
`app/src/main/nfo-placeholder.ts` (new), `app/src/main/nfo-generator.ts`,
`app/src/main/ipc-handlers.ts`, `app/src/renderer/main.ts`, tests.

1. `placeholder-template.ts` (pure, no I/O):

   ```ts
   export type ScalarName = 'group' | 'url' | 'code' | 'filename' | 'format' | 'release_name'
     | 'platform' | 'date' | 'release_number' | 'description' | 'creators';
   export type BlockName = 'notes' | 'news' | 'members' | 'credits';
   export interface TemplateValues { scalars: Record<ScalarName, string>; blocks: Record<BlockName, string[]>; }
   export class TemplateError extends Error { constructor(readonly line: number, message: string) { super(message); } }
   const MARKER = /\{\{([a-z_]+)( *)\}\}/g;
   export function renderTemplate(template: string, values: TemplateValues): string;
   export function templateProblems(template: string): string[]; // unknown names with line numbers, for the preview
   ```

   Rules in `renderTemplate`, in order per line: (a) a line whose only
   marker is a block name and whose text outside the marker is whitespace or
   frame characters becomes prefix + value line + suffix per value line,
   fixed-width padding applied when the marker has inner spaces, removed
   entirely when the block is empty; (b) a block name used inline on a line
   with other markers or text is a `TemplateError` ("block placeholder must
   stand alone on its line"); (c) scalar markers are replaced via a function
   replacer (never a string, the `$&` hazard); (d) fixed-width: pad to
   `name.length + spaces.length`, longer value throws
   `TemplateError(line, 'value for date is 12 characters, field is 10')`;
   (e) unknown name throws `TemplateError(line, 'unknown placeholder {{x}}')`.
   Unmarked text is copied byte for byte, line endings LF, CRLF in the
   template normalised once at read time in `readTemplates`.
2. `nfo-placeholder.ts`: `renderPlaceholderOutputs(profile, templates, form): GeneratedOutputs`
   builds `TemplateValues` from the table above and renders both files.
   `generateOutputs` gains a `profile` and `templates` argument and
   dispatches on `profile.renderer`: `'placeholder'` here, `'xinnovations'`
   to `renderXInnovationsNfo`, `'uprough'` to `renderNfo`. The two code
   branches do not change.
3. Preview: `buildRelease` with dry run already writes the files; the
   renderer shows `dizText` in the existing `diz-banner-preview` for
   placeholder profiles (the `dizPreviewOnly` flag) by calling a new IPC
   `previewOutputs(form)` on input change, debounced, which runs
   `generateOutputs` without staging. Template errors come back as
   `{ problems: string[] }` and print in `build-status`.

Tests (automated): `placeholder-template.spec.ts` covers every rule above,
each as its own case: scalar as is, fixed width pads, fixed width overflow
throws with both numbers, block with prefix and suffix, block fixed width per
line, empty block removes the line, inline block throws, unknown name throws
with the line number, `$&` in a value stays literal, unmarked bytes including
`{` alone and `}}` survive, Latin-1 art bytes survive. `nfo-placeholder.spec.ts`:
the example templates with a full form produce the expected NFO and DIZ
(fixtures `test/fixtures/EX-TEST1.NFO`, `EX-TEST1.DIZ`, hand-checked once),
members padded to longest plus two, credits via `formatCreditLine`. `ur-golden.spec.ts`
and `x-golden.spec.ts` still green.

Reachability: `test/unit/reach-placeholder.spec.ts` drives the real
`buildRelease` handler (dry run) with a tmp userData dir holding a profile
whose `release.nfo` carries the unmarked sentinel line `SENTINEL-7F3A` and
`{{group}}`; `runArchiver` mocked; `generateOutputs` NOT mocked. Asserts the
NFO on disk under `dry-run/EX-<CODE>/EX-<CODE>/EX-<CODE>.NFO` contains both
`SENTINEL-7F3A` and the profile label, and `file_id.diz` is padded to 44
columns on every line.

Manual (LOCAL): type a release in the example profile, tick dry run, Build,
open `releases-out/example/dry-run/EX-TEST1/`; the NFO prints every field
and the DIZ frame is intact. Edit `release.nfo` to add `{{bogus}}`, Build:
the status line names the line number. PASS: both. Then untick dry run,
Build, the Aminet section appears, fill it, Upload: the FTP call runs
(memory note: first real Aminet FTP is untested live; use a throwaway
archive or stop before Upload).

Success: every placeholder in decision 5 appears in `placeholder-template.spec.ts`
as a case (count 15 scalars plus blocks, verified from the `ScalarName` and
`BlockName` unions, not from grep).

### Phase 4: name, README, licence

Files: `app/package.json`, `app/src/renderer/index.html` (title, h1),
`app/src/main/index.ts` (dialog title), `app/README.md`, `README.md` (new,
repo root), `LICENSE` (new), `RULES.md`.

1. `package.json`: `description` "Release archive maker for demoscene
   groups: NFO, DIZ, LHA or ZIP, Aminet upload", `productName` `makerelease`,
   `homepage` `https://github.com/spotUP/makerelease`. `author` and
   `linux.maintainer` stay `Up Rough` with the existing address.
2. `README.md` at the root for groups: install, first launch, the profiles
   folder path on macOS, Windows and Linux, `profile.json` fields,
   placeholder table (decision 5, with the two width rules and the block
   rule), the example templates, output folder, Aminet upload checklist,
   building from source. `app/README.md` becomes the developer page:
   commands, tests, the Up Rough and /X profiles with their server, the
   `RELEASES_WRITE_KEY` and `RELEASES_API_BASE_URL` env overrides, and the
   corrected production server (`releases.uprough.net`). The "Distributing a
   real build" section is replaced by "Builds carry no key; the key lives in
   the profile".
3. `LICENSE`: MIT, "Copyright (c) 2026 Up Rough".
4. `RULES.md` `## Commands`: drop the `RELEASES_WRITE_KEY=<key> npm run dist`
   line in favour of `npm run dist`.

Automated: `npm run typecheck && npm test && npm run build`; `grep -rn
"Up Rough Release Maker\|inject-release-key\|config.generated" app/src
app/package.json app/scripts` returns nothing.

Manual (LOCAL): the window title and h1 read `makerelease`; `npm run dist`
produces an app named makerelease.

### Phase 5: public hygiene

Done on a new branch of the private repo, then a new repository.

1. Scrub list, each hit fixed or deleted, counted from the grep output:
   `grep -rniE "[h]ippoplayer|\.[b]ak-|/etc/[n]ginx|sites-[a]vailable|[s]sh |[s]cp " --exclude-dir=node_modules --exclude-dir=dist .`
   over `app/`, `thoughts/shared/`, `RULES.md`, `README.md`. Expected hits
   today: `app/src/main/config.ts` (deleted in phase 1) and the research and
   plan docs under `thoughts/shared/` that quote server paths; rewrite those
   paragraphs to say "the server" without the path.
2. `thoughts/shared/research/2026-10-17_generalise-release-maker.md` and
   the two companion docs stay (they describe the design); the
   releases-doorserver plans from 2026-08-24 stay only if they pass the grep.
3. Create `spotUP/makerelease` public with `gh repo create spotUP/makerelease
   --public --description "..."`. Push a single orphan commit:

   ```sh
   git checkout --orphan public
   git add app RULES.md CLAUDE.md README.md LICENSE .gitignore thoughts/shared/plans thoughts/shared/research
   git commit -m "makerelease: release archive maker for demoscene groups"
   git remote add public git@github.com:spotUP/makerelease.git
   git push public public:main
   ```

   This touches no existing branch. The private repo keeps its history; its
   `main` is not rewritten. `git add` lists paths by name (no `-A`).
4. Rename nothing in the private repo; its GitHub description gets
   "(private history; public at spotUP/makerelease)".
5. Verify the public tree: clone fresh to the scratchpad, `npm install &&
   npm run typecheck && npm test`, and the scrub grep returns 0 hits.

Manual (spot): confirm the GitHub account is `spotUP` before the push
(`gh auth status`); confirm the private repo description edit.

## Implementation notes (deviations from the plan as written)

Recorded during implement; each was checked against the decisions above.

- Phase 0: the golden fixtures are produced with `vite-node`, not `tsx`
  (already a dependency through vitest).
- Phase 1: `config.generated.ts` stays git-ignored rather than deleted,
  because it may still hold a real key on a maintainer's machine; the plan's
  `config.generated` grep excludes that file. The legacy `UP-*` ignore lines
  stay for older leftovers. `registerIpcHandlers` takes the profiles folder,
  an output-root function and the environment. `ProfileSummary` gained `url`.
- Phase 2: `division` is optional in the form and set by main from the
  profile. The backend interface gained `publishFailure` and
  `deleteFailure` for the window's messages. The doorserver tests stay in
  `ipc-handlers.spec.ts`. `uploadToAminet` takes the group's details from
  the profile. The release date moved to section 2 for every renderer;
  section headings are numbered by `numberHeadings` so hidden sections
  leave no gaps.
- Phase 3: `release.nfo` is optional (spot, mid-implement): without it the
  build writes no NFO and the window says so. A fixed-width field is the
  whole marker's width, braces included. Template problems show in
  `#template-status` with their line numbers. The example NFO uses all 15
  placeholders. The reach sentinel is `SENTINEL-7F3A {{group}}`.
  `readForm` was extracted in the renderer so preview and build read the
  same form.
- Phase 5: the scrub grep writes each word with a character class
  (`[h]ippoplayer`) so this plan does not match itself. The 2026-08-24
  plans were rewritten to say "the server" and "copy" instead of host paths
  and SCP. `thoughts/shared/handoffs/` holds host names, addresses and
  backup paths, so it stays in the private repo and is left out of the
  public commit.

## Automated verification summary

```sh
cd app && npm run typecheck && npm test
```

Per feature reachability specs: `reach-profiles.spec.ts` (phase 1),
`reach-backend-none.spec.ts` (phase 2), `reach-placeholder.spec.ts` (phase 3).
Parity: `ur-golden.spec.ts`, `x-golden.spec.ts` green at the end of every
phase. Regression: `$&` case in `nfo-x-innovations.spec.ts`, `$&` case in
`placeholder-template.spec.ts`.

## Manual verification checklist (spot, LOCAL unless stated)

1. Phase 1: profiles dropdown, Open folder, Reload, Up Rough seed loads with
   the key in `profile.json`.
2. Phase 2: section hiding per profile; Up Rough dry run still lands in
   `releases-out/uprough/dry-run/UP-<CODE>/`.
3. Phase 3: example dry run output; template error reporting; Aminet section
   after a real build.
4. Phase 4: product name in window and built app.
5. Phase 5: public clone builds and tests green; private repo untouched.

## Out of scope

- Server changes (releases-doorserver keeps its API).
- Writing member edits back to `profile.json`.
- Directive language, wrapping or alignment rules beyond the two width rules.
- A local catalogue for generic groups.
