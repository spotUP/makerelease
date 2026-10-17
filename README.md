# makerelease

A desktop app that packs a demoscene release: it writes the NFO and the
`file_id.diz` from your group's own templates, packs everything into an LHA
or ZIP archive with the group's file name scheme, and can upload the archive
with a matching `.readme` to Aminet.

It runs on macOS, Windows and Linux.

## Install

Download a build from the releases page, or build it yourself (see
[Building from source](#building-from-source)).

Packing needs the archiver on your `PATH`:

| System | LHA | ZIP |
|---|---|---|
| macOS | `brew install lha` | ships with macOS |
| Linux | `sudo apt install lhasa` (or your distribution's package) | `sudo apt install zip` |
| Windows | install an `lha` binary and add it to `PATH` | install `zip` and add it to `PATH` |

The Aminet upload uses `curl`, which ships with macOS, Windows 10 and later,
and every Linux distribution.

## First launch

On first launch the app writes an example group profile and selects it.
Press **Open profiles folder** to see it. Each group is one folder:

```
profiles/
  example/
    profile.json
    release.nfo      (optional)
    file_id.diz
```

The profiles folder is in the app's user data folder:

| System | Profiles folder |
|---|---|
| macOS | `~/Library/Application Support/makerelease/profiles` |
| Windows | `%APPDATA%\makerelease\profiles` |
| Linux | `~/.config/makerelease/profiles` |

To add your group, copy the `example` folder, rename the copy (only `a-z`,
`0-9` and `-`), edit its files, and press **Reload profiles**. A profile with
a mistake still shows in the dropdown, with the file and field that need
fixing.

## profile.json

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

| Field | Required | Meaning |
|---|---|---|
| `label` | yes | Group name as printed. |
| `url` | yes | Group website, printed in the NFO and in the Aminet readme. |
| `filePrefix` | no | Put before the code in every file name: `EX-` and code `TEST1` give `EX-TEST1.LHA`. Empty is allowed. No spaces or `\ / : * ? " < > \|`. |
| `codeLength` | no | Length of the release code typed in the form, 1 to 32. Default 5. |
| `renderer` | yes | `placeholder` for your own templates. `uprough` and `xinnovations` are the built-in layouts of Up Rough and /X Innovations. |
| `backend` | no | `none` (default): the release is built on disk only. `releases-doorserver` is Up Rough's release server and needs a `server` entry. |
| `members` | no | The member list the form starts with. |

## Templates

`file_id.diz` is required. `release.nfo` is optional: without it the archive
carries no NFO. Both files are read as Latin-1, so ANSI and Amiga drawing
characters stay single bytes. Windows line endings are accepted.

The form shows the filled `file_id.diz` as you type, and lists any template
problem with its file and line number. A build with a template problem stops
before anything is written.

### Placeholders

| Placeholder | Kind | Value |
|---|---|---|
| `{{group}}` | text | `label` from `profile.json` |
| `{{url}}` | text | `url` from `profile.json` |
| `{{code}}` | text | The release code, upper case |
| `{{filename}}` | text | The archive name, for example `EX-TEST1.LHA` |
| `{{format}}` | text | `LHA` or `ZIP` |
| `{{release_name}}` | text | Release name from the form |
| `{{platform}}` | text | Platform tag from the form, for example `A500` |
| `{{date}}` | text | Release date from the form |
| `{{release_number}}` | text | Release number as typed in the form |
| `{{description}}` | text | The one-line description |
| `{{creators}}` | text | Names from the credits, for example `coder and pixelartist` |
| `{{notes}}` | lines | Release notes, one line each |
| `{{news}}` | lines | News, one line each |
| `{{members}}` | lines | Members, roles lined up in one column |
| `{{credits}}` | lines | Credits as `Role: Name` |

Rules:

- **Plain text.** `{{name}}` is replaced by its value as it is.
- **Fixed width.** Put spaces before the closing braces, as in
  `{{group        }}`. The value is padded with spaces to the width of the
  whole marker, braces included, so ASCII art to the right keeps its
  columns. A value longer than the marker is an error, not a broken frame.
- **Lines.** A lines placeholder stands alone on its line. The text before
  and after it may hold only spaces and drawing characters. That frame is
  repeated for every value line. When the value is empty, the whole line is
  removed. Lines placeholders can be fixed width too.
- Any other `{{word}}` is an error, so a misspelt placeholder is never
  printed by accident.

The example profile's `release.nfo` uses every placeholder, and its
`file_id.diz` shows fixed-width fields inside a frame.

## Output

A build writes the archive and the unpacked release folder:

| Build | Folder |
|---|---|
| Installed app | `Documents/<label> Releases/` (home folder if there is no Documents folder) |
| Run from source | `releases-out/<profile folder>/` in the repository |

A dry run goes to a `dry-run` folder inside, and never touches a server.
The archive holds `file_id.diz` next to a folder named after the release,
which holds the NFO and your files.

## Uploading to Aminet

After a build, the Aminet section uploads the archive and a matching
`.readme` to `ftp://main.aminet.net/new`. There is no test server: an upload
is public once an Aminet moderator approves it. Before you press Upload:

1. Fill in the short description (40 characters at most), the Aminet
   directory (for example `demo/aga`) and the architecture.
2. Fill in your name and email. Aminet uses the email as the FTP password
   and to ask about the upload.
3. Read the `.readme` preview. It is built from the description, release
   notes and credits, and written next to the archive, so a failed upload
   can be redone by hand.

An upload cannot be undone from the app. Ask the Aminet team to remove it,
or upload a corrected archive under the same file name.

## Building from source

Needs Node.js and npm.

```sh
cd app
npm install
npm start          # build and launch
npm test           # unit tests
npm run dist       # installable build for this system
```

Developer notes are in [`app/README.md`](app/README.md).

## Licence

MIT. See [`LICENSE`](LICENSE).
