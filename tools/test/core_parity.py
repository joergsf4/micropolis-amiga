#!/usr/bin/env python3
"""The simulation core of the Amiga build must behave like the same code built natively on the Mac (a wrong -O2 build once left out the traffic,
counted too few residents and froze the demand valves). Runs a few cities for 12 months in both and compares the results with tolerances
(the random numbers are seeded from the clock at load time, so the runs are not identical, only equal in the statistics).

usage: core_parity.py [city ...]      (default senri wetcity; needs Docker for the Amiga run, a C compiler for the native one)
"""
import os
import re
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
MONTHS = 12


def parse(text):
    r = {}
    m = re.findall(r'month +%d: year \d+ funds (-?\d+) pop (\d+) score (\d+)' % MONTHS, text)
    if not m:
        return None
    r['funds'], r['pop'], r['score'] = map(int, m[-1])
    v = re.findall(r'valves? (-?\d+)/(-?\d+)/(-?\d+)', text) or re.findall(r'valve (-?\d+)/(-?\d+)/(-?\d+)', text)
    r['valves'] = tuple(map(int, v[-1])) if v else None
    t = re.findall(r'roads (\d+) \(light (\d+) heavy (\d+)\)', text)
    r['traffic'] = int(t[-1][1]) + int(t[-1][2]) if t else None
    d = re.findall(r'TrfDensity max (\d+), cells>0 (\d+)', text)
    r['trf_cells'] = int(d[-1][1]) if d else None
    return r


def native(city):
    subprocess.run([os.path.join(ROOT, 'tools', 'native_test.sh'), city + '.cty', str(MONTHS), str(MONTHS)], cwd=ROOT, check=True,
                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out = subprocess.run(['../headless_native', os.path.join(ROOT, 'upstream', 'cities', city + '.cty'), str(MONTHS), str(MONTHS)],
                         cwd=os.path.join(ROOT, 'out', 'native', 'run'), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True).stdout
    return parse(out)


def amiga(city):
    cmd = 'Work:Micropolis/headless Work:Micropolis/cities/%s.cty %d' % (city, MONTHS)
    out = subprocess.run([os.path.join(ROOT, 'tools', 'emusys.sh'), cmd, '300'], cwd=ROOT, stdout=subprocess.PIPE, text=True).stdout
    return parse(out)


def main():
    cities = sys.argv[1:] or ['senri', 'wetcity']
    failed = 0
    for city in cities:
        n, a = native(city), amiga(city)
        problems = []
        if not n or not a:
            problems.append('no result (native %s, amiga %s)' % (bool(n), bool(a)))
        else:
            if abs(a['pop'] - n['pop']) > max(0.06 * n['pop'], 800):
                problems.append('population %d (amiga) vs %d (native)' % (a['pop'], n['pop']))
            if abs(a['score'] - n['score']) > 40:
                problems.append('score %d vs %d' % (a['score'], n['score']))
            if n['trf_cells'] and a['trf_cells'] is not None and a['trf_cells'] < 0.4 * n['trf_cells']:
                problems.append('traffic cells %d vs %d' % (a['trf_cells'], n['trf_cells']))
            if n['traffic'] and a['traffic'] is not None and a['traffic'] < 0.4 * n['traffic']:
                problems.append('traffic tiles %d vs %d' % (a['traffic'], n['traffic']))
            if n['valves'] and a['valves']:
                for i, name in enumerate(('residential', 'commercial', 'industrial')):
                    if abs(a['valves'][i] - n['valves'][i]) > 900:
                        problems.append('%s demand %d vs %d' % (name, a['valves'][i], n['valves'][i]))
        print('%-10s %s   (amiga %s | native %s)' % (city, 'ok' if not problems else 'FAIL: ' + '; '.join(problems), a, n))
        failed += bool(problems)
    print('%d of %d cities differ' % (failed, len(cities)) if failed else 'all %d cities agree' % len(cities))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
