#!/usr/bin/env python3
"""Screen tests in the emulator. Each case starts the game (FS-UAE in Docker, tools/emusys.sh) with debug arguments, the game writes
its exact frame buffer and the panel buffer (dump=...), and the test looks for texts in them: the texts are rendered with the game's
own 8x8 font and searched pixel for pixel, so no OCR and no screenshots are needed.  Also checks the log for a clean shutdown.

usage: screens.py [--quick] [case ...]      (needs Docker running; about 40 s per case)
"""
import os
import re
import struct
import subprocess
import sys

import numpy as np
from scipy.signal import fftconvolve

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
AMI = os.path.join(ROOT, 'src', 'amiga')


def load_font():
    def table(path, rows):
        txt = open(path, encoding='latin-1').read()
        return [[int(x, 16) for x in g.split(',')] for g in re.findall(r'\{\s*(0x[0-9A-Fa-f]{2}(?:,\s*0x[0-9A-Fa-f]{2}){7})\s*\}', txt)]
    base = table(os.path.join(AMI, 'font8x8_basic.h'), 128)
    lat = open(os.path.join(AMI, 'font8x8_latin1.h'), encoding='latin-1').read()
    codes = [int(c, 16) for c in re.search(r'latin1_codes\[\d+\] = \{([^}]*)\}', lat).group(1).replace(' ', '').split(',')]
    glyphs = table(os.path.join(AMI, 'font8x8_latin1.h'), 0)
    return base, dict(zip(codes, glyphs))


BASE, LATIN = load_font()


def mask(text):
    cols = []
    for ch in text.encode('latin-1'):
        g = BASE[ch] if ch < 128 else LATIN.get(ch, BASE[ord('?')])
        cols.append(np.array([[(row >> i) & 1 for i in range(8)] for row in g], dtype=np.uint8))
    return np.concatenate(cols, axis=1)


def find_text(plane, text):
    """True if the text is drawn somewhere in the 2-D uint8 array with the game's font (any foreground colour)"""
    m = mask(text)
    h, w = m.shape
    need = int(m.sum())
    for colour in np.unique(plane):
        b = (plane == colour).astype(np.float32)
        if b.sum() < need:
            continue
        on = np.rint(fftconvolve(b, m[::-1, ::-1].astype(np.float32), mode='valid'))      # mask pixels drawn in this colour
        off = np.rint(fftconvolve(b, (1 - m)[::-1, ::-1].astype(np.float32), mode='valid'))  # no such colour where the mask is empty
        if np.any((on == need) & (off == 0)):
            return True
    return False


def read_dump(path):
    d = open(path, 'rb').read()
    magic, w, h, sw, sh, _ = struct.unpack('>6H', d[:12])
    assert magic == 0x4d50, 'bad dump'
    fb = np.frombuffer(d[12:12 + w * h], dtype=np.uint8).reshape(h, w)
    scr = np.frombuffer(d[12 + w * h:12 + w * h + sw * sh], dtype=np.uint8).reshape(sh, sw)
    return fb, scr


def run_case(args, timeout=60):
    out = os.path.join(ROOT, 'out', 'syswork')
    cmd = 'Work:Micropolis/micropolis size=640x480 debug %s det dump=Work:dump.raw quit=14' % args
    subprocess.run([os.path.join(ROOT, 'tools', 'emusys.sh'), cmd, str(timeout)], cwd=ROOT, check=False,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    dump = os.path.join(out, 'dump.raw')
    log = open(os.path.join(out, 'Micropolis', 'micropolis.log'), encoding='latin-1').read() if os.path.exists(os.path.join(out, 'Micropolis', 'micropolis.log')) else ''
    if not os.path.exists(dump):
        return None, None, log
    fb, scr = read_dump(dump)
    return fb, scr, log


# (name, game arguments, [(plane, text, must_be_present)])    plane: fb = the picture, scr = the panel window, log = the log file
CASES = [
    ('start-en', 'show=none', [('fb', 'Speed 2', True), ('fb', '(Pause)', False), ('fb', 'Funds:', True), ('fb', 'Query', True)]),
    ('pause', 'show=none speed=0', [('fb', 'Speed 0', True), ('fb', '(Pause)', True)]),
    ('hover', 'show=none hover=7', [('fb', 'Bulldozer', True), ('fb', 'Cost: $1', True)]),
    ('budget-en', 'show=budget', [('scr', 'Taxes collected', True), ('scr', '$1,000,000', True), ('scr', 'Continue', True)]),
    ('budget-de', 'show=budget lang=deutsch', [('scr', 'Steuereinnahmen', True), ('scr', '$1,000,000', True), ('scr', 'Weiter', True)]),
    ('eval-en', 'show=eval', [('scr', 'City Evaluation', True), ('scr', 'Population', True), ('scr', 'Statistics:', True)]),
    ('eval-de', 'show=eval lang=deutsch', [('scr', 'Stadtbewertung', True), ('scr', 'Einwohner', True)]),
    ('graph-pl', 'show=graph lang=polski', [('scr', 'Wykresy', True), ('scr', 'Zamknij', True)]),
    ('status-de', 'show=none lang=deutsch', [('fb', 'Tempo 2', True), ('fb', 'Geld:', True), ('fb', 'Abfrage', True)]),
    ('save', 'show=none savetest', [('log', 'Saved PROGDIR:cities/savetest.cty', True)]),
]
QUICK = ['start-en', 'budget-en', 'status-de']


def main():
    names = [a for a in sys.argv[1:] if not a.startswith('--')]
    cases = [c for c in CASES if (not names and ('--quick' not in sys.argv or c[0] in QUICK)) or c[0] in names]
    failed = 0
    for name, args, checks in cases:
        fb, scr, log = run_case(args)
        problems = []
        if 'main() is returning' not in log:
            problems.append('no clean shutdown in the log')
        for plane, text, present in checks:
            if plane == 'log':
                found = text in log
            elif fb is None:
                found = False
            else:
                found = find_text(fb if plane == 'fb' else scr, text)
            if found != present:
                problems.append('%s %r %s' % (plane, text, 'missing' if present else 'present but should not be'))
        print('%-10s %s' % (name, 'ok' if not problems else 'FAIL: ' + '; '.join(problems)))
        failed += bool(problems)
    print('%d of %d screen tests failed' % (failed, len(cases)) if failed else 'all %d screen tests passed' % len(cases))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
