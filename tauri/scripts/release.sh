#!/usr/bin/env bash
# Builds the three release zips on ~/Desktop: universal macOS app locally,
# Windows exe and Linux AppImage from the latest green "Tauri build" run on tauri-ci.
# --private: the same three zips with app/private-profiles (keys): bundled in the
# macOS app, a private-profiles/ folder beside the CI exe and AppImage (profiles.rs
# seeds from there). Keys never go to public CI.
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
[ "$PRIVATE" = 1 ] && node tauri/scripts/check-private-profiles.mjs
read -r RUN SHA < <(gh run list -R "$REPO" -b tauri-ci -w "Tauri build" -s success -L 1 --json databaseId,headSha -q '.[0] | "\(.databaseId) \(.headSha)"')
[ -n "${RUN:-}" ] || { echo "no successful Tauri build run on tauri-ci" >&2; exit 1; }
echo "CI run $RUN, commit $SHA"
[ "$SHA" = "$(git rev-parse HEAD)" ] || echo "WARNING: CI commit differs from local HEAD $(git rev-parse HEAD)" >&2

# Windows + Linux from CI
gh run download "$RUN" -R "$REPO" -n release-dropper-tauri-Windows-exe -D "$STAGE/dl-win"
gh run download "$RUN" -R "$REPO" -n release-dropper-tauri-Linux -D "$STAGE/dl-linux"
mkdir "$STAGE/win" "$STAGE/linux"
cp "$STAGE/dl-win/release-dropper.exe" "$STAGE/win/"
APPIMAGE=$(find "$STAGE/dl-linux" -name '*.AppImage' | head -n1)
[ -n "$APPIMAGE" ] || { echo "no AppImage in run $RUN" >&2; exit 1; }
cp "$APPIMAGE" "$STAGE/linux/"
cp "$NOTES/INSTALL-Windows.txt" "$STAGE/win/INSTALL.txt"
cp "$NOTES/INSTALL-Linux.txt" "$STAGE/linux/INSTALL.txt"
if [ "$PRIVATE" = 1 ]; then
  for d in win linux; do
    cp -R app/private-profiles "$STAGE/$d/private-profiles"
    printf '\nPRIVATE BUILD: keep the private-profiles folder next to the program\nfor the first start; it holds the group keys, do not share it.\n' >> "$STAGE/$d/INSTALL.txt"
  done
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
ditto "tauri/src-tauri/target/universal-apple-darwin/release/bundle/macos/Release Dropper.app" "$STAGE/mac/Release Dropper.app"
cp "$NOTES/INSTALL-macOS.txt" "$STAGE/mac/INSTALL.txt"

SUFFIX=""; [ "$PRIVATE" = 1 ] && SUFFIX="_-_private"
PAIRS="macOS:mac Windows:win Linux:linux"
for pair in $PAIRS; do
  zip="$OUT/Up_Rough_Release_Dropper_-_${pair%%:*}${SUFFIX}.zip"
  rm -f "$zip"
  (cd "$STAGE/${pair##*:}" && ditto -c -k --sequesterRsrc . "$zip")
  echo "wrote $zip"
done
