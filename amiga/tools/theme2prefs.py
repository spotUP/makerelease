#!/usr/bin/env python3
"""UP-MCC theme file -> OpenMUI global.prefs (IFF FORM PREF: PRHD, MUIC), so the
stock MUI classes of every application take the UP-MCC colours with no source
change. usage: theme2prefs.py Dark.theme > Dark.prefs
shortcut: colours only (backgrounds, string gadget, focus ring); frame, shine/shadow
and text pens stay the screen's, upgrade with a muimaster patch when frames must match."""
import struct, sys

# MUICFG id -> ("2:" image spec or bare pen spec, theme key). Ids: libraries/mui.h / muimaster config ids.
IMG = {0x37: "background", 0x38: "background", 0x95: "background", 0x54: "background",   # window, requester, page, register
       0x39: "surface_raised", 0x3d: "surface_raised", 0x3c: "surface", 0x5b: "surface",  # button, popup, prop, slider
       0x3a: "surface", 0x96: "surface", 0x3b: "surface", 0x5a: "background",             # list, readlist, text, framed
       0x3e: "surface_pressed", 0x3f: "surface_hover", 0x40: "accent", 0x41: "accent",    # selected, list cursor/select/selcur
       0x5c: "accent",                                                                    # slider knob
       0x84: "surface", 0x86: "surface_raised"}                                           # string background, active
PEN = {0x85: "text", 0x87: "text", 0x400: "accent", 0x401: "accent", 0x402: "accent_text",  # string text/cursor/marked
       0x8f: "focus_ring"}                                                                 # active object


def chunk(cid, body):
    return cid + struct.pack(">L", len(body)) + body + b"\0" * (len(body) & 1)


def prefs(theme_text):
    t = {}
    for line in theme_text.splitlines():
        if line.startswith("gui.") and "=" in line:
            k, v = line[4:].split("=", 1)
            t[k.strip()] = v.strip()
    rec = b""
    for table, lead in ((IMG, "2:r"), (PEN, "r")):
        for cid, key in sorted(table.items()):
            s = (lead + "%06X" % int(t[key], 16)).encode() + b"\0"   # KeyError: the theme lacks a colour we need
            rec += struct.pack(">LL", cid, len(s)) + s
    out = b"FORM" + struct.pack(">L", 0) + b"PREF" + chunk(b"PRHD", bytes(6)) + chunk(b"MUIC", rec)
    return out[:4] + struct.pack(">L", len(out) - 8) + out[8:]


if __name__ == "__main__":
    data = prefs(open(sys.argv[1]).read())
    assert data[8:12] == b"PREF" and len(data) % 2 == 0 and data.count(b"2:r") == len(IMG)
    sys.stdout.buffer.write(data)
