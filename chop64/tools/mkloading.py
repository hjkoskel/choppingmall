#!/usr/bin/env python3
"""Writes the PLACEHOLDER loading picture loading/loading.koa (Koala Painter format, C64 multicolour bitmap).
Replace that file with your own picture and run `make`; see docs/LOADING_SCREEN.md.
Koala layout: 2-byte load address ($6000), bitmap 8000, screen RAM 1000, colour RAM 1000, background colour 1 = 10003 bytes."""
import sys
from PIL import Image, ImageDraw, ImageFont
out = sys.argv[1] if len(sys.argv) > 1 else "loading/loading.koa"
BG, A, B, C = 0, 7, 15, 11            # black, yellow, light grey, dark grey (C64 palette numbers)
W, H = 320, 200                        # drawn at double-wide pixels, folded to 160x200 below
im = Image.new("P", (W, H), 0)
d = ImageDraw.Draw(im)
d.rectangle([4, 4, W - 5, H - 5], outline=A, width=4)             # frame
d.rectangle([10, 10, W - 11, H - 11], outline=C, width=2)
def centred(y, text, col, scale):
    f = ImageFont.load_default(size=8)
    t = Image.new("L", (len(text) * 8, 10), 0); ImageDraw.Draw(t).text((0, 0), text, font=f, fill=255)
    t = t.point(lambda v: 255 if v > 100 else 0).resize((t.width * scale, t.height * scale), Image.NEAREST)
    bb = t.getbbox(); t = t.crop(bb)
    im.paste(col, (( W - t.width) // 2, y), t)
centred(50, "CHOPPING", A, 4)
centred(88, "MALL", A, 4)
centred(135, "LOADING...", B, 2)
centred(165, "PLACEHOLDER - SEE DOCS/LOADING_SCREEN.MD", C, 1)
d.rectangle([140, 118, 180, 128], fill=C)                          # little robot rectangle
px = [[im.getpixel((2 * x, y)) or im.getpixel((2 * x + 1, y)) for x in range(160)] for y in range(H)]
bmp = bytearray(8000); scr = bytearray(1000); col = bytearray(1000)
code = {0: 0b00, A: 0b01, B: 0b10, C: 0b11}
for cy in range(25):
    for cx in range(40):
        used = set(px[cy * 8 + r][cx * 4 + c] for r in range(8) for c in range(4))
        assert used <= {0, A, B, C}, used
        for r in range(8):
            v = 0
            for c in range(4): v = (v << 2) | code[px[cy * 8 + r][cx * 4 + c]]
            bmp[(cy * 40 + cx) * 8 + r] = v
        scr[cy * 40 + cx] = (A << 4) | B; col[cy * 40 + cx] = C
with open(out, "wb") as f:
    f.write(bytes([0x00, 0x60])); f.write(bmp); f.write(scr); f.write(col); f.write(bytes([0]))
print("wrote", out)
