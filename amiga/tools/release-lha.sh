#!/usr/bin/env bash
# usage: amiga/tools/release-lha.sh [--private] [packer-build-dir] [upmcc-build-dir]
# Packs the Amiga Release Packer as an LHA on ~/Desktop (RELEASE_OUT overrides):
#   Up_Rough_Release_Packer_-_Amiga[_-_private].lha
#     ReleasePacker/ReleasePacker   (packer-cli)
#     ReleasePacker/groups/<id>/    (--private only: app/private-profiles, keys included)
#     ReleasePacker/UP-MCC/         (up_mcc_dist output with its Installer script)
#     ReleasePacker/README, INSTALL.txt
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
[ -x "$B/packer-cli" ] || { echo "no $B/packer-cli: build it first" >&2; exit 1; }
[ -f "$U/dist/UP-MCC/Install-UP-MCC" ] || { echo "no $U/dist/UP-MCC: build up_mcc_dist first" >&2; exit 1; }

STAGE=$(mktemp -d); trap 'rm -rf "$STAGE"' EXIT
D="$STAGE/ReleasePacker"
mkdir -p "$D/groups"
cp "$B/packer-cli" "$D/ReleasePacker"
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
Up Rough Release Packer - Amiga
===============================

1. REQUIREMENTS

   AmigaOS 3.x, 68020 or better, MUI 3.8.
   lhA (Aminet util/arc/lha.run) and zip (Info-ZIP) in the Shell path.
   AmiSSL 5 or 6 for the release server, Door repo and Aminet upload.
   ttengine.library is optional (anti-aliased text).

2. INSTALL

   Copy the ReleasePacker drawer anywhere, e.g. Work:ReleasePacker.
   Run UP-MCC/Install-UP-MCC with Installer: it copies the UP- classes
   to MUI:Libs/mui, up-mcc.library, the Inter font and the themes.

3. START

   There is no Workbench icon yet. Start it from a Shell:

       cd Work:ReleasePacker
       stack 49152
       ReleasePacker

   Groups live in ReleasePacker/groups; builds land in
   ReleasePacker/releases.
EOF
# private archives hold write keys: owner-only permissions on the result
umask 077
NAME="Up_Rough_Release_Packer_-_Amiga$SUFFIX.lha"
rm -f "$OUT/$NAME"
(cd "$STAGE" && lha -aq "$OUT/$NAME" ReleasePacker >/dev/null)
lha -l "$OUT/$NAME" | tail -1
echo "wrote $OUT/$NAME"
