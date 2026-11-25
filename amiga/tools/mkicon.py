#!/usr/bin/env python3
"""mkicon.py SVG OUT.info [--stack N] -- the Release Dropper Workbench tool icon from the app's
floppy disk SVG (tauri/src-tauri/icons/icon.svg), written in the DiskObject format
(workbench/workbench.h): DiskObject, a 4-pen planar Image for every Workbench, then an
OS 3.5+ colour icon (IFF FORM ICON: FACE + IMAG, 8-bit RLE) that newer icon.library shows
instead. do_StackSize carries the stack the program needs (MUI custom classes: 48 KB).
Layout follows ~/Code/UP-Term/vtcon-sframe2/tools/mkicon.py. Needs rsvg-convert and magick."""
import argparse, re, struct, subprocess

SIZE = 46  # the usual OS 3.5 colour icon size
NO_ICON_POSITION = -0x80000000


def rgba(svg_path):
    """SIZE x SIZE RGBA pixels of the disk alone: the macOS plate dropped, the view cropped to the disk."""
    svg = open(svg_path).read()
    svg = re.sub(r'\s*<rect x="100(\.5)?"[^>]*/>', '', svg)            # the dark rounded plate
    svg = svg.replace('viewBox="0 0 1024 1024"', 'viewBox="222 222 580 580"')
    png = subprocess.run(['rsvg-convert', '-w', str(SIZE), '-h', str(SIZE)], input=svg.encode(),
                         capture_output=True, check=True).stdout
    raw = subprocess.run(['magick', 'png:-', '-depth', '8', 'rgba:-'], input=png,
                         capture_output=True, check=True).stdout
    assert len(raw) == SIZE * SIZE * 4, len(raw)
    return [tuple(raw[i:i + 4]) for i in range(0, len(raw), 4)]


def classic_pen(p):
    """Workbench pens: 0 grey (also the background), 1 black, 2 white, 3 blue (the disk body)."""
    r, g, b, a = p
    if a < 128:
        return 0
    if r > b + 60:
        return 3
    return 1 if (r + g + b) < 300 else 2


def planes(pens, w, h, depth=2):
    words = (w + 15) // 16
    out = b''
    for d in range(depth):
        for y in range(h):
            bits = [(pens[y * w + x] >> d) & 1 for x in range(w)] + [0] * (words * 16 - w)
            for i in range(words):
                v = 0
                for bit in bits[i * 16:(i + 1) * 16]:
                    v = (v << 1) | bit
                out += struct.pack('>H', v)
    return out


def rle(data):
    """ByteRun1 with 8-bit elements, literal runs only (always valid, a little larger)."""
    out = b''
    for i in range(0, len(data), 128):
        chunk = data[i:i + 128]
        out += bytes([len(chunk) - 1]) + chunk
    return out


def chunk(tag, body):
    return tag + struct.pack('>L', len(body)) + body + (b'\0' if len(body) & 1 else b'')


def colour_icon(px):
    """FORM ICON: index 0 transparent, the rest RGB quantised to 6 levels per channel."""
    q = lambda c: min(5, (c + 25) // 51) * 51
    palette, index = [(0, 0, 0)], []
    for r, g, b, a in px:
        if a < 128:
            index.append(0)
            continue
        c = (q(r), q(g), q(b))
        if c not in palette:
            palette.append(c)
        index.append(palette.index(c))
    assert len(palette) <= 256
    img = rle(bytes(index))
    pal = rle(b''.join(bytes(c) for c in palette))
    face = struct.pack('>BBBBH', SIZE - 1, SIZE - 1, 0, 0x11, len(palette) * 3 - 1)
    imag = struct.pack('>BBBBBBHH', 0, len(palette) - 1, 0x03, 1, 1, 8, len(img) - 1, len(pal) - 1) + img + pal
    body = b'ICON' + chunk(b'FACE', face) + chunk(b'IMAG', imag)
    return b'FORM' + struct.pack('>L', len(body)) + body


def icon(px, stack):
    w = h = SIZE
    gadget = struct.pack('>LhhhhHHHLLLLLHL',
                         0, 0, 0, w, h,
                         0x0004,        # Flags: GFLG_GADGIMAGE, complement highlight
                         0x0003,        # Activation: RELVERIFY | GADGIMMEDIATE
                         0x0001,        # GadgetType: BOOLGADGET
                         1,             # GadgetRender: an image follows
                         0, 0, 0, 0, 0,
                         1)             # UserData: WB_DISKREVISION
    diskobj = struct.pack('>HH', 0xE310, 1) + gadget + struct.pack(
        '>BBLLllLLl',
        3, 0,                          # do_Type WBTOOL
        0, 0,                          # no default tool, no tooltypes
        NO_ICON_POSITION, NO_ICON_POSITION,
        0, 0, stack)
    image = struct.pack('>hhhhhLBBL', 0, 0, w, h, 2, 1, 3, 0, 0) + planes([classic_pen(p) for p in px], w, h)
    return diskobj + image + colour_icon(px)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('svg')
    ap.add_argument('out')
    ap.add_argument('--stack', type=int, default=49152)
    a = ap.parse_args()
    data = icon(rgba(a.svg), a.stack)
    # self-check: DiskObject 78 bytes, Image header 20 + planar data, then a whole FORM
    form = 78 + 20 + 2 * SIZE * ((SIZE + 15) // 16) * 2
    assert data[:2] == b'\xe3\x10' and data[form:form + 4] == b'FORM'
    assert struct.unpack('>L', data[form + 4:form + 8])[0] + 8 == len(data) - form
    assert struct.unpack('>l', data[74:78])[0] == a.stack
    open(a.out, 'wb').write(data)


if __name__ == '__main__':
    main()
