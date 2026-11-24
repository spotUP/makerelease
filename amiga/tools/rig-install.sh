#!/bin/bash
# usage: UPTERM_RIG=<n> amiga/tools/rig-install.sh [upmcc-build-dir] [packer-build-dir]
# Installs the up-mcc classes and the packer on a fresh vtcon rig and starts one instance.
# Creates the drawers a clean rig lacks (Packer, groups, ENV and ENVARC UP-MCC) and seeds the group drawers.
set -u
A="$(cd "$(dirname "$0")/.." && pwd)"
U="${1:-$A/build/upmcc}"; B="${2:-$A/build/m68k-am}"
: "${UPTERM_RIG:?set UPTERM_RIG to your rig number}"
cd /Users/spot/Code/UP-Term/vtcon/tools/rig || exit 1
P="python3 ami.py"
# one exec per dir: makedir aborts the rest of its chain when a dir exists
for d in DH0:Packer DH0:Packer/groups DH0:Packer/groups/uprough DH0:Packer/groups/xinnovations ENVARC:UP-MCC ENV:UP-MCC; do $P exec "makedir $d" >/dev/null 2>&1; done
# AmiSSL: for the packer's TLS (same line as RIG_LIBS in UP-Term vtcon rig.py); a fresh rig has none after go
$P exec "Assign >NIL: AmiSSL: DH0:AmiSSL" >/dev/null 2>&1
for f in "$U"/UP-*.mcc; do $P put "$f" "MUI:Libs/mui/$(basename "$f")" >/dev/null || echo "put fail $f"; done
$P put "$U/up-mcc.library" VTCX:ixp6/up-mcc.library >/dev/null
for t in Dark Light; do for d in ENVARC:UP-MCC ENV:UP-MCC; do $P put "$U/dist/UP-MCC/Themes/$t.theme" "$d/$t.theme" >/dev/null; done; done
$P put "$U/dist/UP-MCC/Fonts/Inter-Regular.ttf" FONTS:Inter-Regular.ttf >/dev/null
for g in uprough xinnovations; do for f in "$A"/scratch-rig/groups/$g/*; do $P put "$f" "DH0:Packer/groups/$g/$(basename "$f")" >/dev/null; done; done
$P exec "echo \"Up Rough release\" >DH0:Packer/groups/uprough/file_id.diz" >/dev/null 2>&1
$P put "$B/packer-cli" DH0:Packer/packer-cli >/dev/null
$P exec "run >NIL: <NIL: DH0:Packer/packer-cli ; echo started" 2>&1 | tail -1
sleep 10
if timeout 12 $P ping >/dev/null 2>&1; then echo ALIVE; else echo FROZEN; fi
