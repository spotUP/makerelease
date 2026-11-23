# Desktop wizard vs Amiga packer: feature gaps (audit 2026-11-22)

Amiga today: one MUI window (ui_amiga.c): group cycle, release name, code, platform cycle, description, format cycle, file picker, Build, Preview, flatten nested folder, preview list. Members and group data come from PROGDIR:groups (groups.c). NFO renderers exist (nfo_uprough.c, nfo_x.c) but are fed hardcoded empties (ui_amiga.c:344-346: notes="", news="", history number "?").
Desktop lines are in app/src/renderer/{index.html,main.ts} and app/src/main/*.ts. Line numbers marked "html" are index.html.

| feature | desktop file:line | Amiga status | size | network/TLS |
|---|---|---|---|---|
| Release name field | index.html:36, main.ts:171 | present | - | no |
| Code field + auto-suggest from name | main.ts:36,171 (shared/code-suggest.js) | partial (manual entry only, no suggest, no uppercase-as-you-type, no maxLength from profile) | S | no |
| Code validation: length, exists-on-server check | main.ts:769-781, ipc checkCodeAvailability, releases-client.ts:141 | missing (ui_amiga.c:254 only upper-cases) | M | yes (server check); length check alone S, no network |
| Code autocomplete / prefill | main.ts:171 (codePrefill), prefill.ts | missing | S | no |
| Platform tag (select + free-text Other) | index.html:46-54, main.ts:48-56 | partial (4 fixed tags, ui_amiga.c:545/g_platform_tags; no free text) | S | no |
| Release date + "released at" fields | index.html:58-62 | partial (date = today, ui_amiga.c:254; no edit, no "released at") | S | no |
| Description line | index.html:66 | present | - | no |
| NFO notes textarea | index.html:70-75, main.ts release-notes-text | missing (notes="" at ui_amiga.c:344; needs multiline MUI editor) | M | no |
| NFO live preview | index.html:77-83 | missing (Preview shows file list only) | M | no |
| News textarea | index.html:113-116, main.ts:71 | missing (news="" at ui_amiga.c:346) | S after notes editor exists | no |
| Members list edit (add/remove, creators autocomplete) | memberlist-table.ts, creators-autocomplete.ts, index.html:55-63 | partial (read-only from group file, ui_amiga.c:264-271; no edit) | M | no |
| Credits table (per-release credits, creators preview) | credits-table.ts, credits-creators.ts, index.html:94-107 | missing | M | no |
| Release number (server-allocated / manual) | index.html:67-72, main.ts updateReleaseNumberPreview | missing (history number hardcoded "?") | M; allocation needs server | yes for server numbering; manual entry no |
| History entry (category, title, filenames) | index.html:74-92, main.ts:84,657 | partial (rp_history built with empty category/creators, ui_amiga.c:333-340; no UI, no history of previous releases) | M | seed of prior history needs server (fetchSeed, releases-client.ts:112) |
| DIZ banner preview + release-line edit | index.html:126-132, main.ts:188-228 | partial (DIZ rendered from template, ui_amiga.c:258-376, packed in archive; no preview, no line editing) | M | no |
| /X Innovations fields (system, boards table, credits) | index.html:134-150, boards-table.ts, nfo-x-innovations.ts | partial (nfo_x.c renders; no boards/system UI, fed from group data only) | M | no |
| File picker (files/folders) | index.html:155-158, ipc pickFilesAndDirs:157 | present (Pick...) | - | no |
| Nested-folder warning + flatten | index.html:158 | present (flatten_btn) | - | no |
| Archive contents preview | index.html:162-163, archive-preview.ts | present (preview_list, archive_preview.c) | - | no |
| Archive format LHA/ZIP | index.html:167-172 | present (format cycle) | - | no |
| Build (LHA/ZIP, NFO, DIZ staging) | index.html:175, archiver.ts, staging.ts | present (stage.c, archive_run.c) | - | no |
| Build progress bar | index.html:177-180 | missing (status text only) | S | no |
| Dry-run preview build | index.html:176, ipc buildRelease dryRun:173 | partial (Preview lists, does not build locally) | S | no |
| Persist settings between runs (format, Aminet fields, last group) | main.ts:731-745 localStorage | missing | S | no |
| Stale draft / numbering warnings | stale-drafts.ts, main.ts:695 | missing | M | yes (needs server state) |
| Media embeds (YouTube / image URLs in post) | index.html:120-124, media-embeds-table.ts | missing | M | no for entry; only meaningful with publish (yes) |
| Cover image pick + upload | index.html:190-191, ipc pickImage:263, image-thumb.ts | missing | L (IFF/PNG handling, thumbnail, upload) | yes (HTTPS upload) |
| Publish to group website | index.html:188-196, main.ts:1008, releases-client.ts:167,204, backend-doorserver.ts | missing | L | yes, TLS (AmiSSL / bsdsocket) |
| Aminet upload (short, type, arch, uploader, readme preview, problems) | index.html:199-233, aminet.ts:48 | missing | L | FTP to ftp://main.aminet.net/new/ (aminet.ts:15), no TLS but needs TCP stack (bsdsocket); readme text normalization aminet.ts:36 is S |
| Aminet type guess from category/platform | main.ts:422 guessAminetType | missing | S | no |
| Revoke a release | index.html:237-246, releases-client.ts:185, ipc deleteRelease:302, revoke-message.ts | missing | M | yes, TLS |
| Group/profile switching | listProfiles ipc:90 | partial (group cycle over PROGDIR:groups files) | - | no |
| Profile editor (label, id, url, prefix, code length, renderer, templates, import) | profile-editor.ts, index.html:249-, ipc:102-138, profiles.ts | missing (groups are hand-edited files) | L | no |
| Field error highlighting | field-errors.ts | partial (single status line) | S | no |
| Toasts | toast.ts | missing (status line only) | S | no |
| Wizard paging / step nav | wizard.ts | not applicable / single page by design | - | no |

Notes
- Everything offline (notes, news, members edit, credits, history UI, DIZ preview, NFO preview, code length check, persistence) is reachable without a network stack; the renderers already take these inputs.
- Network features all share one prerequisite: an HTTP(S) client on Amiga (TLS via AmiSSL or a proxy). Aminet is plain FTP.
- Not verified: backend-doorserver.ts internal protocol, and which fields each group renderer actually requires.
