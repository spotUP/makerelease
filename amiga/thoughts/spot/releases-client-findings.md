# Releases client (C) vs app/src/main/releases-client.ts

Files: src/http.h (rp_http_fn seam, rp_http_amiga stub decl under __amigaos__), src/releases_client.[ch], tests/test_releases_client.c (ctest `releases_client`, fake transport).
Reused: json.c parser (responses), CMake packer lib + suite loop. strbuf/naming not needed (no JSON writer exists; bodies are passed in by the caller).

Gaps vs TS:
- (closed, see online-gaps-findings.md) toPublishBody, multipart, door repo, cfg filler.
- No multipart builder: rp_rel_publish takes a finished body + content type (archive/image/embedImage parts).
- door-repo-client.ts (submitDoor, doors.uprough.net) not ported; needs the same multipart builder.
- No clientConfigFor / env override; caller fills rp_rel_cfg.
- Create errors also carry status (TS omits it on create); harmless.
- Error text is truncated to 255 bytes; URL to 599.
- Real transport (bsdsocket + AmiSSL) not written; rp_http_amiga is declared only.
