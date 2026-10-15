#!/usr/bin/env python3
"""settr.py LANGUAGE 'English text' 'translation' [...]   sets translations in l10n/<language>.ct (texts must exist, run l10n.py extract first)"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import l10n
lang = sys.argv[1]
path = os.path.join(l10n.L10N, {'deutsch': 'deutsch.ct', 'polski': 'polski.ct'}.get(lang, lang + '.ct'))
_, tr = l10n.read_ct(path)
strings = l10n.all_strings()
args = sys.argv[2:]
for en, loc in zip(args[::2], args[1::2]):
    en, loc = l10n.unesc(en), l10n.unesc(loc)
    if l10n.name(en) not in {l10n.name(s) for s in strings}:
        sys.exit('unknown text: %r' % en)
    tr[l10n.name(en)] = loc
l10n.write_ct(path, lang, strings, tr)
