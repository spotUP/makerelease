#!/usr/bin/env bash
# usage: amiga/tools/release-lha.sh [--private] [packer-build-dir] [upmcc-build-dir]
# Packs the Amiga Release Dropper as an LHA on ~/Desktop (RELEASE_OUT overrides):
#   Up_Rough_Release_Dropper_-_Amiga[_-_private].lha
#     ReleaseDropper/ReleaseDropper   (packer-cli)
#     ReleaseDropper/ReleaseDropper.info (tool icon from the app's disk SVG, stack 49152)
#     ReleaseDropper/groups/<id>/    (--private only: app/private-profiles, keys included)
#     ReleaseDropper/UP-MCC/         (up_mcc_dist output with its Installer script)
#     ReleaseDropper/README, INSTALL.txt
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

STAGE=$(mktemp -d); trap 'rm -rf "$STAGE"' EXIT
D="$STAGE/ReleaseDropper"
mkdir -p "$D/groups"
cp "$B/packer-cli" "$D/ReleaseDropper"
python3 "$A/tools/mkicon.py" "$ROOT/tauri/src-tauri/icons/icon.svg" "$D/ReleaseDropper.info" --stack 49152
cp -R "$U/dist/UP-MCC" "$D/UP-MCC"
cp "$A/README.md" "$D/README"
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

   AmigaOS 3.x, 68020 or better, MUI 3.8.
   lhA (Aminet util/arc/lha.run) and zip (Info-ZIP) in the Shell path.
   AmiSSL 5 or 6 for the release server, Door repo and Aminet upload.
   ttengine.library is optional (anti-aliased text).

2. INSTALL

   Copy the ReleaseDropper drawer anywhere, e.g. Work:ReleaseDropper.
   Run UP-MCC/Install-UP-MCC with Installer: it copies the UP- classes
   to MUI:Libs/mui, up-mcc.library, the Inter font and the themes.

3. START

   Double-click the ReleaseDropper icon. The icon and the program
   both ask for a 49152 byte stack, so no Stack command is needed.
   From a Shell:

       cd Work:ReleaseDropper
       ReleaseDropper

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
