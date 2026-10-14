#!/usr/bin/env python3
"""Converts Micropolis' XPM graphics into compact indexed binaries for the Amiga frontend.

  out/data/palette.bin   256 x RGB (3 bytes); index 0 = transparent (sprites) / black
  out/data/tiles.bin     960 tiles of 16x16 pixels, 8 bit palette indices (245760 bytes)
  out/data/tilessm.bin   960 mini-map tiles of 4x3 pixels (12 bytes each)
  out/data/objects.bin   sprite frames: u16 count, then per frame (u8 type, u8 frame, u8 w, u8 h,
                         u32 offset) in file-relative offset, followed by w*h index bytes (0 = transparent)
All multi-byte values big-endian (native on the 68k).
usage: convert_assets.py UPSTREAM_IMAGES_DIR OUT_DIR [--preview]"""
import re, sys, os, struct, glob

def parse_xpm(path):
    txt = open(path, encoding='latin-1').read()
    rows = re.findall(r'"([^"]*)"', txt)
    w, h, ncol, cpp = map(int, rows[0].split())
    cmap = {}
    for r in rows[1:1 + ncol]:
        key, rest = r[:cpp], r[cpp:]
        m = re.search(r'\bc\s+(\S+)', rest)
        v = m.group(1)
        if v.lower() == 'none':
            cmap[key] = None
        else:
            hexv = v[1:]
            n = len(hexv) // 3
            cmap[key] = tuple(int(hexv[i * n:(i + 1) * n][:2], 16) for i in range(3))
    pix = []
    for r in rows[1 + ncol:1 + ncol + h]:
        assert len(r) == w * cpp, (path, len(r), w)
        pix.append([cmap[r[x * cpp:(x + 1) * cpp]] for x in range(w)])
    return w, h, pix

class Palette:
    def __init__(self):
        self.colors = [(0, 0, 0)]      # 0 reserved: transparent
    def index(self, rgb):
        if rgb is None:
            return 0
        if rgb not in self.colors[1:]:
            self.colors.append(rgb)
        return self.colors.index(rgb, 1)

def main():
    src, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    pal = Palette()

    w, h, pix = parse_xpm(os.path.join(src, 'tiles.xpm'))
    assert w == 16 and h == 960 * 16
    tiles = bytes(pal.index(c) for row in pix for c in row)
    open(os.path.join(out, 'tiles.bin'), 'wb').write(tiles)

    w, h, pix = parse_xpm(os.path.join(src, 'tilessm.xpm'))
    assert w == 4 and h == 960 * 3, (w, h)
    open(os.path.join(out, 'tilessm.bin'), 'wb').write(bytes(pal.index(c) for row in pix for c in row))

    frames = []
    for f in sorted(glob.glob(os.path.join(src, 'obj*-*.xpm'))):
        m = re.match(r'obj(\d+)-(\d+)\.xpm$', os.path.basename(f))
        if not m:
            continue
        w, h, pix = parse_xpm(f)
        frames.append((int(m.group(1)), int(m.group(2)), w, h, bytes(pal.index(c) for row in pix for c in row)))
    frames.sort()
    head = struct.pack('>H', len(frames))
    off = len(head) + len(frames) * 8
    table, data = b'', b''
    for t, fr, w, h, px in frames:
        table += struct.pack('>BBBBI', t, fr, w, h, off + len(data))
        data += px
    open(os.path.join(out, 'objects.bin'), 'wb').write(head + table + data)

    assert len(pal.colors) <= 64, len(pal.colors)
    p = b''.join(bytes(c) for c in pal.colors) + bytes(3 * (256 - len(pal.colors)))
    open(os.path.join(out, 'palette.bin'), 'wb').write(p)
    print('palette: %d colors, tiles: %d bytes, %d sprite frames' % (len(pal.colors), len(tiles), len(frames)))

    if '--preview' in sys.argv:
        from PIL import Image
        im = Image.new('RGB', (16 * 40, 16 * 24))
        for t in range(960):
            for y in range(16):
                for x in range(16):
                    im.putpixel(((t % 40) * 16 + x, (t // 40) * 16 + y), pal.colors[tiles[t * 256 + y * 16 + x]])
        im.save(os.path.join(out, 'preview_tiles.png'))

main()
