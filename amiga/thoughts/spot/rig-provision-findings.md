# Rig 3 provisioning (2026-11-23)

Installed: DH0:Packer/{packer-cli,groups/demoplace,groups/testup,files}; UP-MCC from build/upmcc/dist (Libs/mui/*, LIBS:up-mcc.library, themes to ENVARC:UP-MCC and ENV:UP-MCC, Inter-Regular.ttf to FONTS:); AmiSSL 6.1 (master, v41x 68020-40, 118 certs, DH0:AmiSSL/Certs now 294 files); RAM:net-probe.
Versions: up-mcc.library 2.0, UP-Cycle.mcc 1.0, amisslmaster.library 6.1, amissl_v41x.library 6.1.
Note: DH0:AmiSSL dirs already existed (disk not fully fresh). AMISSL: was NOT assigned on this boot; I ran "Assign AMISSL: DH0:AmiSSL" (runtime only, lost on reboot; add to S:User-Startup).

## Probes (GET only)
- http://example.com/ : rc 0, "status 200, 577 body bytes" (DNS + TCP OK).
- https://doors.uprough.net/ first run (no AMISSL: assign, no certs): rc 10 "FAIL: TLS handshake failed: certificate unable to get local issuer certificate". https://example.com/ identical. Stage: cert verify (DNS, TCP, handshake itself fine); cause = http_amiga.c loads trust store from AMISSL:Certs, which was unassigned/empty.
- After assign + certs: https://doors.uprough.net/ rc 0 "status 200, 591 body bytes" (Up Rough Door Repository); https://example.com/ rc 0 status 200, 577 bytes.
