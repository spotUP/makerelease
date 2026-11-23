# AmiSSL 6.1 setup (2026-11-22)

Source: gh release 6.1 of jens-maus/amissl (AmiSSL-6.1-OS3.lha, AmiSSL-6.1-SDK.lha). Note: current release is 6.1, not 5.x (OpenSSL 4.1.0, lib name amissl_v41x.library; the SDK README says v5/v6 API is the same).

## Rig 3 (running, no reboot)
- System: is DH0:. Assign AMISSL: already existed = System:AmiSSL (LIBS: already searches System:AmiSSL/Libs).
- Old: amisslmaster.library 5.27 + amissl_v362.library 5.27 (kept; master backed up to DH0:AmiSSL/Libs/amisslmaster.library.v527).
- New: DH0:AmiSSL/Libs/amisslmaster.library 6.1, DH0:AmiSSL/Libs/AmiSSL/amissl_v41x.library 6.1 (68020-40 build).
- Not copied: the 118 new Certs (rig has 290 in DH0:AmiSSL/Certs); nothing copied into LIBS: (not needed, path via assign).
- Verified: Version of both new files = 6.1; RAM:OpenSSL (the release's C/AmigaOS3/OpenSSL, put in RAM:) prints "OpenSSL 4.1.0 23 Oct 2026" with the new libs, so master+v41x load.
- UNVERIFIED: live TLS to doors.uprough.net:443. bsdsocket.library 4.1 present, no ping/wget/curl in C:. "OpenSSL s_client -connect ... -brief" exited rc 1 with no output (clib2 stderr not captured by redirect); without -brief it hung until the 60 s timeout. Cause unknown (DNS, connect or tool), not diagnosed.
- Leftover: RAM:OpenSSL, RAM:req, RAM:out, RAM:err on the rig (RAM only).

## SDK for the cross build
Copied (uncommitted) to amiga/vendor/amissl-sdk/{include,lib,README-SDK}. m68k-amigaos-gcc is NOT on PATH here, so no compile was tried.
- Include dir: amiga/vendor/amissl-sdk/include (use <proto/amissl.h>, <proto/amisslmaster.h>, <libraries/amissl.h>, <amissl/amissl.h>)
- Link: amiga/vendor/amissl-sdk/lib/AmigaOS3/libamisslauto.a (auto-opens master+lib, pass -lamisslauto with -L.../lib/AmigaOS3) or libamisslstubs.a for manual open. Source autoinit: lib/autoinit_amissl_main.c.
- clib2 vs libnix: the release libs are built -mcrt=clib2; check README-SDK sections 5 and 9 before choosing the crt flag.
