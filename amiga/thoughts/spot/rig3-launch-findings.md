# Rig 3 reprovision and packer launch (2026-11-23)

Install (all via ami.py put, no failures): up-mcc.library (Version 2.0), 12 files to MUI:Libs/mui, Dark/Light theme to ENVARC:UP-MCC and ENV:UP-MCC, Inter-Regular.ttf to FONTS:, AmiSSL 6.1 master + v41x (68020-40) both Version 6.1, 118 certs to DH0:AmiSSL/Certs (0 put failures), packer-cli to DH0:Packer, net-probe to RAM:.
Disk was NOT fresh: it still had old AmiSSL 5.27 (amissl_v362). User-Startup already assigns AmiSSL: SYS:AmiSSL (same dir as DH0:AmiSSL, AMISSL: is the same name case-insensitively); no extra line added.
net-probe https://doors.uprough.net/ : status 200, 591 body bytes.

Launch 1 (groups missing): Amiga stayed responsive (ping ok). uitree: only "Release Packer" window 243x75 with an Ok button = shell_requester, probably "No groups found" (ui_amiga.c:616); DH0:Packer/groups did not exist. A second foreground run of packer-cli stacked a second requester.
Fix: put scratch-rig/groups/{demoplace,testup} and scratch-rig/files to DH0:Packer.
Launch 2 (after `ami.py free 1`, groups present): wasabid then refused connections (ConnectionRefusedError) on ping and uitree, still refused 45 s later. fs-uae rig3 process alive at ~90% CPU. Cause unknown: Amiga froze or wasabid died after launch 2 (or after `free 1`). Not retried further. No main window seen.
