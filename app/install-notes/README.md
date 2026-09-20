# Distribution install notes

One file per platform, shipped as `INSTALL.txt` inside the matching member
zip (`Up_Rough_Release_Maker_-_{macOS,Linux,Windows}.zip`). They lived only
inside the zips until 2026-09-20, which meant the only copy of the text was
whatever had last been uploaded to the web server.

Packing a release zip:

    <platform dir>/INSTALL.txt   <- INSTALL-<platform>.txt from here
    <platform dir>/<artifacts>   <- from app/release/ after `npm run dist`

Keep them in sync with what the builds actually produce: the file names in
section "INSTALL" are the artifact names electron-builder writes, and the
output path in the last section is the one `output-root.ts` resolves.

Windows section 2 exists because a member hit SmartScreen and reported
"Windows blocked the execution ... I did not even see the application".
Unblocking the ZIP before extracting clears the Mark-of-the-Web from
everything inside it, so the exe never triggers SmartScreen at all. The only
real fix is an Authenticode signature, which we do not have.
