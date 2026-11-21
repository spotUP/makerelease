# makerelease: developer notes

User documentation is in the [root README](../README.md). This page is for
working on the app itself.

## Commands

All commands run from `app/`.

```sh
npm install
npm start            # build main, preload and renderer, then launch Electron
npm run typecheck    # main, preload and renderer projects
npm test             # vitest run, suites under test/unit/
npx vitest run test/unit/<name>.spec.ts
npm run dist         # electron-builder for this system
npm run dist:private # two-distribution private build: mac, Windows, Linux
```

## Two distributions

`npm run dist` builds the public app: first launch seeds the example group,
and anyone adds their own groups through the Manage groups menu.

`npm run dist:private` builds the private app for our own groups: it bundles
`app/private-profiles/` (git-ignored; copy the group folders there, key
included, never the example) and first launch seeds those instead. The script
refuses to build when the folder is missing, empty, holds `example`, or holds
a doorserver group without a real write key. Seeding copies missing ids only,
so reinstalling never overwrites a group the user edited.
`PRIVATE_PROFILES_DIR=<dir> npm start` previews private seeding in a dev run.

## Layout

- `src/main/` Electron main process: profiles, build, staging, archiving,
  backends, Aminet upload.
- `src/renderer/` the form and its previews.
- `src/shared/` code both sides use: profile parsing, the placeholder
  template language, naming, the DIZ release line, the Aminet readme.
- `test/unit/` vitest suites. `test/fixtures/` byte-exact NFO and DIZ files.

## How a build flows

1. `profiles.ts` loads `profile.json` and the templates from the user data
   folder.
2. `backendFor` in `backend.ts` picks the backend named in the profile:
   `none` builds on disk only, `releases-doorserver` numbers and publishes
   through a server.
3. `generateOutputs` in `nfo-generator.ts` dispatches on the profile's
   renderer: `placeholder` fills the group's templates, `uprough` and
   `xinnovations` draw the two built-in layouts.
4. `staging.ts` lays out the release folder and `archiver.ts` runs `lha` or
   `zip`.

Each path has a reachability test (`test/unit/reach-*.spec.ts`) that drives
the real IPC handlers.

## Golden output

The `uprough` and `xinnovations` renderers must keep producing the same
bytes. `test/unit/ur-golden.spec.ts` and `test/unit/x-golden.spec.ts` compare
them with fixtures. Update a fixture only from a measured source, never by
retyping it.

## Up Rough and /X Innovations profiles

These groups use `"backend": "releases-doorserver"` with a `server` entry:

```json
"server": { "baseUrl": "https://releases.uprough.net", "writeKey": "<key>" }
```

Builds carry no key; the key lives in the profile on the machine that
publishes. Two environment variables override the profile for local work
against another server:

| Variable | Overrides |
|---|---|
| `RELEASES_API_BASE_URL` | `server.baseUrl` |
| `RELEASES_WRITE_KEY` | `server.writeKey` |

Without a valid key the server rejects a publish with 401, and the window
says so on startup. The server is the separate `releases-doorserver`
repository, which documents its own deployment.

## Aminet upload

The upload runs `curl` against `ftp://main.aminet.net/new`. There is no
sandbox: an upload is public once a moderator approves it, so never exercise
it outside a real release. Tests replace `curl`.
