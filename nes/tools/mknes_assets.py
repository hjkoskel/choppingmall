#!/usr/bin/env python3
"""mknes_assets.py - builds the NES CHR-ROM (8 KB: BG table + sprite table) and the data blobs the program includes.
   input : gfx/bg_*.bin  (from tools/mknes_bg.py, checked in because that step takes ~2 minutes)
   output: build/chr.bin, build/nt.bin (full nametable incl. HUD rows blank), build/attr.bin
   BG tile numbers: 0 space, 1-10 digits, 11-36 A-Z, 37 ':', 38 '.', 39 '-', 40 '!', 41 '/', 42 '+', 43 '?', 44 '>', 45 heart, 46 '$',
                    48-56 health bar 0..8 pixels, 57.. the mall picture.
   Sprite tiles ($1000 table): 0 blank, 1 bullet, 2 bolt, 3 molotov, 4-11 fire (2 frames x 4), 12-47 player (9 frames x 2x2), 48-128 robot (9 frames x 3x3)."""
import os, math
import numpy as np
here = os.path.dirname(os.path.abspath(__file__)); root = os.path.join(here, '..')
G = lambda n: open(os.path.join(root, 'gfx', n), 'rb').read()
os.makedirs(os.path.join(root, 'build'), exist_ok=True)

def enc(px):  # 8x8 array of 0..3 -> 16 bytes
    lo = bytes(sum(((int(px[r][c]) & 1) << (7 - c)) for c in range(8)) for r in range(8))
    hi = bytes(sum((((int(px[r][c]) >> 1) & 1) << (7 - c)) for c in range(8)) for r in range(8))
    return lo + hi

FONT = {
'0': ".###. #...# #..## #.#.# ##..# #...# .###.", '1': "..#.. .##.. ..#.. ..#.. ..#.. ..#.. .###.",
'2': ".###. #...# ....# ...#. ..#.. .#... #####", '3': ".###. #...# ....# ..##. ....# #...# .###.",
'4': "...#. ..##. .#.#. #..#. ##### ...#. ...#.", '5': "##### #.... ####. ....# ....# #...# .###.",
'6': "..##. .#... #.... ####. #...# #...# .###.", '7': "##### ....# ...#. ..#.. .#... .#... .#...",
'8': ".###. #...# #...# .###. #...# #...# .###.", '9': ".###. #...# #...# .#### ....# ...#. .##..",
'A': ".###. #...# #...# ##### #...# #...# #...#", 'B': "####. #...# #...# ####. #...# #...# ####.",
'C': ".###. #...# #.... #.... #.... #...# .###.", 'D': "###.. #..#. #...# #...# #...# #..#. ###..",
'E': "##### #.... #.... ####. #.... #.... #####", 'F': "##### #.... #.... ####. #.... #.... #....",
'G': ".###. #...# #.... #.### #...# #...# .####", 'H': "#...# #...# #...# ##### #...# #...# #...#",
'I': ".###. ..#.. ..#.. ..#.. ..#.. ..#.. .###.", 'J': "..### ...#. ...#. ...#. ...#. #..#. .##..",
'K': "#...# #..#. #.#.. ##... #.#.. #..#. #...#", 'L': "#.... #.... #.... #.... #.... #.... #####",
'M': "#...# ##.## #.#.# #.#.# #...# #...# #...#", 'N': "#...# ##..# #.#.# #..## #...# #...# #...#",
'O': ".###. #...# #...# #...# #...# #...# .###.", 'P': "####. #...# #...# ####. #.... #.... #....",
'Q': ".###. #...# #...# #...# #.#.# #..#. .##.#", 'R': "####. #...# #...# ####. #.#.. #..#. #...#",
'S': ".#### #.... #.... .###. ....# ....# ####.", 'T': "##### ..#.. ..#.. ..#.. ..#.. ..#.. ..#..",
'U': "#...# #...# #...# #...# #...# #...# .###.", 'V': "#...# #...# #...# #...# #...# .#.#. ..#..",
'W': "#...# #...# #...# #.#.# #.#.# ##.## #...#", 'X': "#...# #...# .#.#. ..#.. .#.#. #...# #...#",
'Y': "#...# #...# .#.#. ..#.. ..#.. ..#.. ..#..", 'Z': "##### ....# ...#. ..#.. .#... #.... #####",
':': "..... ..#.. ..#.. ..... ..#.. ..#.. .....", '.': "..... ..... ..... ..... ..... .##.. .##..",
'-': "..... ..... ..... ##### ..... ..... .....", '!': "..#.. ..#.. ..#.. ..#.. ..#.. ..... ..#..",
'/': "....# ...#. ...#. ..#.. .#... .#... #....", '+': "..... ..#.. ..#.. ##### ..#.. ..#.. .....",
'?': ".###. #...# ....# ...#. ..#.. ..... ..#..", '>': "#.... .#... ..#.. ...#. ..#.. .#... #....",
'#': ".#.#. ##### ##### ##### .###. ..#.. .....", '$': "..#.. .#### #.#.. .###. ..#.# ####. ..#..",
}
ORDER = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:.-!/+?>#$"
bg = bytearray()
for ch in ORDER:
    px = np.ones((8, 8), int)                    # background = index 1
    if ch != ' ':
        for r, row in enumerate(FONT[ch].split()):
            for c, v in enumerate(row):
                if v == '#': px[r, c + 1] = 3     # foreground = index 3
    bg += enc(px)
while len(bg) < 48 * 16: bg += enc(np.ones((8, 8), int))
for n in range(9):                               # health bar: n pixels filled
    px = np.ones((8, 8), int)
    px[7, :] = 2
    for r in range(1, 7): px[r, :n] = 3
    bg += enc(px)
BASE = len(bg) // 16
bg += G('bg_tiles.bin')
assert len(bg) <= 4096, len(bg)
bg += bytes(4096 - len(bg))

# ---- sprites
spr = np.zeros((256, 8, 8), int)
def put(canvas, x, y, v):
    if 0 <= x < canvas.shape[1] and 0 <= y < canvas.shape[0]: canvas[y, x] = v
def blit(first, canvas, tiles_w):  # canvas (h, w) -> consecutive tiles, row-major
    h, w = canvas.shape
    for ty in range(h // 8):
        for tx in range(w // 8): spr[first + ty * tiles_w + tx] = canvas[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8]
c = np.zeros((8, 8), int)
for y in range(8):
    for x in range(8):
        if (x - 3.5) ** 2 + (y - 3.5) ** 2 <= 2.2: c[y, x] = 3
spr[1] = c
c = np.zeros((8, 8), int)
for y in range(8):
    for x in range(8):
        if (x - 3.5) ** 2 + (y - 3.5) ** 2 <= 9.5: c[y, x] = 3
        if (x - 3.5) ** 2 + (y - 3.5) ** 2 <= 3.0: c[y, x] = 2
spr[2] = c
c = np.zeros((8, 8), int)
for y in range(1, 8):
    for x in range(2, 7):
        if y > 3 or (3 <= x <= 4): c[y, x] = 3
        if y > 3 and x in (3, 4) and y < 7: c[y, x] = 2
spr[3] = c
for f in range(2):                                # fire: ragged disc, 16x16
    cv = np.zeros((16, 16), int)
    for y in range(16):
        for x in range(16):
            d2 = (x - 7.5) ** 2 + (y - 7.5) ** 2
            if d2 <= 52 and ((x * 5 + y * 3 + f * 7) % 5) != 0: cv[y, x] = 1 if d2 > 25 else 2
            if d2 <= 9 and f == 0: cv[y, x] = 3
    blit(4 + f * 4, cv, 2)
def player(d):
    ang = d * math.pi / 16; cv = np.zeros((16, 16), int)
    for y in range(16):
        for x in range(16):
            r2 = (x + .5 - 8) ** 2 + (y + .5 - 8) ** 2
            if r2 <= 6.5 ** 2: cv[y, x] = 1
            if r2 <= 3.2 ** 2: cv[y, x] = 2
    for t in np.arange(4, 11.5, .5):
        x = int(math.floor(8 + math.cos(ang) * t)); y = int(math.floor(8 + math.sin(ang) * t))
        put(cv, x, y, 3); put(cv, x + (1 if abs(math.sin(ang)) > .5 else 0), y + (1 if abs(math.cos(ang)) > .5 else 0), 3)
    return cv
def robot(d):
    ang = d * math.pi / 16; c_, s_ = math.cos(ang), math.sin(ang); cv = np.zeros((24, 24), int)
    for y in range(24):
        for x in range(24):
            dx, dy = x + .5 - 12, y + .5 - 12
            u = dx * c_ + dy * s_; v = -dx * s_ + dy * c_
            if -8 <= u <= 8 and -4 <= v <= 4: cv[y, x] = 1
            if -1.5 <= u <= 1.5 and -10 <= v <= 10: cv[y, x] = 1
    ex, ey = int(math.floor(12 + c_ * 6)), int(math.floor(12 + s_ * 6))
    for yy in range(ey - 1, ey + 3):                 # black outline around the eye (it would vanish on the red corridor)
        for xx in range(ex - 1, ex + 3):
            if 0 <= xx < 24 and 0 <= yy < 24 and cv[yy, xx] == 1: cv[yy, xx] = 2
    for k in range(2): put(cv, ex, ey + k, 3); put(cv, ex + 1, ey + k, 3)
    return cv
for d in range(9):
    blit(12 + d * 4, player(d), 2)
    blit(48 + d * 9, robot(d), 3)
c = np.zeros((8, 8), int)   # door marker: a small $ in index 3
for r, row in enumerate(FONT['$'].split()):
    for cc, v in enumerate(row):
        if v == '#': c[r, cc + 1] = 2
spr[129] = c
chr_ = bytes(bg) + b''.join(enc(spr[i]) for i in range(256))
open(os.path.join(root, 'build', 'chr.bin'), 'wb').write(chr_)

# ---- nametable / attributes: picture tile numbers move up by BASE
nt = bytearray(G('bg_nt.bin'))
for i in range(960):
    r = i // 32
    nt[i] = (nt[i] + BASE) & 0xFF if 2 <= r <= 26 else 0
# ---- shop labels and the title on the dark shop block (same placement as chop64/src/picture.c, shifted -4 columns / +2 rows): text on black "plates"
attr = bytearray(G('bg_attr.bin'))
SNX = [111, 0, -111, -111, 0, 111]; SNY = [64, 127, 64, -64, -127, -64]
NAMES = ["SHELL", "GAS", "AID", "AMMO", "SHELL", "AMMO"]
def text(col, row, t):
    for i, ch in enumerate(t):
        nt[row * 32 + col + i] = ORDER.index(ch)
        bx, by = (col + i) // 2, row // 2
        a_i = (by // 2) * 8 + bx // 2; sh = ((by & 1) * 2 + (bx & 1)) * 2
        attr[a_i] &= ~(3 << sh)                        # palette 0: white text on black
for k in range(6):
    ln = len(NAMES[k]); px = 160 + ((SNX[k] * 36) >> 7); py = 100 + ((SNY[k] * 36) >> 7)
    text((px - ln * 4) // 8 - 4, (py - 4) // 8 + 2, NAMES[k])
text(16 - 4, 11 + 2, "CHOPPING"); text(18 - 4, 12 + 2, "MALL")
open(os.path.join(root, 'build', 'nt.bin'), 'wb').write(bytes(nt[2 * 32:27 * 32]))   # rows 2..26 only (800 bytes)
open(os.path.join(root, 'build', 'attr.bin'), 'wb').write(bytes(attr))
print("BG base tile", BASE, "picture tiles", (len(G('bg_tiles.bin')) // 16), "font+bar", BASE)
