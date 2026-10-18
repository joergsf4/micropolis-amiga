"""Alternative tool palette icons for Micropolis: uniform 32x32 tiles drawn as ASCII pixel art.

Method as in ~/dev/sprites (pixelart.py): every character is a pixel, '.' is transparent, letters are colours.
Each glyph is cropped and centred on a tile, the tile colour says which group the tool belongs to
(zones, transport, power, services, buildings, tools), one dark outline for all.

usage: python3 toolicons.py PREVIEW.png [UPSTREAM_IMAGES_DIR]     (writes a preview sheet, optionally with the original icons for comparison)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image
from pixelart import sprite

T = 32                                   # tile size

# glyph colours (letters) -------------------------------------------------
PAL = {
    'k': (24, 22, 32),                   # outline
    'w': (252, 252, 248), 'e': (196, 198, 204), 'E': (110, 112, 124),
    'r': (222, 64, 52), 'R': (150, 36, 40),
    'o': (255, 150, 40), 'y': (255, 218, 60), 'Y': (200, 150, 30),
    'g': (80, 190, 80), 'G': (36, 122, 56), 'l': (160, 230, 120),
    'b': (70, 120, 220), 'B': (30, 60, 140), 'q': (150, 210, 245),
    'd': (150, 98, 52), 'D': (96, 60, 34),
    'p': (244, 150, 170), 'm': (196, 96, 140),
}

# tile colours per group: (background, lighter edge, darker edge)
GROUP = {
    'zone_r': ((46, 140, 70), (90, 190, 110), (24, 90, 44)),
    'zone_c': ((56, 98, 200), (110, 150, 240), (30, 56, 140)),
    'zone_i': ((196, 160, 40), (240, 210, 90), (130, 100, 20)),
    'transport': ((120, 122, 134), (170, 172, 184), (70, 72, 84)),
    'power': ((200, 110, 40), (240, 160, 90), (130, 66, 20)),
    'service': ((40, 140, 140), (90, 195, 190), (20, 90, 92)),
    'building': ((84, 150, 205), (140, 195, 235), (44, 96, 150)),
    'tool': ((150, 140, 170), (200, 192, 215), (92, 84, 112)),
}


def sym(rows, odd=False):
    """mirror the left half: every row is mirrored to the right (odd=True shares the centre column)"""
    out = []
    for r in rows:
        right = r[::-1]
        out.append(r + (right[1:] if odd else right))
    return out


def stack(*parts):
    out = []
    for p in parts:
        out += p
    return out


def compose(w, h, parts):
    """place several row lists on one canvas: parts = [(rows, x, y)], later parts on top ('.' keeps what is below)"""
    cv = [['.'] * w for _ in range(h)]
    for rows, x, y in parts:
        for j, r in enumerate(rows):
            for i, ch in enumerate(r):
                if ch != '.' and 0 <= y + j < h and 0 <= x + i < w:
                    cv[y + j][x + i] = ch
    return [''.join(r) for r in cv]


# glyphs ------------------------------------------------------------------
GLYPH = {}

GLYPH['res'] = sym([                      # house
    "..........kk",
    ".........kRR",
    "........kRRR",
    ".......kRRRR",
    "......kRRRRR",
    ".....kRRRRRR",
    "....kRRRRRRR",
    "...kRRRRRRRR",
    "..kRRRRRRRRR",
    ".kkkkkkkkkkk",
    "..kwwwwwwwww",
    "..kwwwwwwwww",
    "..kwwbbbbwww",
    "..kwwbqqbwww",
    "..kwwbbbbwww",
    "..kwwwwwwwkk",
    "..kwwwwwwwkd",
    "..kwwwwwwwkd",
    "..kkkkkkkkkk",
])

_left = ["kkkkkkkk"] + ["kebebeek", "keeeeeek"] * 7 + ["kkkkkkkk"]
_right = ["kkkkkkk"] + ["kwbwbwk", "kwwwwwk"] * 5 + ["kwwkwwk", "kkkkkkk"]
GLYPH['com'] = compose(18, 17, [(_left, 1, 0), (_right, 10, 5)])

GLYPH['ind'] = [                          # factory with chimneys and smoke
    "...ww..........ww...",
    "..wwwww.......wwww..",
    "...ww..........ww...",
    "...kk..........kk...",
    "...kEk.........kEk..",
    "...kEk.........kEk..",
    "...kEk.........kEk..",
    "...kEk..kk.kk..kEk..",
    "k..kEk.kEkkEEk.kEk.k",
    "kk.kEk.kEEkEEk.kEkkk",
    "kEkkEkkkEEkEEkkkEkEk",
    "kEEEEEEEEEEEEEEEEEEk",
    "kEeeEeeEeeEeeEeeEeEk",
    "kEeyEeyEeeEeeEeyEeEk",
    "kEeyEeyEeeEeeEeyEeEk",
    "kEEEEEEEEEEEEEEEEEEk",
    "kkkkkkkkkkkkkkkkkkkk",
]

GLYPH['fire'] = sym([                     # flame
    ".........kk",
    "........krr",
    ".......krrr",
    ".......krrr",
    "......krrro",
    ".....krrroo",
    "....krrrooo",
    "...krrrrooy",
    "...krrroyyy",
    "..krrrooyyy",
    "..krrroyyyw",
    "..krrooyyww",
    "..krrooyyww",
    "..krrroyyww",
    "...krrooyyy",
    "....krrooyy",
    ".....kkrroo",
    ".......kkkk",
])

GLYPH['qry'] = [                          # magnifier
    "......kkkkkk........",
    "....kkqqqqqqkk......",
    "...kqqwwqqqqqqk.....",
    "..kqqwwqqqqqqqqk....",
    "..kqwqqqqqqqqqqk....",
    ".kqqqqqqqqqqqqqqk...",
    ".kqqqqqqqqqqqqqqk...",
    ".kqqqqqqqqqqqqqqk...",
    "..kqqqqqqqqqqqqk....",
    "..kqqqqqqqqqqqqk....",
    "...kqqqqqqqqqkkk....",
    "....kkkkkkkkkDDk....",
    ".............kDDk...",
    "..............kDDk..",
    "...............kDDk.",
    "................kDDk",
    ".................kkk",
]

GLYPH['pol'] = sym([                      # badge with star
    "kkkkkkkkkkkk",
    "kBBBBBBBBBBB",
    "kBBBBBBBBByB",
    "kBBBBBBBByyy",
    "kBBBBBBBByyy",
    "kBByyyyyyyyy",
    "kBBByyyyyyyy",
    "kBBBByyyyyyy",
    "kBBBBByyyyyy",
    "kBBBByyyyyyy",
    "kBBBByyyBByy",
    "kBBByyBBBBBB",
    "kBBByBBBBBBB",
    ".kBBBBBBBBBB",
    "..kBBBBBBBBB",
    "...kBBBBBBBB",
    "....kkBBBBBB",
    "......kkkkkk",
])

GLYPH['wire'] = [                         # pylon with wires
    ".........kk.........",
    "........kEEk........",
    "........kEEk........",
    ".....kkkkEEkkkkk....",
    "...kkyyykEEkyyykk...",
    "..k..kkkkEEkkkk..k..",
    ".k.....kEeEEk.....k.",
    ".k.....kEeEEk.....k.",
    "k....kkkEeEEkkk....k",
    "k...kyyykEEkyyyk...k",
    "....kkkkkEEkkkkk....",
    "......kEEeEEEk......",
    "......kEEeEEEk......",
    ".....kEEEeEEEEk.....",
    ".....kEEEeEEEEk.....",
    "....kEEEEeEEEEEk....",
    "....kEEEEeEEEEEk....",
    "...kkkkkkkkkkkkkk...",
]

GLYPH['dozr'] = [                         # bulldozer
    "kk..........kkkkkkkk...",
    "kYk.........kyyyyyyk...",
    "kYk.........kyqqqqyk...",
    "kYk.........kyqqqqyk...",
    "kYkkkkkkkkkkkyyyyyykkk.",
    "kYYYYYYYYkyyyyyyyyyyyk.",
    "kYYYYYYYYkyyyyyyyyyyyk.",
    "kYk.kkkkkkkkkkkkkkkkkk.",
    "kYkkkEEEEEEEEEEEEEEEEk.",
    "kkkEkEeEkEeEkEeEkEeEEk.",
    "...kEEEEEEEEEEEEEEEEk..",
    "....kkkkkkkkkkkkkkkk...",
]

GLYPH['rail'] = [                         # tracks
    "..kk............kk..",
    ".kewk..........kewk.",
    "kddddddddddddddddddk",
    "kDDDDDDDDDDDDDDDDDDk",
    ".kewk..........kewk.",
    ".kewk..........kewk.",
    "kddddddddddddddddddk",
    "kDDDDDDDDDDDDDDDDDDk",
    ".kewk..........kewk.",
    ".kewk..........kewk.",
    "kddddddddddddddddddk",
    "kDDDDDDDDDDDDDDDDDDk",
    ".kewk..........kewk.",
    ".kewk..........kewk.",
    "kddddddddddddddddddk",
    "kDDDDDDDDDDDDDDDDDDk",
    ".kewk..........kewk.",
    "..kk............kk..",
]

GLYPH['road'] = [                         # road
    "kkkkkkkkkkkkkkkkkk",
    "kwEEEEEEyyEEEEEEwk",
    "kwEEEEEEyyEEEEEEwk",
    "kwEEEEEEyyEEEEEEwk",
    "kwEEEEEEEEEEEEEEwk",
    "kwEEEEEEEEEEEEEEwk",
    "kwEEEEEEyyEEEEEEwk",
    "kwEEEEEEyyEEEEEEwk",
    "kwEEEEEEyyEEEEEEwk",
    "kwEEEEEEEEEEEEEEwk",
    "kwEEEEEEEEEEEEEEwk",
    "kwEEEEEEyyEEEEEEwk",
    "kwEEEEEEyyEEEEEEwk",
    "kwEEEEEEyyEEEEEEwk",
    "kwEEEEEEEEEEEEEEwk",
    "kwEEEEEEEEEEEEEEwk",
    "kwEEEEEEyyEEEEEEwk",
    "kwEEEEEEyyEEEEEEwk",
    "kkkkkkkkkkkkkkkkkk",
]

GLYPH['hwy'] = [                          # highway: wide carriageway with a median and an exit ramp
    "kkkkkkkkkkkkkkkkkkkk",
    "kyEEEEEEEEwwEEEEEEyk",
    "kyEEEEEEEEwwEEEEEEyk",
    "kywEEEEEEEEEEEEEEwyk",
    "kyEEEEEEEEwwEEEEEEyk",
    "kyEEEEEEEEwwEEEEEEyk",
    "kyEEEEwEEEEEEEEwEEyk",
    "kyEEEEwEEEeeEEEwEEyk",
    "kyEEEEEEEEeeEEEEEEyk",
    "kyEEEEEEEEeeEEEEEEyk",
    "kywEEEEEEEEEEEEEEwyk",
    "kyEEEEwEEEeeEEEwEEyk",
    "kyEEEEwEEEEEEEEwEEyk",
    "kyEEEEEEEEeeEEEEEEyk",
    "kyEEEEEEEEeeEEEEEEyk",
    "kywEEEEEEEEEEEEEEwyk",
    "kyEEEEEEEEwwEEEEEEyk",
    "kkkkkkkkkkkkkkkkkkkk",
]

GLYPH['chlk'] = [                         # chalk
    "..............kkk.",
    ".............kwwwk",
    "............kwwwwk",
    "...........kwwwwk.",
    "..........kwwwwk..",
    ".........kwwwwk...",
    "........kwwwwk....",
    ".......kwwwwk.....",
    "......kwwwwk......",
    ".....kwwwwk.......",
    "....kwwwwk........",
    "...kwwwwk.........",
    "..kwwwwk..........",
    ".kwwwwk...........",
    ".kwwwk.....kkkkkk.",
    "..kkk.....kwwwwwwk",
    "..........kkkkkkkk",
]

GLYPH['ersr'] = sym([                     # eraser
    "..kkkkkkkkkkkk",
    ".kppppppppppkk",
    "kppppppppppkbb",
    "kpppppppppkbbb",
    "kpppppppppkbbb",
    "kpppppppppkbbb",
    "kppppppppmkbbb",
    "kmmmmmmmmmkbbb",
    ".kmmmmmmmmkkkk",
    "..kkkkkkkkk...",
])
# the eraser is not symmetric: right half is blue
GLYPH['ersr'] = [
    "..kkkkkkkkkkkkkkkkkkk.",
    ".kppppppppkbbbbbbbbbbk",
    "kppppppppkbbbbbbbbbbbk",
    "kppppppppkbbbbbbbbbbk.",
    "kppppppppkbbbbbbbbbbk.",
    "kppppppppkbbbbbbbbbbk.",
    "kmmmmmmmmkBBBBBBBBBBk.",
    ".kmmmmmmmkkkkkkkkkkkk.",
    "..kkkkkkkk............",
]
GLYPH['ersr'] = [r.ljust(22, '.') for r in GLYPH['ersr']]

GLYPH['stad'] = [                         # stadium
    "....kkkkkkkkkkkkkkkkkk....",
    "..kkeeeeeeeeeeeeeeeeeekk..",
    ".keeeeeeeeeeeeeeeeeeeeeek.",
    "keeeekkkkkkkkkkkkkkkkeeeek",
    "keeekgggggggggggggggkeeeek",
    "keekgggggwwggggwwggggkeeek",
    "keekgggggwwggggwwggggkeeek",
    "keeekgggggggggggggggkeeeek",
    "keeeekkkkkkkkkkkkkkkkeeeek",
    ".keeeeeeeeeeeeeeeeeeeeeek.",
    "..kkeeeeeeeeeeeeeeeeeekk..",
    "....kkkkkkkkkkkkkkkkkk....",
]

GLYPH['park'] = sym([                     # tree
    ".....kkkkk",
    "...kkgggggg",
    "..kgggggggg",
    ".kgglggggGg",
    ".kggggggggg",
    "kggggGggggg",
    "kgggggggggG",
    "kgggggggGgg",
    ".kggglgggGg",
    ".kgggggggGg",
    "..kkgggggGG",
    "....kkGGGGG",
    "......kkddd",
    ".......kdDD",
    ".......kdDD",
    ".......kdDD",
    "....kkkkkkk",
])

GLYPH['seap'] = sym([                     # anchor
    "..........kk",
    ".........kEE",
    "........kE.k",
    ".........kEE",
    "......kkkkEE",
    "......kEEEEE",
    "......kkkkEE",
    ".........kEE",
    ".........kEE",
    ".........kEE",
    "..k......kEE",
    ".kEk.....kEE",
    ".kEEk....kEE",
    ".kEEEkkkkkEE",
    "..kEEEEEEEEE",
    "...kkEEEEEEE",
    ".....kkkEEEE",
], odd=False)

_heap = [
    "..........kk..........",
    ".........kEEk.........",
    "........kEeEEk........",
    ".......kEEEEeEk.......",
    "......kEeEEEEEEk......",
    ".....kEEEEEEeEEEk.....",
    "....kEEeEEEEEEEEEk....",
    "...kEEEEEEEeEEEEEEk...",
    "..kEEeEEEEEEEEeEEEEk..",
    ".kEEEEEEeEEEEEEEEEEEk.",
    ".kkkkkkkkkkkkkkkkkkkk.",
]
_bolt = [
    "...kkkk",
    "..kyyyk",
    ".kyyyk.",
    "kyyyyyk",
    ".kkyyk.",
    "..kyk..",
    "..kk...",
]
GLYPH['coal'] = compose(22, 17, [(_heap, 0, 6), (_bolt, 15, 0)])

GLYPH['nuc'] = sym([                      # cooling tower with steam
    "....www.ww",
    "...wwwwwww",
    "....wwwwww",
    "..kkkkkkkk",
    "..keeeeeee",
    "...keeeeee",
    "...keeeeee",
    "....keeeee",
    "....keeeee",
    ".....keeee",
    ".....keeee",
    "....keeeee",
    "...keeeeee",
    "..kkeeeeee",
    ".kkEEEEEEE",
    ".kEEEEEEEE",
    ".kkkkkkkkk",
])

GLYPH['airp'] = sym([                     # plane from above
    "..........kk",
    ".........kww",
    ".........kww",
    ".........kww",
    "........kkww",
    "........kwww",
    "....kkkkkwww",
    "..kkwwwwwwww",
    ".kwwwwwwwwww",
    "kkkkkkwwwwww",
    "......kwwwww",
    "......kwwwww",
    ".......kkwww",
    ".........kww",
    "......kkkkww",
    ".....kwwwwww",
    ".....kkkkkkk",
])

ORDER = ['res', 'com', 'ind', 'fire', 'qry', 'pol', 'wire', 'dozr', 'rail', 'road', 'chlk', 'ersr',
         'stad', 'park', 'seap', 'coal', 'nuc', 'airp', 'hwy']
GROUP_OF = {'res': 'zone_r', 'com': 'zone_c', 'ind': 'zone_i', 'fire': 'service', 'pol': 'service', 'stad': 'service', 'park': 'service',
            'qry': 'tool', 'dozr': 'tool', 'chlk': 'tool', 'ersr': 'tool',
            'wire': 'power', 'coal': 'power', 'nuc': 'power',
            'rail': 'transport', 'road': 'transport', 'hwy': 'transport', 'seap': 'building', 'airp': 'building'}
NAME = {'res': 'Residential', 'com': 'Commercial', 'ind': 'Industrial', 'fire': 'Fire Station', 'qry': 'Query', 'pol': 'Police Station',
        'wire': 'Wire', 'dozr': 'Bulldozer', 'rail': 'Rail', 'road': 'Road', 'chlk': 'Chalk', 'ersr': 'Eraser',
        'stad': 'Stadium', 'park': 'Park', 'seap': 'Seaport', 'coal': 'Coal Power', 'nuc': 'Nuclear Power', 'airp': 'Airport', 'hwy': 'Highway'}


def glyph_image(name):
    rows = GLYPH[name]
    w = max(len(r) for r in rows)
    rows = [r.ljust(w, '.') for r in rows]
    img = sprite(rows, PAL)
    bb = img.getbbox()
    return img.crop(bb)


def tile(name, selected=False):
    bg, light, dark = GROUP[GROUP_OF[name]]
    if selected:
        bg = tuple(min(255, c + 60) for c in bg)
    im = Image.new('RGBA', (T, T), (*PAL['k'], 255))
    px = im.load()
    for y in range(1, T - 1):
        for x in range(1, T - 1):
            c = bg
            if x == 1 or y == 1:
                c = light
            if x == T - 2 or y == T - 2:
                c = dark
            px[x, y] = (*c, 255)
    g = glyph_image(name)
    gw, gh = g.size
    assert gw <= T - 6 and gh <= T - 6, (name, g.size)
    im.alpha_composite(g, ((T - gw) // 2, (T - gh) // 2))
    if selected:                                         # white frame inside the outline
        for i in range(1, T - 1):
            for (x, y) in ((i, 1), (i, T - 2), (1, i), (T - 2, i)):
                px[x, y] = (252, 252, 248, 255)
    return im


def preview(path, up=None):
    S = 4                                                # preview zoom
    gap = 8
    cols = 3
    rows = 6
    pw = cols * (T + gap) + gap
    ph = rows * (T + gap) + gap
    new = Image.new('RGBA', (pw, ph), (32, 36, 80, 255))
    sel = Image.new('RGBA', (pw, ph), (32, 36, 80, 255))
    for i, n in enumerate(ORDER):
        x = gap + (i % cols) * (T + gap)
        y = gap + (i // cols) * (T + gap)
        new.alpha_composite(tile(n), (x, y))
        sel.alpha_composite(tile(n, selected=(n == 'dozr')), (x, y))
    panels = [new]
    if up:
        sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
        old = Image.new('RGBA', (144, 410), (32, 36, 80, 255))
        pos = {'res': (13, 0), 'com': (51, 0), 'ind': (89, 0), 'fire': (13, 54), 'qry': (51, 54), 'pol': (89, 54),
               'wire': (32, 92), 'dozr': (70, 92), 'rail': (10, 130), 'road': (70, 130), 'chlk': (32, 158), 'ersr': (70, 158),
               'stad': (5, 196), 'park': (51, 196), 'seap': (89, 196), 'coal': (5, 242), 'nuc': (89, 242), 'airp': (39, 288)}
        import re
        for n, (x, y) in pos.items():
            w, h, pix = _xpm(os.path.join(up, 'ic%s.xpm' % n))
            for yy in range(h):
                for xx in range(w):
                    c = pix[yy][xx]
                    if c:
                        old.putpixel((x + xx, y + yy), (*c, 255))
        panels.insert(0, old)
    panels.append(sel)
    W = sum(p.width * S for p in panels) + 20 * (len(panels) + 1)
    H = max(p.height * S for p in panels) + 40
    sheet = Image.new('RGBA', (W, H), (16, 18, 40, 255))
    x = 20
    for p in panels:
        sheet.alpha_composite(p.resize((p.width * S, p.height * S), Image.NEAREST), (x, 20))
        x += p.width * S + 20
    sheet.convert('RGB').save(path)
    # 1:1 strip as the game would show it (a column of tiles, next to the map colours)
    print('preview written:', path, sheet.size)


def _xpm(path):
    import re
    txt = open(path, encoding='latin-1').read()
    rows = re.findall(r'"([^"]*)"', txt)
    w, h, ncol, cpp = map(int, rows[0].split())
    cmap = {}
    for r in rows[1:1 + ncol]:
        key, rest = r[:cpp], r[cpp:]
        v = re.search(r'\bc\s+(\S+)', rest).group(1)
        if v.lower() == 'none':
            cmap[key] = None
        else:
            hx = v[1:]
            n = len(hx) // 3
            cmap[key] = tuple(int(hx[i * n:(i + 1) * n][:2], 16) for i in range(3))
    pix = [[cmap[r[x * cpp:(x + 1) * cpp]] for x in range(w)] for r in rows[1 + ncol:1 + ncol + h]]
    return w, h, pix


if __name__ == '__main__':
    preview(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
