#!/bin/sh
# Copy the compiled renderer and shared code from the Electron build into tauri/web (never edit tauri/web by hand).
set -e
cd "$(dirname "$0")/.."
(cd ../app && npm run build:renderer >/dev/null)
rm -rf web && mkdir -p web
cp -R ../app/dist/renderer/renderer web/renderer
cp -R ../app/dist/renderer/shared web/shared
cp web-shim/shim.js web/renderer/shim.js
printf '<!doctype html><meta http-equiv="refresh" content="0;url=renderer/index.html">\n' > web/index.html
# Electron preload is replaced by the shim; Tauri IPC needs connect-src.
sed -i.bak "s#img-src 'self' file:;#img-src 'self' file: data:; connect-src 'self' ipc: http://ipc.localhost;#" web/renderer/index.html
grep -q '<script src="shim.js">' web/renderer/index.html || sed -i.bak 's#<script type="module" src="main.js">#<script src="shim.js"></script> <script type="module" src="main.js">#' web/renderer/index.html
rm -f web/renderer/index.html.bak
