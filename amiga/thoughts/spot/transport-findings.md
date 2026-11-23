# HTTP(S) transport (rp_http_amiga), 2026-11-23

- src/http_amiga.c: bsdsocket (Roadshow API, proto/bsdsocket.h), AmiSSL opened by hand (amisslmaster + OpenAmiSSLTags, no libamisslauto, so no auto-exit when the lib is missing), SNI, SSL_VERIFY_PEER, hostname check via X509_VERIFY_PARAM_set1_host, trust from AMISSL:Certs (fallback default paths). HTTP/1.1, Connection: close, reads to EOF, then Content-Length / chunked parse. 30 s SO_RCVTIMEO/SO_SNDTIMEO. Plain http:// supported. 8 MB response cap.
- src/net_probe.c + targets http_amiga (static lib) and net-probe in CMakeLists.txt (cross build only): `net-probe URL`.
- crt question: builds and links with the project's -mcrt=nix20 (libnix); libamisslauto not needed, AmiSSL ErrNoPtr gets libnix's &errno. No clib2 mismatch at link time. Runtime unverified.
- SDK has no SSL_set1_host stub: use X509_VERIFY_PARAM_set1_host(SSL_get0_param(ssl), host, 0).
- Cosmetic: 4 -Wint-conversion warnings (pointer tag values in OpenAmiSSLTags macro); harmless on m68k, add (ULONG) casts.
- NOT tested live: rig 3 was DOWN (ami.py ping: connection refused; rig.py status: down). Not started, per instructions. Binary: build/amiga/net-probe; to run: UPTERM_RIG=3 ami.py put build/amiga/net-probe RAM:net-probe; exec "RAM:net-probe http://example.com/".
- Builds wait behind a long buildq queue (hours of waits seen); shared scratchpad files get deleted by other sessions, use a private dir.
