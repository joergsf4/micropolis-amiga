#!/usr/bin/env python3
"""Writes classic (planar, 4 colour, Workbench 1.x-3.x compatible) .info icons for the release.
usage: make_icons.py OUT_DIR [PREVIEW.png]
Palette = default Workbench: 0 grey, 1 black, 2 white, 3 blue."""
import struct, sys, os
from PIL import Image, ImageDraw

PAL = [(0xaa, 0xaa, 0xaa), (0, 0, 0), (0xff, 0xff, 0xff), (0x66, 0x88, 0xbb)]
G, K, W, B = 0, 1, 2, 3

def planar(img, depth=2):
    w, h = img.size
    words = (w + 15) // 16
    px = img.load()
    out = bytearray()
    for plane in range(depth):
        for y in range(h):
            for wd in range(words):
                v = 0
                for bit in range(16):
                    x = wd * 16 + bit
                    v = (v << 1) | (((px[x, y] >> plane) & 1) if x < w else 0)
                out += struct.pack('>H', v)
    return bytes(out)

def image_struct(img):
    w, h = img.size
    return struct.pack('>hhhhhIBBI', 0, 0, w, h, 2, 1, 3, 0, 0) + planar(img)

def disk_object(kind, img, sel, default_tool=None, tooltypes=(), stack=4096, drawer=False):
    w, h = img.size
    flags = 6 if sel else 4
    d = struct.pack('>HHIhhhhHHHIIIiIHI', 0xE310, 1, 0, 0, 0, w, h, flags, 1, 1,
                    1, 1 if sel else 0, 0, 0, 0, 0, 0)
    # do_Type, pad, DefaultTool, ToolTypes, CurrentX, CurrentY, DrawerData, ToolWindow, StackSize
    d += struct.pack('>BBIIiiIII', kind, 0, 1 if default_tool else 0, 1 if tooltypes else 0,
                     -2147483648, -2147483648, 1 if drawer else 0, 0, stack)
    assert len(d) == 78, len(d)
    if drawer:
        # window block of a typical drawer (copied from AmigaOS 3.2's Install.info), size/position patched
        nw = bytearray(bytes.fromhex('00600011020c00b6ffff000000000200107f0032bd7600000000002100f4000000000000000000640046ffffffff0001'))
        struct.pack_into('>hhhh', nw, 0, 40, 40, 360, 170)
        nw = bytes(nw)
        assert len(nw) == 48, len(nw)
        d += nw + struct.pack('>ii', 0, 0)
    d += image_struct(img)
    if sel:
        d += image_struct(sel)
    if drawer:
        d += struct.pack('>IH', 1, 0)               # DrawerData2: flags, view modes
    if default_tool:
        b = default_tool.encode('latin-1') + b'\0'
        d += struct.pack('>I', len(b)) + b
    if tooltypes:
        d += struct.pack('>I', (len(tooltypes) + 1) * 4)
        for t in tooltypes:
            b = t.encode('latin-1') + b'\0'
            d += struct.pack('>I', len(b)) + b
    return d

def inverted(img):
    """selected look: swap black and white, keep the rest"""
    m = {G: G, K: W, W: K, B: B}
    out = img.copy()
    out.putdata([m[p] for p in img.getdata()])
    return out

def skyline(w, h, drawer=False):
    img = Image.new('P', (w, h), G)
    d = ImageDraw.Draw(img)
    base = h - 4
    # sky box
    d.rectangle([0, 0, w - 1, h - 1], outline=K)
    d.rectangle([1, 1, w - 2, base - 1], fill=B)
    # ground and road
    d.rectangle([1, base, w - 2, h - 2], fill=K)
    d.line([3, h - 3, w - 4, h - 3], fill=W)
    # buildings: (x, width, height, windows)
    blds = [(4, 8, 10), (14, 10, 22), (26, 9, 15), (37, 12, 26), (51, 8, 12)]
    sx = max(1, (w - 8) / 60.0)
    for (x, bw, bh) in blds:
        x = int(x * sx); bw = int(bw * sx)
        bh = min(bh + (h - 32) , base - 3)
        if x + bw >= w - 2:
            continue
        d.rectangle([x, base - bh, x + bw, base - 1], fill=W, outline=K)
        for wy in range(base - bh + 3, base - 3, 4):
            for wx in range(x + 2, x + bw - 1, 3):
                d.point([wx, wy], fill=K)
    # power-plant chimney with smoke at the right
    d.rectangle([w - 9, base - 8, w - 5, base - 1], fill=K)
    d.point([w - 7, base - 11], fill=W); d.point([w - 6, base - 13], fill=W)
    if drawer:
        d.rectangle([0, 0, w - 1, 3], fill=K)          # drawer lid
        d.rectangle([w // 2 - 8, 1, w // 2 + 8, 2], fill=W)
    img.putpalette([c for rgb in PAL for c in rgb] + [0] * (256 * 3 - 12))
    return img

def document(w, h):
    img = Image.new('P', (w, h), G)
    d = ImageDraw.Draw(img)
    d.rectangle([14, 1, w - 15, h - 2], fill=W, outline=K)
    for y in range(6, h - 5, 3):
        d.line([18, y, w - 19, y], fill=B if (y // 3) % 4 == 0 else K)
    img.putpalette([c for rgb in PAL for c in rgb] + [0] * (256 * 3 - 12))
    return img

def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    tool = skyline(64, 36)
    drawer = skyline(64, 36, drawer=True)
    doc = document(54, 30)
    files = {
        'Micropolis.info':        disk_object(3, tool, inverted(tool), tooltypes=('CITY=cities/haight.cty', '(SIZE=800x600)', '(MODE=ASK)', '(DEBUG)'), stack=16384),
        'Micropolis-drawer.info': disk_object(2, drawer, inverted(drawer), drawer=True),
        'ReadMe.info':            disk_object(4, doc, inverted(doc), default_tool='SYS:Utilities/MultiView'),
    }
    for name, data in files.items():
        open(os.path.join(out, name), 'wb').write(data)
    if len(sys.argv) > 2:
        sheet = Image.new("RGB", (5 * 280 + 20, 36 * 4 + 20), (170, 170, 170))
        for i, im in enumerate([tool, inverted(tool), drawer, inverted(drawer), doc]):
            rgb = im.convert('RGB').resize((im.size[0] * 4, im.size[1] * 4), Image.NEAREST)
            sheet.paste(rgb, (10 + i * 280, 10))
        sheet.save(sys.argv[2])
    print('icons:', ', '.join('%s %d bytes' % (k, len(v)) for k, v in files.items()))

main()
