#!/usr/bin/env python3
"""usage: mkfont.py <TopazPlus.ttf> <out-dir> [--check]

Turns the TopazPlus a1200 TrueType (a pixel font: 100 units = 1 pixel, 8x16 cell, outlines only on whole
pixels) into an Amiga bitmap diskfont the NFO preview opens on RTG screens:
  <out-dir>/TopazPlus.font   FontContentsHeader, one 16 px entry
  <out-dir>/TopazPlus/16     hunk file: DiskFontHeader + TextFont + chardata/charloc, chars 32..255 (Latin-1)
Stdlib only: the TTF is parsed by hand and each pixel is lit when its centre is inside the outline (nonzero
winding), which is exact for an outline that only runs along pixel edges.
--check parses the written files back and asserts the layout and a few glyphs against the TTF raster.
The name stays TopazPlus: the font asks not to be renamed (name table id 13; licence CC BY-NC-SA 3.0, id 14).
"""
import os
import struct
import sys

UNITS = 100            # TTF units per pixel
CELL_W, CELL_H = 8, 16
BASELINE = 13          # last row above the baseline: 'A' fills rows 0..13, descenders 14..15
LO, HI = 32, 255
NAME = "TopazPlus"
FPF_DESIGNED, FPF_DISKFONT = 0x40, 0x02


class TTF:
    def __init__(self, data):
        self.d = data
        n = struct.unpack(">H", data[4:6])[0]
        self.t = {}
        for i in range(n):
            tag, _, off, ln = struct.unpack(">4sIII", data[12 + 16 * i:28 + 16 * i])
            self.t[tag.decode()] = off
        h = self.t["head"]
        assert struct.unpack(">H", data[h + 18:h + 20])[0] == CELL_H * UNITS, "unitsPerEm is not 16 px"
        self.long_loca = struct.unpack(">h", data[h + 50:h + 52])[0] == 1
        self._cmap()

    def _cmap(self):
        d, o = self.d, self.t["cmap"]
        sub = None
        for i in range(struct.unpack(">H", d[o + 2:o + 4])[0]):
            p, e, off = struct.unpack(">HHI", d[o + 4 + 8 * i:o + 12 + 8 * i])
            if (p, e) == (3, 1):
                sub = o + off
        assert sub is not None and struct.unpack(">H", d[sub:sub + 2])[0] == 4, "no Windows BMP cmap format 4"
        sc = struct.unpack(">H", d[sub + 6:sub + 8])[0] // 2
        a = sub + 14
        self.ends = struct.unpack(">%dH" % sc, d[a:a + 2 * sc])
        a += 2 * sc + 2
        self.starts = struct.unpack(">%dH" % sc, d[a:a + 2 * sc])
        a += 2 * sc
        self.deltas = struct.unpack(">%dh" % sc, d[a:a + 2 * sc])
        a += 2 * sc
        self.ro_base = a
        self.ranges = struct.unpack(">%dH" % sc, d[a:a + 2 * sc])

    def gid(self, ch):
        for i, (s, e) in enumerate(zip(self.starts, self.ends)):
            if s <= ch <= e:
                if self.ranges[i] == 0:
                    return (ch + self.deltas[i]) & 0xFFFF
                a = self.ro_base + 2 * i + self.ranges[i] + 2 * (ch - s)
                g = struct.unpack(">H", self.d[a:a + 2])[0]
                return (g + self.deltas[i]) & 0xFFFF if g else 0
        return 0

    def _loca(self, g):
        lo = self.t["loca"]
        if self.long_loca:
            return struct.unpack(">I", self.d[lo + 4 * g:lo + 4 * g + 4])[0]
        return 2 * struct.unpack(">H", self.d[lo + 2 * g:lo + 2 * g + 2])[0]

    def contours(self, g):
        d = self.d
        a = self.t["glyf"] + self._loca(g)
        if self._loca(g) == self._loca(g + 1):
            return []
        nc = struct.unpack(">h", d[a:a + 2])[0]
        assert nc >= 0, "composite glyph %d not supported" % g
        ends = struct.unpack(">%dH" % nc, d[a + 10:a + 10 + 2 * nc])
        npts = ends[-1] + 1
        p = a + 12 + 2 * nc + struct.unpack(">H", d[a + 10 + 2 * nc:a + 12 + 2 * nc])[0]
        flags = []
        while len(flags) < npts:
            f = d[p]; p += 1
            flags.append(f)
            if f & 8:
                flags += [f] * d[p]; p += 1
        assert all(f & 1 for f in flags), "glyph %d has curves: not a pixel font" % g
        coords = []
        for short, same in ((2, 16), (4, 32)):
            v, out = 0, []
            for f in flags:
                if f & short:
                    dv = d[p] if f & same else -d[p]; p += 1
                elif f & same:
                    dv = 0
                else:
                    dv = struct.unpack(">h", d[p:p + 2])[0]; p += 2
                v += dv
                out.append(v)
            coords.append(out)
        pts = list(zip(*coords))
        res, s = [], 0
        for e in ends:
            res.append(pts[s:e + 1]); s = e + 1
        return res

    def raster(self, ch):
        """CELL_H rows of CELL_W bits (MSB = leftmost pixel); unmapped chars are blank."""
        g = self.gid(ch)
        cs = self.contours(g) if g else []
        rows = []
        for r in range(CELL_H):
            y = CELL_H * UNITS - UNITS * r - UNITS // 2
            bits = 0
            for c in range(CELL_W):
                x = UNITS * c + UNITS // 2
                w = 0
                for pts in cs:
                    for i in range(len(pts)):
                        (x0, y0), (x1, y1) = pts[i], pts[(i + 1) % len(pts)]
                        if (y0 <= y) != (y1 <= y) and x0 + (y - y0) * (x1 - x0) / (y1 - y0) > x:
                            w += 1 if y1 > y0 else -1
                if w:
                    bits |= 0x80 >> c
            rows.append(bits)
        return rows


def build_font(ttf):
    """Returns (hunk file bytes, number of glyph slots)."""
    n = HI - LO + 2                      # + the default glyph diskfont draws for chars outside LO..HI
    glyphs = [ttf.raster(c) for c in range(LO, HI + 1)] + [[0] * CELL_H]
    modulo = (n + 1) & ~1                # one byte per 8 px glyph, word aligned rows
    chardata = bytearray(modulo * CELL_H)
    for i, rows in enumerate(glyphs):
        for r, bits in enumerate(rows):
            chardata[r * modulo + i] = bits
    charloc = b"".join(struct.pack(">HH", i * CELL_W, CELL_W) for i in range(n))

    # hunk layout: 0 moveq #-1,d0 / rts, 4 DiskFontHeader (Node 14, FileID, Revision, Segment, Name[32]),
    # 58 TextFont (Message 20, then the font fields, 52 bytes), 110 chardata, then charloc
    DFH, TF = 4, 58
    name_off = DFH + 22
    data_off = TF + 52
    loc_off = data_off + len(chardata)
    node = lambda: struct.pack(">IIbbI", 0, 0, 12, 0, name_off)          # NT_FONT, ln_Name -> dfh_Name
    body = struct.pack(">I", 0x70FF4E75)
    body += node() + struct.pack(">HHI", 0x0F80, 1, 0) + (NAME + ".font").encode().ljust(32, b"\0")
    body += node() + struct.pack(">IH", 0, 52)
    body += struct.pack(">HBBHHHHBBIHIII", CELL_H, 0, FPF_DESIGNED | FPF_DISKFONT, CELL_W, BASELINE, 1, 0,
                        LO, HI, data_off, modulo, loc_off, 0, 0)
    assert len(body) == data_off
    body += chardata + charloc
    body += b"\0" * (-len(body) % 4)
    relocs = [DFH + 10, TF + 10, TF + 34, TF + 40]                       # both ln_Name, tf_CharData, tf_CharLoc
    hunk = struct.pack(">IIIII", 0x3F3, 0, 1, 0, 0) + struct.pack(">I", len(body) // 4)
    hunk += struct.pack(">II", 0x3E9, len(body) // 4) + body
    hunk += struct.pack(">III", 0x3EC, len(relocs), 0) + b"".join(struct.pack(">I", r) for r in relocs)
    hunk += struct.pack(">II", 0, 0x3F2)
    return hunk, n


def build_contents():
    return struct.pack(">HH", 0x0F00, 1) + ("%s/%d" % (NAME, CELL_H)).encode().ljust(256, b"\0") + \
        struct.pack(">HBB", CELL_H, 0, FPF_DESIGNED | FPF_DISKFONT)


def check(out, ttf):
    fc = open(os.path.join(out, NAME + ".font"), "rb").read()
    assert len(fc) == 264 and struct.unpack(">HH", fc[:4]) == (0x0F00, 1)
    assert fc[4:260].rstrip(b"\0") == b"TopazPlus/16" and struct.unpack(">H", fc[260:262])[0] == CELL_H
    h = open(os.path.join(out, NAME, str(CELL_H)), "rb").read()
    magic, _, nh, first, last, size = struct.unpack(">IIIIII", h[:24])
    assert (magic, nh, first, last) == (0x3F3, 1, 0, 0)
    kind, nlong = struct.unpack(">II", h[24:32])
    assert kind == 0x3E9 and nlong == size
    b = h[32:32 + 4 * nlong]
    p = 32 + 4 * nlong
    kind, cnt, target = struct.unpack(">III", h[p:p + 12])
    relocs = struct.unpack(">%dI" % cnt, h[p + 12:p + 12 + 4 * cnt])
    assert kind == 0x3EC and target == 0 and h[p + 12 + 4 * cnt:] == struct.pack(">II", 0, 0x3F2)
    for r in relocs:
        assert struct.unpack(">I", b[r:r + 4])[0] < len(b), "reloc %d points outside the hunk" % r
    assert struct.unpack(">HH", b[18:22]) == (0x0F80, 1)
    tf = 58
    ysize, style, flags, xsize, base = struct.unpack(">HBBHH", b[tf + 20:tf + 28])
    lo, hi, cd, mod, cl = struct.unpack(">BBIHI", b[tf + 32:tf + 44])
    assert (ysize, xsize, base, lo, hi) == (CELL_H, CELL_W, BASELINE, LO, HI)
    assert b[struct.unpack(">I", b[tf + 10:tf + 14])[0]:].startswith(b"TopazPlus.font\0")
    n = hi - lo + 2
    locs = [struct.unpack(">HH", b[cl + 4 * i:cl + 4 * i + 4]) for i in range(n)]
    assert len(locs) == 225 and all(w == CELL_W for _, w in locs)

    def glyph(ch):
        off = locs[ord(ch) - lo][0]
        return [b[cd + r * mod + off // 8] for r in range(ysize)]
    # topaz 'A' doubled vertically, as rendered from the TTF; space is blank
    assert glyph("A") == [0x3C, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x7E, 0x7E] + [0x66] * 6 + [0, 0]
    assert glyph(" ") == [0] * CELL_H
    for ch in "g@|\xe9\xff":
        assert glyph(ch) == ttf.raster(ord(ch)), ch
    assert any(glyph("\xe9")), "Latin-1 e-acute missing"
    print("check ok: %d glyph slots, YSize %d, chars %d..%d" % (n, ysize, lo, hi))


def main(argv):
    if len(argv) < 3:
        sys.exit(__doc__)
    ttf = TTF(open(argv[1], "rb").read())
    out = argv[2]
    os.makedirs(os.path.join(out, NAME), exist_ok=True)
    hunk, _ = build_font(ttf)
    with open(os.path.join(out, NAME, str(CELL_H)), "wb") as f:
        f.write(hunk)
    with open(os.path.join(out, NAME + ".font"), "wb") as f:
        f.write(build_contents())
    if "--check" in argv:
        check(out, ttf)


if __name__ == "__main__":
    main(sys.argv)
