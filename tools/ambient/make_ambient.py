#!/usr/bin/env python3
"""Cuts the downloaded ambient recordings (src/, see SOURCES.md) into short seamless loops and writes IFF 8SVX files
(8 bit mono 11025 Hz) to ../../assets/ambient/. Needs ffmpeg and numpy.

  python3 make_ambient.py            -> assets/ambient/amb_sea.8svx amb_city.8svx amb_nature.8svx (+ WAV previews in out/ambient/)

Per source: the steadiest window of LOOP seconds (lowest relative change of the loudness), then an equal-power
crossfade of its tail into its head, so the end continues into the start without a click.
"""
import os
import struct
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
RATE = 11025
SOURCES = {                    # name: (file in src/, loop seconds, crossfade seconds)
    'amb_sea': ('vistula_short.mp3', 16, 3),
    'amb_city': ('traffic.ogg', 14, 2),
    'amb_nature': ('birds_wind.ogg', 16, 3),
}


def decode(path):
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-ac', '1', '-ar', str(RATE), '-f', 'f32le', '-'],
                         check=True, stdout=subprocess.PIPE).stdout
    return np.frombuffer(raw, dtype='<f4').astype(np.float64)


def steadiest(x, seconds, fade):
    n, frame = int((seconds + fade) * RATE), RATE // 4
    nf = len(x) // frame
    rms = np.sqrt(np.mean(x[:nf * frame].reshape(nf, frame) ** 2, axis=1)) + 1e-9
    w = int((seconds + fade) * 4)
    best, at = None, 0
    for i in range(0, nf - w):
        seg = rms[i:i + w]
        score = np.std(seg) / np.mean(seg)
        if best is None or score < best:
            best, at = score, i
    return x[at * frame:at * frame + n], at / 4.0, best


def loop(seg, seconds, fade):
    n, f = int(seconds * RATE), int(fade * RATE)
    out = seg[:n].copy()
    t = np.linspace(0, np.pi / 2, f)
    out[:f] = seg[:f] * np.sin(t) + seg[n:n + f] * np.cos(t)        # tail fades out while the head fades in
    return out


def to_8bit(x, rng):
    x = x - np.mean(x)
    x = x / (np.max(np.abs(x)) + 1e-9) * 0.9
    q = np.round(x * 127 + (rng.random(len(x)) - rng.random(len(x))))     # TPDF dither
    return np.clip(q, -127, 127).astype(np.int8).tobytes()


def iff_8svx(samples, rate):
    vhdr = struct.pack('>IIIHBBI', len(samples), 0, 0, rate, 1, 0, 0x10000)
    body = samples + (b'\0' if len(samples) & 1 else b'')
    chunks = b'VHDR' + struct.pack('>I', len(vhdr)) + vhdr + b'BODY' + struct.pack('>I', len(samples)) + body
    return b'FORM' + struct.pack('>I', 4 + len(chunks)) + b'8SVX' + chunks


def main():
    rng = np.random.default_rng(1)
    outdir = os.path.join(ROOT, 'assets', 'ambient')
    prev = os.path.join(ROOT, 'out', 'ambient')
    os.makedirs(outdir, exist_ok=True)
    os.makedirs(prev, exist_ok=True)
    for name, (src, secs, fade) in SOURCES.items():
        x = decode(os.path.join(HERE, 'src', src))
        seg, at, score = steadiest(x, secs, fade)
        lp = loop(seg, secs, fade)
        data = to_8bit(lp, rng)
        open(os.path.join(outdir, name + '.8svx'), 'wb').write(iff_8svx(data, RATE))
        # WAV preview: the loop played three times (to hear the seam)
        pcm = np.tile((np.frombuffer(data, dtype=np.int8).astype(np.int16) * 256), 3).astype('<i2').tobytes()
        with open(os.path.join(prev, name + '_x3.wav'), 'wb') as f:
            f.write(b'RIFF' + struct.pack('<I', 36 + len(pcm)) + b'WAVEfmt ' + struct.pack('<IHHIIHH', 16, 1, 1, RATE, RATE * 2, 2, 16) + b'data' + struct.pack('<I', len(pcm)) + pcm)
        print('%-11s from %-18s at %6.1f s (steadiness %.3f)  %d KB' % (name, src, at, score, len(data) // 1024))


if __name__ == '__main__':
    main()
