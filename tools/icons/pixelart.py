"""Pixel-art helpers: ASCII rows -> RGBA images, mirroring, 1 px outline.

Every character is a letter from PAL ('.' = transparent); a sprite can bring
its own small palette that overrides or extends PAL.
"""
from __future__ import annotations

from PIL import Image

# --------------------------------------------------------------------------
# Palette (slightly desaturated, warm)
# --------------------------------------------------------------------------
PAL = {
    '.': None,
    'k': (34, 30, 40), 'x': (58, 48, 66), 'w': (248, 246, 240),
    'r': (214, 78, 64), 'R': (150, 44, 44),
    'b': (132, 92, 58), 'B': (92, 62, 40), 'c': (240, 218, 180), 'C': (214, 186, 146),
    'n': (48, 60, 104), 'N': (34, 42, 76), 'y': (246, 200, 84), 'Y': (255, 236, 160),
    'g': (106, 170, 90), 'G': (70, 126, 68), 'l': (152, 204, 112), 'L': (190, 226, 150),
    'o': (236, 142, 62), 'O': (176, 92, 40),
    's': (232, 214, 170), 'S': (206, 184, 140), 'd': (176, 160, 128),
    'a': (96, 176, 214), 'A': (52, 118, 176), 'q': (150, 212, 232),
    'p': (240, 160, 176), 'e': (160, 162, 168), 'E': (104, 106, 114), 'f': (196, 198, 202),
    't': (72, 160, 150), 'T': (46, 112, 106), 'P': (252, 248, 255), 'm': (206, 96, 128), 'M': (160, 62, 96),
    'v': (128, 96, 168), 'h': (120, 78, 52), 'i': (255, 250, 230),
}


def sprite(rows, pal=None):
    """Build an RGBA image from ASCII rows (ragged rows are right-padded)."""
    p = dict(PAL)
    if pal:
        p.update(pal)
    w = max(len(r) for r in rows)
    h = len(rows)
    img = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    px = img.load()
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            col = p.get(ch)
            if col is not None:
                px[x, y] = (*col, 255)
    return img


def mirror(img):
    return img.transpose(Image.FLIP_LEFT_RIGHT)


def _outlined(img: Image.Image) -> Image.Image:
    """1 px dark outline (4-neighbourhood) around the opaque pixels, canvas 2 px larger."""
    w, h = img.size
    out = Image.new('RGBA', (w + 2, h + 2), (0, 0, 0, 0))
    out.alpha_composite(img, (1, 1))
    src, px = img.load(), out.load()
    for y in range(h + 2):
        for x in range(w + 2):
            if px[x, y][3]:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                sx, sy = x - 1 + dx, y - 1 + dy
                if 0 <= sx < w and 0 <= sy < h and src[sx, sy][3]:
                    px[x, y] = (*PAL['x'], 255)
                    break
    return out
