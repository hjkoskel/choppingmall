#!/usr/bin/env python3
"""Explore: best universal colour U + four 3-colour BG palettes for the NES picture (pixel colour error)."""
import sys, random, itertools
from PIL import Image
import numpy as np
im = Image.open(sys.argv[1]).convert('RGB')
a = np.array(im)[:, 32:288, :]          # 200 x 256
H, W = a.shape[:2]
cols = sorted({tuple(p) for p in a.reshape(-1, 3)})
cid = {c: i for i, c in enumerate(cols)}
idx = np.zeros((H, W), int)
for y in range(H):
    for x in range(W): idx[y, x] = cid[tuple(a[y, x])]
C = np.array(cols, float)
# perceptual-ish weights
D = np.sqrt((((C[:, None, :] - C[None, :, :]) * np.array([.3, .59, .11])) ** 2).sum(-1)) 
blocks = []
for by in range(0, 200, 16):
    for bx in range(0, 256, 16):
        h = np.bincount(idx[by:by + 16, bx:bx + 16].ravel(), minlength=len(cols))
        blocks.append(h)
blocks = np.array(blocks)   # nb x ncol
def cost_pal(U, pal):       # cost of every block under the palette (U + 3 colours)
    avail = [U] + list(pal)
    best = D[:, avail].min(1)            # per colour: cheapest representation
    return blocks @ best
def total(U, pals):
    cs = np.array([cost_pal(U, p) for p in pals])
    return cs.min(0).sum(), cs.argmin(0)
n = len(cols); rnd = random.Random(1)
best = None
for U in range(n):
    others = [c for c in range(n) if c != U]
    cur = [tuple(rnd.sample(others, 3)) for _ in range(4)]
    ct, _ = total(U, cur)
    improved = True
    while improved:
        improved = False
        for pi in range(4):
            for trip in itertools.combinations(others, 3):
                t = cur[:]; t[pi] = trip
                c2, _ = total(U, t)
                if c2 < ct - 1e-9: cur, ct, improved = t, c2, True
    if best is None or ct < best[0]: best = (ct, U, cur)
    print(cols[U], round(ct), file=sys.stderr)
ct, U, pals = best
print("best U", cols[U], "cost", round(ct), "pixels", H * W)
for p in pals: print([cols[c] for c in p])
