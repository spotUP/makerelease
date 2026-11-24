#!/usr/bin/env bash
# Builds the three release zips on ~/Desktop: universal macOS app locally,
# Windows exe and Linux AppImage from the latest green "Tauri build" run on tauri-ci.
# --private: macOS only, with app/private-profiles (keys) bundled. Keys never go to
# public CI, so there is no private Windows/Linux build.
set -euo pipefail

PRIVATE=0
[ "${1:-}" = "--private" ] && PRIVATE=1

REPO=spotUP/makerelease
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${RELEASE_OUT:-$HOME/Desktop}
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

cd "$ROOT"
NOTES=app/install-notes
if [ "$PRIVATE" = 1 ]; then
  node tauri/scripts/check-private-profiles.mjs
else
read -r RUN SHA < <(gh run list -R "$REPO" -b tauri-ci -w "Tauri build" -s success -L 1 --json databaseId,headSha -q '.[0] | "\(.databaseId) \(.headSha)"')
[ -n "${RUN:-}" ] || { echo "no successful Tauri build run on tauri-ci" >&2; exit 1; }
echo "CI run $RUN, commit $SHA"
[ "$SHA" = "$(git rev-parse HEAD)" ] || echo "WARNING: CI commit differs from local HEAD $(git rev-parse HEAD)" >&2

# Windows + Linux from CI
gh run download "$RUN" -R "$REPO" -n makerelease-tauri-Windows-exe -D "$STAGE/dl-win"
gh run download "$RUN" -R "$REPO" -n makerelease-tauri-Linux -D "$STAGE/dl-linux"
mkdir "$STAGE/win" "$STAGE/linux"
cp "$STAGE/dl-win/makerelease.exe" "$STAGE/win/"
APPIMAGE=$(find "$STAGE/dl-linux" -name '*.AppImage' | head -n1)
[ -n "$APPIMAGE" ] || { echo "no AppImage in run $RUN" >&2; exit 1; }
cp "$APPIMAGE" "$STAGE/linux/"
cp "$NOTES/INSTALL-Windows.txt" "$STAGE/win/INSTALL.txt"
cp "$NOTES/INSTALL-Linux.txt" "$STAGE/linux/INSTALL.txt"
fi

# macOS universal, built locally against the current renderer
sh tauri/scripts/sync-web.sh
rustup target add x86_64-apple-darwin aarch64-apple-darwin
[ -d tauri/node_modules ] || (cd tauri && npm ci)
EXTRA=()
# resource_dir()/private-profiles is where profiles.rs seeds from; path relative to src-tauri.
[ "$PRIVATE" = 1 ] && EXTRA=(--config '{"bundle":{"resources":{"../../app/private-profiles/":"private-profiles/"}}}')
(cd tauri && nice -n 19 npx tauri build --target universal-apple-darwin --bundles app ${EXTRA[@]+"${EXTRA[@]}"})
mkdir "$STAGE/mac"
ditto "tauri/src-tauri/target/universal-apple-darwin/release/bundle/macos/makerelease.app" "$STAGE/mac/makerelease.app"
cp "$NOTES/INSTALL-macOS.txt" "$STAGE/mac/INSTALL.txt"

if [ "$PRIVATE" = 1 ]; then PAIRS="macOS:mac"; SUFFIX="_-_private"; else PAIRS="macOS:mac Windows:win Linux:linux"; SUFFIX=""; fi
for pair in $PAIRS; do
  zip="$OUT/Up_Rough_Release_Maker_-_${pair%%:*}${SUFFIX}.zip"
  rm -f "$zip"
  (cd "$STAGE/${pair##*:}" && ditto -c -k --sequesterRsrc . "$zip")
  echo "wrote $zip"
done
