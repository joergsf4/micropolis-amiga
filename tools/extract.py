#!/usr/bin/env python3
"""Extract named top-level functions (and verbatim header text) from an upstream C file.
usage: extract.py UPSTREAM.c OUT.c "header text" func1 func2 ...
Used to pull the simulation parts out of upstream files that also hold Tk widget code."""
import re, sys
src, out, header, *names = sys.argv[1:]
L = open(src, encoding='latin-1').read().split('\n')
res = [header, '']
for n in names:
    pat = re.compile(r'^(?:[A-Za-z_][\w \t\*]*?[ \t\*])?%s\s*\(' % re.escape(n))
    for i, l in enumerate(L):
        if pat.match(l) and not l.rstrip().endswith(';'):
            s = i
            if s > 0 and re.match(r'^(static |extern )?(void|int|short|char|float|double|long|QUAD|unsigned.*|SimSprite \*|\w+ \*)$', L[s-1].strip()):
                s -= 1
            e = next(j for j in range(i, len(L)) if L[j] == '}')
            res += L[s:e+1] + ['', '']
            break
    else:
        sys.exit('function not found: ' + n)
open(out, 'w', encoding='latin-1').write('\n'.join(res))
