# Release Packer

An Amiga (M68k, AmigaOS 3.x, MUI 3.8) release packer for the demoscene.
It takes your group's templates and your release files and packs a finished,
upload-ready scene release: the archive, the NFO, the `FILE_ID.DIZ` and the
Aminet `.readme`.

Sibling of `makerelease` (the desktop release baker): same pack logic, same
templates, native Amiga UI. A `FILE_ID.DIZ` and NFO built here are
byte-identical to the ones the desktop build makes from the same input.

## What a packed release looks like

```
UP-XXXX.LHA                  <- the archive (or .ZIP)
  FILE_ID.DIZ                <- one-line BBS/DIZ description, ANSI frame intact
  UP-XXXX/                   <- folder named after the release
    UP-XXXX.NFO              <- the full NFO: notes, members, release history
    <your files>             <- the demo, intro, musicdisk, ...
UP-XXXX.readme               <- Aminet description file, next to the archive
```

## Templates

Each group is a drawer of template files, edited with any text editor. The
drawer is shared byte for byte with the desktop release baker: copy a
`profiles/<id>` drawer straight across and it works here, key included:

```
groups/
  uprough/
    profile.json             (name, URL, file prefix, code length, renderer, backend)
    release.nfo              (optional: without it the archive carries no NFO)
    file_id.diz              (required)
```

Both files are read as Latin-1, so ANSI and Amiga drawing characters stay
single bytes. A `{{word}}` in a template is a placeholder:

| Placeholder       | Value                                              |
|---|---|
| `{{group}}`       | group name from the profile                        |
| `{{url}}`         | group website                                      |
| `{{code}}`        | release code, upper case                           |
| `{{filename}}`    | archive name, e.g. `UP-XXXX.LHA`                   |
| `{{format}}`      | `LHA` or `ZIP`                                     |
| `{{release_name}}`| release name                                       |
| `{{platform}}`    | platform tag, e.g. `A500`, `A1200`                 |
| `{{date}}`        | release date                                       |
| `{{description}}` | the one-line description                           |
| `{{creators}}`    | names from the credits                             |
| `{{notes}}`       | release notes, one line each                       |
| `{{news}}`        | news, one line each                                |
| `{{members}}`     | members, roles lined up in one column              |
| `{{credits}}`     | credits as `Role: Name`                            |

Rules, same as the desktop build:

- **Plain text.** `{{name}}` is replaced by its value as it is.
- **Fixed width.** Pad with spaces before the closing braces, as in
  `{{group        }}`. The value is padded to the width of the whole marker,
  braces included, so ASCII art to the right keeps its columns. A value longer
  than the marker is an error, never a broken frame.
- **Lines.** A lines placeholder stands alone on its line; the frame around it
  repeats for every value line. Empty value, the line is removed.
- Any other `{{word}}` is an error, so a misspelt placeholder is never
  printed by accident.

Up Rough and /X Innovations have built-in layouts instead of templates:
Up Rough's plain 80-column NFO and /X's framed drawing, drawn by the same
code that draws them on desktop.

## Packing

The packer shells out to the archivers, so they must be in `PATH`:

| Format | Tool  |
|---|---|
| LHA | `lhA` (Aminet `util/arc/lha.run`) |
| ZIP | `zip` (Info-ZIP Amiga port) |

A build writes the archive and the unpacked release folder into
`PROGDIR:releases/`, named after the release. Pick the files drawer first
with the Pick button (or build NFO + `FILE_ID.DIZ` only with no drawer
picked). A build whose archive or folder already exists in `PROGDIR:releases`
stops instead of overwriting — move the old one away first.

After a build, the Aminet drawer holds the archive plus a matching
`.readme`, built from the short description (40 characters at most), the
Aminet directory (e.g. `demo/aga`), the architecture, and your name and
email. Uploading is manual in this version: send the two files to
`ftp://main.aminet.net/new` with any FTP client. There is no test server:
an upload is public once a moderator approves it.

## Interface

MUI 3.8, built on our own classes and our own dark look. The classes come
from https://github.com/spotUP/up-mcc ("MUI custom class package for the
Amiga: eleven `UP-<Name>.mcc` classes ..., the `UP-MCC.mcp` preferences
class, the `up-mcc.library` support library, themes and a demo"; its CMake
target `up_mcc_dist` stages the built `UP-MCC/` directory). `vendor/up-mcc`
is that repo as a git submodule; the build uses its `include/` (clone with
`--recurse-submodules`, or run `git submodule update --init`):

- UP-MCC: UP-Button, UP-Text, UP-Panel, UP-Tabs, UP-Balance, UP-Slider,
  UP-Progress, UP-Cycle, UP-Field, UP-Scroller, UP-List, plus the UP-MCC
  preferences class and `up-mcc.library`.
- Dark theme by default (light theme included), Inter as screen font,
  anti-aliased text when `ttengine.library` is installed, plain text
  without it.
- Its own public screen with an exact-pen palette, so the dark greys are
  real greys instead of MUI's nearest-pen black. Falls back to the
  Workbench screen when the screen cannot open.

Known upstream bug: `UP-Cycle.mcc` hard-freezes when created without
`UPA_Cycle_Entries`. Always pass a NULL-terminated entry array that outlives
the object. The up-mcc repo does not document or fix this (checked its
README, `docs/up.autodoc` and `dist/`).

Give the program at least 48 KB of stack (MUI 3.8 requirement for custom
classes; the icon carries it).

## Building

Needs bebbo's amiga-gcc cross compiler with the MUI 3.8 SDK installed
(`SDK=mui` when building the toolchain), CMake and Ninja. The configure
step fails with the fix when the toolchain or the MUI SDK is missing.

```sh
cmake -S . -B build/amiga -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/amiga-68020.cmake
cmake --build build/amiga
```

The 68k binary also runs under `vamos` for roundtrip tests on the build
machine, and is verified on the FS-UAE rig and the real A1200 before a
release, same flow as MusicPlayer.

### UP-MCC class set for the rig

The classes come from the `vendor/up-mcc` submodule, not from MusicPlayer.
Cross build them with the same toolchain file:

```sh
cmake -S vendor/up-mcc -B build/up-mcc -G Ninja -DSEL_AMIGA=ON \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/amiga-68020.cmake
~/.claude/bin/buildq --purpose "<session>: <task>" -- nice -n 19 cmake --build build/up-mcc --target up_mcc_dist -- -j3
```

Install `build/up-mcc/dist/UP-MCC/Libs/mui/*.mcc` (and `up-mcc.library`) on
the rig in place of the MusicPlayer set. Verified to build; the rig install
itself is not scripted.

## Scope

This version packs on disk only: NFO, `FILE_ID.DIZ`, archive, Aminet
`.readme`. Release numbering and publishing through the Up Rough release
server, and direct Aminet FTP upload from the app, are later phases, not
this one.

## Licence

MIT.
