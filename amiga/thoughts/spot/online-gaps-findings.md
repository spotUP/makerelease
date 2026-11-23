# Online gaps closed (JSON writer, multipart, door repo, cfg filler)

New: src/json_write.[ch] (jw_str/jw_key, JSON.stringify escaping, UTF-8 bytes pass through), src/multipart.[ch] (undici FormData bytes),
src/door_repo_client.[ch] (rp_door_submit, same rp_http_fn seam), additions in src/releases_client.[ch]:
rp_trim_diz, rp_rel_form + rp_rel_publish_body (toPublishBody, same key order, division/boards omitted when NULL),
rp_rel_publish_files (archive, image, embedImage* parts), rp_rel_cfg_for (clientConfigFor; env override wins when non-empty).
Test: tests/test_online_writers.c (ctest `online_writers`), expected strings generated from node 24 (JSON.stringify of the TS body shape, undici FormData via Request), boundary seed 14997584896.

Limits / notes:
- Bodies and archives sit fully in memory (archive cap 8 MB on the door server); fine for the real transport only if it takes body+len.
- Strings are written as bytes: Amiga Latin-1 text would go out as invalid UTF-8; convert Latin-1 -> UTF-8 in the UI layer before building the form (not done).
- JSON.stringify also escapes lone UTF-16 surrogates; not reachable from byte strings.
- Boundary randomness is the caller's seed (rp_mp_boundary); pass e.g. a timer/clock value.
- Door submit takes filename + data pointer; reading the file is the UI's job.
- ui_amiga.c untouched: nothing calls these yet (wiring is the UI task).
