#!/usr/bin/env python3
"""mknes_bg.py - converts the C64 mall picture (no-text render) into NES background data:
   per 16x16 block one of four 3-colour palettes (universal colour 0 = yellow), 8x8 tiles deduplicated and, above the
   tile budget, merged lossily (cheapest pixel error first).  usage: mknes_bg.py pic.png budget out_prefix
   writes <out>_tiles.bin (16 B per tile), <out>_nt.bin (32x30 nametable, tile numbers relative to tile 0 of the picture set),
   <out>_attr.bin (64 B), <out>_pal.bin (colour ids) and <out>_preview.png"""
import sys
import numpy as np
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB')
budget = int(sys.argv[2]); out = sys.argv[3]
a = np.array(im)[:, 32:288, :]   # 200 x 256 (game x - 32)
H, W = a.shape[:2]
RGB = {'blk': (0, 0, 0), 'wht': (255, 255, 255), 'ltgr': (187, 187, 187), 'gr': (119, 119, 119), 'dk': (51, 51, 51), 'red': (136, 0, 0),
       'pink': (255, 119, 119), 'yel': (238, 238, 119), 'grn': (0, 204, 85), 'lgrn': (170, 255, 102), 'brn': (102, 68, 0), 'blu': (0, 136, 255),
       'cyn': (170, 255, 238), 'pur': (204, 68, 204)}
U = 'yel'
PALS = [('blk', 'ltgr', 'wht'), ('red', 'ltgr', 'pink'), ('dk', 'gr', 'wht'), ('grn', 'red', 'wht')]
names = list(RGB); C = np.array([RGB[n] for n in names], float)
wts = np.array([.3, .59, .11])
def dist(c1, c2): return np.sqrt((((c1 - c2) * wts) ** 2).sum(-1))
cidx = {tuple(v): i for i, v in enumerate(map(tuple, C.astype(int)))}
src = np.zeros((H, W), int)
for y in range(H):
    for x in range(W): src[y, x] = cidx[tuple(a[y, x])]
D = np.array([[dist(C[i], C[j]) for j in range(len(C))] for i in range(len(C))])
pal_ids = [[names.index(U)] + [names.index(n) for n in p] for p in PALS]   # slot 0..3 -> colour id
nbx, nby = W // 16, (H + 15) // 16
# 1. palette per block
blk_pal = np.zeros((nby, nbx), int)
for by in range(nby):
    for bx in range(nbx):
        sub = src[by * 16:by * 16 + 16, bx * 16:bx * 16 + 16]
        h = np.bincount(sub.ravel(), minlength=len(C))
        costs = [(h * D[:, ids].min(1)).sum() for ids in pal_ids]
        blk_pal[by, bx] = int(np.argmin(costs))
# 2. index image (0..3 per pixel) and per tile patterns; picture is 25 tile rows
idx = np.zeros((H, W), np.uint8)
for y in range(H):
    for x in range(W):
        ids = pal_ids[blk_pal[y // 16, x // 16]]
        idx[y, x] = int(np.argmin([D[src[y, x], i] for i in ids]))
TR, TC = (H + 7) // 8, W // 8
tiles = []   # unique patterns
tmap = np.zeros((TR, TC), int)
lookup = {}
for ty in range(TR):
    for tx in range(TC):
        t = idx[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8]
        k = t.tobytes()
        if k not in lookup: lookup[k] = len(tiles); tiles.append(t.copy())
        tmap[ty, tx] = lookup[k]
print("unique tiles before merge:", len(tiles), file=sys.stderr)
# 3. lossy merge down to the budget. cost of showing tile b where tile a was (in palette pi): sum of colour distances
def place_cost(a, b, pi):
    ids = pal_ids[pi]; col = C[ids]
    return sum(dist(col[a.ravel()[k]], col[b.ravel()[k]]) for k in range(64))
uses = {}
for ty in range(TR):
    for tx in range(TC):
        uses.setdefault(int(tmap[ty, tx]), []).append(int(blk_pal[ty // 2, tx // 2]))
alive = set(range(len(tiles)))
# precompute pairwise cost lazily with numpy: distance table per palette on index values
pal_d = [np.array([[dist(C[ids[i]], C[ids[j]]) for j in range(4)] for i in range(4)]) for ids in pal_ids]
def cost_ab(a, b):
    ta, tb = tiles[a], tiles[b]
    return sum(pal_d[pi][ta, tb].sum() for pi in uses[a])
repl = {}
while len(alive) > budget:
    best = None
    al = sorted(alive)
    for a in al:
        if a in repl: continue
        for b in al:
            if a == b: continue
            c = cost_ab(a, b)
            if best is None or c < best[0]: best = (c, a, b)
    c, a, b = best
    # a is replaced by b; everything that mapped to a now maps to b
    alive.discard(a); repl[a] = b
    uses[b] = uses[b] + uses[a]
    print(len(alive), round(c, 1), file=sys.stderr, end='\r')
def final(t):
    while t in repl: t = repl[t]
    return t
order = sorted(alive); newid = {t: i for i, t in enumerate(order)}
nt = np.zeros((30, 32), np.uint8)
for ty in range(TR):
    for tx in range(TC):
        nt[ty + 2, tx] = newid[final(int(tmap[ty, tx]))]    # picture occupies tile rows 2..26
# CHR bytes (2 bitplanes)
chr_ = bytearray()
for t in order:
    tt = tiles[t]
    lo = bytes(sum(((int(tt[r, c]) & 1) << (7 - c)) for c in range(8)) for r in range(8))
    hi = bytes(sum((((int(tt[r, c]) >> 1) & 1) << (7 - c)) for c in range(8)) for r in range(8))
    chr_ += lo + hi
attr = bytearray(64)
for by in range(15):
    for bx in range(16):
        # attribute blocks are 32x32 px: four 16x16 quadrants, picture starts at y=16 px -> block rows align at 2 tile rows
        py = by * 16 - 16     # picture y of this 16x16 block row (screen row by)
        pi = int(blk_pal[py // 16, bx]) if 0 <= py < H else 0
        a_i = (by // 2) * 8 + bx // 2
        attr[a_i] |= pi << (((by & 1) * 2 + (bx & 1)) * 2)
open(out + '_tiles.bin', 'wb').write(chr_)
open(out + '_nt.bin', 'wb').write(bytes(nt.ravel()))
open(out + '_attr.bin', 'wb').write(bytes(attr))
open(out + '_pal.bin', 'wb').write(bytes(i for p in pal_ids for i in p))
# preview: decode what the NES will show
pv = np.zeros((TR * 8, W, 3), np.uint8)
for ty in range(TR):
    for tx in range(TC):
        tt = tiles[order[nt[ty + 2, tx]]]; ids = pal_ids[blk_pal[ty // 2, tx // 2]]
        for r in range(8):
            for c in range(8): pv[ty * 8 + r, tx * 8 + c] = C[ids[tt[r, c]]]
Image.fromarray(pv).resize((W * 3, TR * 8 * 3), Image.NEAREST).save(out + '_preview.png')
print("tiles used:", len(order), "of budget", budget, file=sys.stderr)
