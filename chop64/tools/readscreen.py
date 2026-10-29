#!/usr/bin/env python3
"""Reads digits that the debug builds draw as glyph cells (3x5 multicolour font) off a VICE screenshot.
usage: readscreen.py screenshot.png row col ncells     (the font is parsed from src/assets_gen.h)"""
import os, re, sys
from PIL import Image
here = os.path.dirname(os.path.abspath(__file__))
hdr = open(os.path.join(here, '..', 'src', 'assets_gen.h')).read()
font = [int(v) for v in re.search(r'ASSET_MCFONT\[\d+\] = \{(.*?)\};', hdr, re.S).group(1).replace('\n', ' ').replace(',', ' ').split()]
GLYPH = {d: font[d * 8:d * 8 + 8] for d in range(10)}
def read_digits(png, row, col0, n):
    im = Image.open(png).convert('RGB')
    out = ''
    for c in range(col0, col0 + n):
        x0, y0 = 32 + c * 8, 35 + row * 8     # the 320x200 screen starts at (32, 35) in VICE's 384x272 screenshot
        cell = []
        for r in range(8):
            b = 0
            for px in range(4):
                b = (b << 2) | (3 if im.getpixel((x0 + 2 * px, y0 + r)) != (0, 0, 0) else 0)
            cell.append(b)
        if all(v == 0 for v in cell): out += ' '; continue
        m = [d for d, g in GLYPH.items() if g == cell]
        out += str(m[0]) if m else '?'
    return out.strip()
if __name__ == '__main__':
    print(read_digits(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])))
