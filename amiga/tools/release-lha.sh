#!/usr/bin/env bash
# usage: amiga/tools/release-lha.sh [--private] [packer-build-dir] [upmcc-build-dir]
# Packs the Amiga Release Dropper as an LHA on ~/Desktop (RELEASE_OUT overrides):
#   Up_Rough_Release_Dropper_-_Amiga[_-_private].lha
#     ReleaseDropper/ReleaseDropper   (packer-cli)
#     ReleaseDropper/ReleaseDropper.info (tool icon from the app's disk SVG, stack 49152)
#     ReleaseDropper/groups/<id>/    (--private only: app/private-profiles, keys included)
#     ReleaseDropper/UP-MCC/         (up_mcc_dist output with its Installer script)
#     ReleaseDropper/OpenMUI/        (upstream OpenMUI v1.0 release, unmodified, with its Install script and licences)
#     ReleaseDropper/Fonts/          (Topaz Pro 16 bitmap font for the NFO preview on RTG screens, from amiga/fonts, with its MIT LICENSE)
#     ReleaseDropper/README, INSTALL.txt
# OpenMUI: OPENMUI_LHA=<path> or a cached `gh release download` into amiga/build/openmui, sha256 pinned below.
# Builds nothing: build packer-cli and up_mcc_dist first. Keys never go to public CI,
# so the private archive is made here only.
set -euo pipefail

PRIVATE=0
[ "${1:-}" = "--private" ] && { PRIVATE=1; shift; }
A="$(cd "$(dirname "$0")/.." && pwd)"
ROOT="$(cd "$A/.." && pwd)"
B="${1:-$A/build/m68k-am}"; U="${2:-$A/build/upmcc}"
OUT="${RELEASE_OUT:-$HOME/Desktop}"
command -v lha >/dev/null || { echo "lha not found (brew install lha)" >&2; exit 1; }
for t in python3 rsvg-convert magick; do
  command -v "$t" >/dev/null || { echo "$t not found (icon needs python3, librsvg, imagemagick)" >&2; exit 1; }
done
[ -x "$B/packer-cli" ] || { echo "no $B/packer-cli: build it first" >&2; exit 1; }
[ -f "$U/dist/UP-MCC/Install-UP-MCC" ] || { echo "no $U/dist/UP-MCC: build up_mcc_dist first" >&2; exit 1; }

# OpenMUI (no-keyfile MUI replacement) ships as the upstream release archive, verified by hash.
OPENMUI_SHA=3fe44bca5aa1d34eb56048b870bf7c20074f918091a25315782eb3c9a68a28e5
OPENMUI_LHA="${OPENMUI_LHA:-$A/build/openmui/up-opmui.lha}"
if [ ! -f "$OPENMUI_LHA" ]; then
  command -v gh >/dev/null || { echo "no $OPENMUI_LHA and gh not found" >&2; exit 1; }
  mkdir -p "$(dirname "$OPENMUI_LHA")"
  gh release download v1.0 -R creep-ltx/openmui -p up-opmui.lha -O "$OPENMUI_LHA"
fi
[ "$(shasum -a 256 "$OPENMUI_LHA" | cut -d' ' -f1)" = "$OPENMUI_SHA" ] || { echo "$OPENMUI_LHA: sha256 mismatch (pinned $OPENMUI_SHA)" >&2; exit 1; }

STAGE=$(mktemp -d); trap 'rm -rf "$STAGE"' EXIT
D="$STAGE/ReleaseDropper"
mkdir -p "$D/groups"
cp "$B/packer-cli" "$D/ReleaseDropper"
python3 "$A/tools/mkicon.py" "$ROOT/tauri/src-tauri/icons/icon.svg" "$D/ReleaseDropper.info" --stack 49152
cp -R "$U/dist/UP-MCC" "$D/UP-MCC"
cp "$A/README.md" "$D/README"
OM=$(mktemp -d "$STAGE/om.XXXXXX")
(cd "$OM" && lha -xq "$OPENMUI_LHA")
[ -f "$OM/UP-OPMUI/OpenMUI/Install" ] && [ -f "$OM/UP-OPMUI/OpenMUI/LICENSE" ] || { echo "OpenMUI archive layout unexpected" >&2; exit 1; }
cp -R "$OM/UP-OPMUI/OpenMUI" "$D/OpenMUI"
# PROGDIR:Fonts: the NFO preview opens it by path on RTG screens, so nothing goes to FONTS:
mkdir -p "$D/Fonts"
cp -R "$A/fonts/TopazPro.font" "$A/fonts/TopazPro" "$D/Fonts/"
cp "$A/fonts/LICENSE" "$D/Fonts/LICENSE"
if [ "$PRIVATE" = 1 ]; then
  node "$ROOT/tauri/scripts/check-private-profiles.mjs"
  for g in "$ROOT"/app/private-profiles/*/; do
    id=$(basename "$g"); mkdir -p "$D/groups/$id"; cp "$g"/* "$D/groups/$id/"
    # Build reads groups/<id>/file_id.diz when the server seed has no banner art (same fallback as rig-install.sh)
    [ -f "$D/groups/$id/file_id.diz" ] || printf '%s release\n' "$id" > "$D/groups/$id/file_id.diz"
  done
  SUFFIX="_-_private"
else
  SUFFIX=""
fi
cat > "$D/INSTALL.txt" <<'EOF'
Up Rough Release Dropper - Amiga
================================

1. REQUIREMENTS

   AmigaOS 3.x, 68020 or better. MUI 3.8 or OpenMUI (OpenMUI is
   in this archive and needs no keyfile).
   lhA (Aminet util/arc/lha.run) and zip (Info-ZIP) in the Shell path.
   AmiSSL 5 or 6 for the release server, Door repo and Aminet upload.
   ttengine.library is optional (anti-aliased text).

2. INSTALL

   Copy the ReleaseDropper drawer anywhere, e.g. Work:ReleaseDropper.
   a) Install OpenMUI first: run OpenMUI/Install with Installer.
      Skip this only if you keep a registered MUI.
      OpenMUI is a third-party project (APL 1.1, TextEditor and
      BetterString LGPL; licences in OpenMUI/LICENSE and OpenMUI/Docs).
   b) Then run UP-MCC/Install-UP-MCC with Installer: it copies the UP-
      classes to MUI:Libs/mui, up-mcc.library, the Inter font and the
      themes.

3. START

   Double-click the ReleaseDropper icon. The icon and the program
   both ask for a 49152 byte stack, so no Stack command is needed.
   From a Shell:

       cd Work:ReleaseDropper
       ReleaseDropper

   On an RTG screen the NFO preview uses Fonts/TopazPro (topaz 8 on
   chipset screens); keep the Fonts drawer next to the program.

   Groups live in ReleaseDropper/groups; builds land in
   ReleaseDropper/releases.
EOF
# private archives hold write keys: owner-only permissions on the result
umask 077
NAME="Up_Rough_Release_Dropper_-_Amiga$SUFFIX.lha"
rm -f "$OUT/$NAME"
(cd "$STAGE" && lha -aq "$OUT/$NAME" ReleaseDropper >/dev/null)
lha -l "$OUT/$NAME" | tail -1
echo "wrote $OUT/$NAME"
