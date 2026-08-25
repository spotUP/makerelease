---
date: 2026-08-25
topic: makerelease — cross-platform archiver and output-root fixes, rebuild, redistribution
tags: [makerelease, electron, cross-platform, linux, appimage, distribution, bugfix]
status: implemented
---

# Cross-platform archiver + output-root fixes

Follows on from `2026-08-24_releases-pipeline-and-distribution.md`. That
document is still accurate about the **server and deploy** side; its
**distribution** section is superseded here — the binaries it describes
had two bugs that made the app unusable off macOS.

## Task(s)

Triggered by a member bug report (Varin0x, Linux) rather than planned work:

> Build failed: Error invoking remote method 'buildRelease': Error: spawn
> /opt/homebrew/bin/lha ENOENT

and, for ZIP:

> Command failed: /usr/bin/zip -r /tmp/.mount_Up Rou7zSVsB/resources/UP-EVK26.ZIP …

- Diagnosed two independent root causes behind those two messages. **Done.**
- Fixed both (`294937c`). **Done.**
- Reviewed that fix, found two defects **in the fix itself**, corrected them
  (`acb8645`). **Done.**
- Rebuilt all three platforms, restaged distribution zips with per-platform
  install notes. **Done.**

Nothing is mid-flight.

## Critical References

- **Commits**: `294937c` (the two fixes), `acb8645` (review corrections + tests).
- `app/src/main/archiver.ts` — binary resolution and the ENOENT message.
- `app/src/main/output-root.ts` — **new**; packaged-build output root with
  fallback. Deliberately takes a `getPath` callback so it is testable
  without booting Electron.
- `app/src/main/index.ts` — startup wiring; output root is resolved inside
  `whenReady`, never at module scope (see Learnings).
- `app/test/unit/archiver.spec.ts`, `app/test/unit/output-root.spec.ts` — **new**.
- Distribution zips: `~/Desktop/Up Rough Release Maker - {macOS,Windows,Linux}.zip`,
  each containing that platform's installers plus an `INSTALL.txt`.

## Recent Changes

### Bug 1 — archiver binaries were hardcoded to macOS paths

`archiver.ts` held `const LHA_BIN = '/opt/homebrew/bin/lha'` and
`const ZIP_BIN = '/usr/bin/zip'`. Absolute, macOS-Homebrew-specific, checked
as literal paths — which is why the member symlinking `lha` into
`/opt/homebrew/` changed nothing.

Now spawns bare `lha` / `zip` and lets the OS resolve them from `PATH`.
`PATH` is augmented with common per-OS install dirs first, because
GUI-launched apps (Finder/Dock, AppImage, desktop entries) inherit a
minimal environment that a login shell would not. On `ENOENT` the error is
rewritten to name the missing binary and give a per-platform install hint,
instead of surfacing a raw spawn error.

### Bug 2 — output root resolved inside the packaged app bundle

`MAKERELEASE_ROOT` was `path.resolve(__dirname, '..','..','..','..')`. In dev
that reaches the repo root. In a **packaged** build `__dirname` sits inside
the app's own read-only bundle, so the walk lands in the install location —
for an AppImage, its temporary `/tmp/.mount_*` squashfs mount, which is
read-only and unmounts on exit. That is the odd path in the member's second
error; the build was trying to write its output there.

Packaged builds now use `~/Documents/Up Rough Releases`, created at startup.
Dev behaviour is unchanged. **This affected macOS and Windows too**, not just
Linux — the packaged mac app was writing into its own `.app/Contents/Resources`.

### The review found two defects in that fix

Worth recording because the fix shipped before the review, and the review
was not a formality:

1. **The fix made the reported platform worse.** `app.getPath('documents')`
   was called at module top level. `getPath` *throws* when a location cannot
   be resolved (verified against the real Electron binary: `Failed to get
   'recent' path`), and `documents` depends on XDG user dirs — precisely what
   a minimal Linux install lacks. A throw there happens before any window
   exists, so the app would have died on launch with no window and no error
   dialog: a silent startup crash traded for a legible build error.
   Resolution now falls back `documents -> home -> userData`, runs inside
   `whenReady`, and reports failure via `dialog.showErrorBox` before quitting.
2. **No tests.** `294937c` violated the standing rule that every bug fix
   ships with a regression test. Both fixes now have coverage, verified the
   way the rule requires — by reverting to the broken code and confirming
   failure: `archiver.spec.ts` 5-of-6 fail pre-fix (the 6th is the negative
   case, that a genuine archiver failure still propagates rather than being
   misreported as a PATH problem); `output-root.spec.ts` 4-of-5 fail against
   single-lookup behaviour.

67/67 tests pass, typecheck clean.

## Learnings / gotchas worth remembering

- **`execFile` DOES honour a custom `env.PATH` for binary resolution.** This
  is a common Node gotcha and worth not trusting from memory — it was probed
  directly before relying on it. libuv replaces `environ` before `execvp`, so
  the lookup uses the PATH you passed.
- **`execFile` with array args never goes through a shell.** The space in
  `/tmp/.mount_Up Rou7zSVsB/...` looked like a quoting bug and was not — it
  was cosmetic in the error string. The real defect was the output root.
  Do not "fix" quoting here; there is nothing to quote.
- **`app.getPath()` throws rather than returning empty.** Never call it at
  module scope. Anything that throws before `whenReady` kills the app with
  no window and no dialog — the least debuggable failure mode there is.
- **A packaged Electron app must never derive writable paths from
  `__dirname`.** Use `app.getPath(...)`, and branch on `app.isPackaged`.
- **Misleading exit codes, again.** The build wrapper ended in an `echo`, so
  the harness reported exit 0 regardless. The real status came from an
  explicit `EXIT=$?` written into the log. Same lesson as the previous
  session — check the recorded code, not the wrapper's.
- **Verify the fix reached the artifact.** After building, the shipped
  `app.asar` was grepped to confirm the new PATH code was present, the old
  `/opt/homebrew/bin/lha` string was gone, and the write key was baked in.
  A green test run says nothing about what electron-builder packaged.
- **`dist/` size is the canary for the output-collision bug** (see the
  previous handoff). 172K is correct; a bloated `dist/` means the packaged
  output is being recursively re-bundled.

## Artifacts

- Three distribution zips on the Desktop, each with an `INSTALL.txt` naming
  the required tools, the unsigned-build warning workaround, and the new
  output location.
- Build log convention: `npx electron-builder -mwl`, with an explicit
  `EXIT=$?` appended to the log.

## Next Steps (not done, in rough priority order)

1. **Members already have the broken build and must re-download.** The
   previous zips were distributed before these bugs were known. Anyone who
   already installed it has a version that cannot build on Linux or Windows
   at all.
2. **Windows cannot pack archives out of the box, and this is unsolved.**
   Windows ships neither `lha` nor `zip`, and 7-Zip is not a drop-in (it does
   not take the same arguments). The app now fails with a clear message
   naming the missing binary, but it still fails. The real fix is bundling
   `lha.exe` and `zip.exe` in the Windows package via electron-builder's
   `extraResources` and preferring the bundled copy — that has NOT been done.
   Until then the Windows `INSTALL.txt` points members at macOS or Linux.
3. `/Applications/Up Rough Release Maker.app` on the dev machine is still the
   old buggy build. The user is handling this replacement themselves.
4. **Code signing / notarization still not done** (carried over unchanged
   from the previous handoff). All builds unsigned; Gatekeeper and SmartScreen
   both warn. Members need the right-click-Open / "Run anyway" workaround.
5. The `EXTRA_PATH_DIRS` entries chosen for Linux (`/usr/local/bin`,
   `/usr/bin`, `/bin`) are almost certainly already on `PATH` and so are
   near no-ops — the actual Linux fix was using `PATH` at all. If a member
   reports a missing binary that is installed, `~/.local/bin` and the
   Nix/Snap paths are the obvious additions.

## Other Notes

- The server side was not touched this session. `releases-doorserver` in
  production is unchanged and still holds the 216 real releases; see the
  previous handoff and `releases-doorserver/DEPLOYMENT.md`.
- The distribution zips are built with the `members-distribution` write key
  baked in, per the previous handoff's convention — not the `makerelease-app`
  testing key.
