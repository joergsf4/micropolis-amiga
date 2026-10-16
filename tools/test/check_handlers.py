#!/usr/bin/env python3
"""Every "UI..." notification the simulation core sends must be handled by the front end (src/amiga) or be listed with a
reason in ui_ignored.txt. This catches handlers that get lost (the budget numbers were missing from v0.4.0 to v0.8.0 because
the UISetBudget handler had been deleted by accident).  exit status 1 on failure."""
import glob, os, re, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))


def names(pattern, regex=r'"(UI[A-Za-z]+)'):
    found = set()
    for f in glob.glob(os.path.join(ROOT, pattern)):
        for m in re.finditer(regex, open(f, encoding='latin-1').read()):
            found.add(m.group(1))
    return found


def main():
    core = names('src/core/*.c')
    handled = names('src/amiga/*.c', r'strn?cmp\(cmd, "(UI[A-Za-z]+)')       # only real handlers: strncmp(cmd, "UIName ...") in ui_eval
    ignored = {}
    for line in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ui_ignored.txt'), encoding='utf-8'):
        if line.strip() and not line.startswith('#'):
            n, _, why = line.strip().partition(' ')
            ignored[n] = why.strip()
    missing = sorted(core - handled - set(ignored))
    stale = sorted(n for n in ignored if n in handled or n not in core)
    print('core notifications: %d, handled: %d, deliberately ignored: %d' % (len(core), len(core & handled), len(ignored)))
    ok = True
    if missing:
        ok = False
        print('NOT HANDLED (add a handler in ui_eval or a line in ui_ignored.txt):', ', '.join(missing))
    if stale:
        print('note: listed in ui_ignored.txt but handled or unknown (remove the line):', ', '.join(stale))
    print('PASS' if ok else 'FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
