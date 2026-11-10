#!/usr/bin/env python3
"""Bucket samples.txt (frames.py --prof) by over-budget frame: python overbuckets.py <samples.txt> <elf> [ms]"""
import sys
from collections import Counter
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from elfsym import symbols
import bisect
sy = symbols(Path(sys.argv[2])); thr = float(sys.argv[3]) * 1000 if len(sys.argv) > 3 else 33300
fs = sorted((v[0], k) for k, v in sy.items())
addrs = [a for a, _ in fs]
def name(pc):
    i = bisect.bisect_right(addrs, pc) - 1
    return fs[i][1] if i >= 0 else "?"
cur = []; tot = Counter(); n = 0
for ln in open(sys.argv[1]):
    ln = ln.split()
    if not ln: continue
    if ln[0] == "F":
        if int(ln[2]) > thr:
            n += 1
            for pc in cur: tot[name(pc)] += 1
        cur = []
    elif ln[0] != "end":
        cur.append(int(ln[0], 16))
print(n, "over frames; samples (ms) in them:")
for k, v in tot.most_common(30): print(f"  {v:5d} {'IW ' if sy[k][0] >> 24 == 3 else 'ROM'} {sy[k][1]:5d} B {k}")
