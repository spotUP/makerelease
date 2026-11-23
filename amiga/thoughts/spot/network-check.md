# Rig 3 network check (2026-11-22)

Evidence (UPTERM_RIG=3, ami.py exec, rig not touched):
- `Version bsdsocket.library` -> rc 0, `bsdsocket.library 4.1` (present).
- `Version amissl.library` -> rc 20, object not found. LIBS: holds only up-mcc, screennotify, ixnet, ixemul.
- DEVS:NetInterfaces and SYS:Storage/NetInterfaces: not found. No Roadshow/Miami/AmiTCP in C:.
- build/rig3/vtcon-rig.fs-uae line 7: `bsdsocket_library = 1`; rig.py sets it in every rig (lines ~1387, 1565).
- tools/rig/README.md ~221: "FS-UAE's bsdsocket gives the Amiga the Mac's sockets" (wasabid listens on 1234+N-1 this way).

Conclusion:
- TCP/IP: yes, via FS-UAE's built-in bsdsocket emulation (not a real stack). Sockets are the Mac's, so host network is reachable (outbound connect, DNS via gethostbyname expected; not tested with a live connect). No ethernet device config.
- TLS: no. AmiSSL is not installed on the rig.

To get bsdsocket + AmiSSL working:
- bsdsocket: nothing, already on. Only 68k bsdsocket API; no Roadshow-only calls (e.g. no interface/route queries).
- AmiSSL: install the AmiSSL 5.x 68k runtime (Aminet util/libs/AmiSSL-5.x, same as the 5.27 SDK in README line 212) into the rig: amissl.library, amisslmaster.library, AmiSSL/ dir (LIBS:AmiSSL/*.library), plus the AMISSL: assign (S:User-Startup). Needs 68020+ for the 68k libs; rig is 060, fine. Copy via the rig's VTC: drive (build/rig3/vtc) and add the assign in the rig's user_startup.py mechanism, then reboot the rig (not done here).
- Also needed: root CA bundle (AmiSSL ships certs in AmiSSL:Certs) and correct system date (rig shows 22-Nov-26, ok) for cert checks.
- Quick verification once installed: `Version amissl.library`, then a test program doing OpenAmiSSL + connect to a host HTTPS port.
