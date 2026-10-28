"""The biggest linked sections of the last release build (from its .map): python tools/mapsize.py [n]"""
import os
import re
import sys
m = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'CHStarlift', 'build', 'release', 'CHStarlift.ino.map')).read()
m = m[m.index('Linker script and memory map'):]
agg = {}
for name, addr, size, f in re.findall(r'\n \.(?:text|rodata|srodata|sdata|data)\.?(\S*)\s*\n?\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+)', m):
    if int(addr, 16):
        agg[name] = agg.get(name, 0) + int(size, 16)
for n, sz in sorted(agg.items(), key=lambda x: -x[1])[:int(sys.argv[1]) if len(sys.argv) > 1 else 30]:
    print(sz, n[:70])
