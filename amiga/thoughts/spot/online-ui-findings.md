# Online features in the UI (Publish, Revoke, door repository)

New: src/online.[ch] (host-testable: Latin-1 to UTF-8, rp_online_publish / rp_online_revoke / rp_online_door), tests/test_online.c (ctest `online`, fake transport).
Edited in place: src/ui_amiga.c (new "Online" card, 4 buttons, handlers), CMakeLists.txt (online.c in packer lib, `online` suite, packer-cli links http_amiga).

Wiring, end to end:
- "Pick cover..." -> ID_COVER -> shell_pick_cover (ASL file requester) -> g_cover, shown in the Cover row.
- "Publish" -> ID_PUBLISH -> shell_online -> rp_online_publish -> rp_rel_cfg_for (group server_base/server_key from profile.json via rp_load_group) -> rp_rel_publish_body + rp_rel_create -> rp_rel_publish_files (archive PROGDIR:releases/<BASE>.LHA|ZIP, image g_cover) -> rp_http_amiga -> status line "Published: <post url> (archive: <url>)" or "Not published - <server message>" ("Already published: <url>" on 409 with a post url).
- "Revoke" -> ID_REVOKE -> EasyRequest confirm (Revoke|Cancel) -> rp_online_revoke (code field upper-cased) -> rp_rel_delete -> status (desktop describeRevoke wording).
- "Submit to door repository" -> ID_DOOR -> rp_online_door -> rp_door_submit (/X Innovations only) -> status with the server's own wording.
- All ids are matched in the main loop of shell_main before the preview refresh; all messages go to the existing status line.

Decisions / differences from the desktop:
- Desktop creates the catalog entry during Build and publishes later; here Publish does create then publish in one press (as asked). A failed publish after a successful create leaves a draft on the server: Revoke it.
- Desktop submits to the door repository automatically after Build for /X; here it is a button (no network inside Build). The button refuses non-/X groups.
- Inputs: name/code/platform/description/format from the form, members from the group file, DIZ read back from PROGDIR:releases/<BASE>/file_id.diz (the staged drawer Build leaves), archive from PROGDIR:releases/. Publish therefore needs a prior Build with the same code and format. Release notes, news, category, creators, credits, embeds are empty (those fields do not exist in the Amiga UI yet).
- Server URL / write key: the group's profile.json (server_base, server_key, backend_doorserver); no env override on the Amiga. Missing key: "The <id> profile has no server write key".
- Latin-1 -> UTF-8 for code, name, platform, description, DIZ and member nicknames/roles before the JSON body is built. Cover file name and archive name are ASCII Amiga names, sent as is.

Limits:
- Synchronous: status set to "Working...", one MUI input pump to draw it, then the window is blocked until the transport returns (no cancel, no progress; large archives freeze the window for the upload). A click made during that pump may be swallowed.
- Archive and cover are read fully into memory.
- UI itself (buttons, layout in 640x256 scroll group, requester) is unverified on the rig; only online.c is host-tested.

## Verification state (end of agent run)
- Host build compiled clean (online.c, test_online.c, writers). ctest: online_writers pass; online failed one check (revoke wasDraft case: test forgot reset() of the scripted transport). Test fixed; rebuild queued in buildq (amiga session: online ui wiring, host), result not seen.
- build/amiga (ui_amiga.c, http_amiga link) NOT compiled: queue was 5+ builds deep. Run `buildq ... cmake --build build/amiga` first thing; watch the rp_http_amiga declaration (under __amigaos__) and the EasyRequest call in shell_confirm_revoke.
