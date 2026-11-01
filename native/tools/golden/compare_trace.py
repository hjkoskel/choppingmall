#!/usr/bin/env python3
"""Compare a JS golden trace with a C replay: floats within tol, everything else exact."""
import sys, re
def parse(path):
    rows = []
    for line in open(path):
        if line.startswith('#') or not line.strip(): continue
        rows.append(dict(kv.split('=', 1) for kv in line.split()))
    return rows
a, b = parse(sys.argv[1]), parse(sys.argv[2]); tol = float(sys.argv[3]) if len(sys.argv) > 3 else 1e-6
if len(a) != len(b): print(f"FAIL: {len(a)} JS rows vs {len(b)} C rows"); sys.exit(1)
worst = 0.0; bad = 0
for ra, rb in zip(a, b):
    for k, va in ra.items():
        vb = rb.get(k)
        if vb is None: print("missing key", k); sys.exit(1)
        try:
            fa, fb = float(va), float(vb)
        except ValueError:
            if va != vb:
                bad += 1
                if bad <= 8: print(f"MISMATCH f={ra['f']} {k}: js={va} c={vb}")
            continue
        d = abs(fa - fb); worst = max(worst, d / max(1.0, abs(fa)))
        discrete = k in ('f','lives','level','kills','w','a0','a1','a2','rdead','rdir','rside','nbul','nrb','nmol','nfire','nparts')
        if (discrete and fa != fb) or d > tol * max(1.0, abs(fa)):
            bad += 1
            if bad <= 8: print(f"MISMATCH f={ra['f']} {k}: js={va} c={vb}")
print(("PASS" if bad == 0 else "FAIL") + f": {len(a)} snapshots, worst relative float diff {worst:.3g}, {bad} mismatches")
sys.exit(1 if bad else 0)
