/* picture.c - paints the static mall picture and converts it to a C64 multicolour bitmap. HOST ONLY (build time).
 *
 * The picture is painted per screen pixel (320x200) from the same hexagon geometry as the collision code (core.c's
 * mdot), then converted: each pair of pixels becomes one multicolour pixel and each 4x8 multicolour cell keeps the
 * background colour (black, $D021) plus its three most frequent other colours (screen-RAM hi/lo nibble and colour
 * RAM); other pixels snap to the nearest of those. The painting is designed so that most cells need <= 3 colours
 * (tiles are 16x16 and cell aligned, trims are drawn between regions, not on top of them). */
#include <string.h>
#include "assets.h"
#include "core.h"
#include "mcfont.h"

static u8 img[200][320];
static const u8 PAL[16][3] = {{0, 0, 0}, {255, 255, 255}, {136, 0, 0}, {170, 255, 238}, {204, 68, 204}, {0, 204, 85},
                              {0, 0, 170}, {238, 238, 119}, {221, 136, 85}, {102, 68, 0}, {255, 119, 119}, {51, 51, 51},
                              {119, 119, 119}, {170, 255, 102}, {0, 136, 255}, {187, 187, 187}};
static const i8 SNX[6] = {111, 0, -111, -111, 0, 111}, SNY[6] = {64, 127, 64, -64, -127, -64};
static const u8 SHOPCOL[4] = {7, 10, 13, 14}; /* ammo yellow, shells light red, gas light green, first aid light blue */
static const char *const SHOPNAME[6] = {"SHELL", "GAS", "AID", "AMMO", "SHELL", "AMMO"};

/* 3x5 glyphs, rows top to bottom, 'X' = pixel */
static const char *const FONT[GL_COUNT][5] = {
    {"XXX", "X.X", "X.X", "X.X", "XXX"}, {".X.", "XX.", ".X.", ".X.", "XXX"}, {"XX.", "..X", ".X.", "X..", "XXX"},
    {"XX.", "..X", ".X.", "..X", "XX."}, {"X.X", "X.X", "XXX", "..X", "..X"}, {"XXX", "X..", "XX.", "..X", "XX."},
    {".XX", "X..", "XXX", "X.X", "XXX"}, {"XXX", "..X", ".X.", ".X.", ".X."}, {"XXX", "X.X", "XXX", "X.X", "XXX"},
    {"XXX", "X.X", "XXX", "..X", "XX."},
    {".X.", "X.X", "XXX", "X.X", "X.X"}, {"XX.", "X.X", "XX.", "X.X", "XX."}, {".XX", "X..", "X..", "X..", ".XX"},
    {"XX.", "X.X", "X.X", "X.X", "XX."}, {"XXX", "X..", "XX.", "X..", "XXX"}, {"XXX", "X..", "XX.", "X..", "X.."},
    {".XX", "X..", "X.X", "X.X", ".XX"}, {"X.X", "X.X", "XXX", "X.X", "X.X"}, {"XXX", ".X.", ".X.", ".X.", "XXX"},
    {"..X", "..X", "..X", "X.X", ".X."}, {"X.X", "X.X", "XX.", "X.X", "X.X"}, {"X..", "X..", "X..", "X..", "XXX"},
    {"X.X", "XXX", "XXX", "X.X", "X.X"}, {"XX.", "X.X", "X.X", "X.X", "X.X"}, {".X.", "X.X", "X.X", "X.X", ".X."},
    {"XX.", "X.X", "XX.", "X..", "X.."}, {".X.", "X.X", "X.X", "XX.", ".XX"}, {"XX.", "X.X", "XX.", "X.X", "X.X"},
    {".XX", "X..", ".X.", "..X", "XX."}, {"XXX", ".X.", ".X.", ".X.", ".X."}, {"X.X", "X.X", "X.X", "X.X", "XXX"},
    {"X.X", "X.X", "X.X", "X.X", ".X."}, {"X.X", "X.X", "XXX", "XXX", "X.X"}, {"X.X", "X.X", ".X.", "X.X", "X.X"},
    {"X.X", "X.X", ".X.", ".X.", ".X."}, {"XXX", "..X", ".X.", "X..", "XXX"},
    {"...", "...", "...", "...", "..."}, {"...", ".X.", "...", ".X.", "..."}, {"..X", "..X", ".X.", "X..", "X.."},
    {"...", ".X.", "XXX", ".X.", "..."}, {".XX", "XX.", ".X.", ".XX", "XX."}, {".X.", ".X.", ".X.", "...", ".X."},
    {"...", "...", "XXX", "...", "..."}, {"...", "...", "...", "...", ".X."}, {"X..", ".X.", "..X", ".X.", "X.."},
    {"X.X", "XXX", "XXX", ".X.", "..."}, {"XXX", "XXX", "XXX", "XXX", "XXX"}, {"XX.", "..X", ".X.", "...", ".X."}};

/* one glyph as an 8-byte multicolour cell: every set pixel is multicolour code 11 (colour RAM colour) */
void picture_glyph_bytes(u8 gi, u8 *out) {
  u8 r, c;
  memset(out, 0, 8);
  if (gi == GL_SOLID) { memset(out, 0xFF, 8); return; }
  for (r = 0; r < 5; r++) {
    u8 b = 0;
    for (c = 0; c < 3; c++) b = (u8)(b | ((FONT[gi][r][c] == 'X' ? 3 : 0) << (6 - 2 * c)));
    out[r + 1] = b; /* glyph rows 1..5 of the cell, one blank pixel column on the right */
  }
}

static void disc(i16 cx, i16 cy, i16 r, u8 col) {
  i16 x, y;
  for (y = cy - r; y <= cy + r; y++)
    for (x = cx - r; x <= cx + r; x++)
      if (x >= 0 && x < 320 && y >= 0 && y < 200 && (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) img[y][x] = col;
}

static void paint(void) {
  i16 x, y, k, i;
  memset(img, 0, sizeof img);
  for (y = 0; y < 200; y++)
    for (x = 0; x < 320; x++) {
      i16 dx = x - CX, dy = y - CY;
      u16 m = mdot(dx, dy);
      u8 c = 0;
      if (m <= AO + 3) {
        if (m > AO) c = 7; /* outer gold trim */
        else if (m >= AI) { /* floor: 16x16 tiles, carpet in the middle with gold edging */
          i16 d = (i16)m - 72;
          c = (((x >> 4) + (y >> 4)) & 1) ? 15 : 1;
          if (d < 0) d = -d;
          if (d <= 12) c = d <= 1 ? 10 : 2;
          else if (d == 13) c = 7;
        } else { /* the shop block */
          i16 best = -999, bk = 0, t;
          c = 11;
          if (m >= AI - 3) c = 7; /* inner gold trim */
          else if (m == AI - 14) c = 12;
          for (k = 0; k < 6; k++) { i16 v = (i16)((dx * SNX[k] + dy * SNY[k]) >> 7); if (v > best) { best = v; bk = k; } }
          t = (i16)((-dy * SNX[bk] + dx * SNY[bk]) >> 7);
          if (t < 0) t = -t;
          if (best >= AI - 10 && best < AI - 3 && t <= 24) { /* striped shop awning */
            i16 tt = (i16)((-dy * SNX[bk] + dx * SNY[bk]) >> 7) + 64;
            c = ((tt / 6) & 1) ? 1 : SHOPCOL[shop_kind[bk]];
          }
        }
      }
      img[y][x] = c;
    }
  /* fountain */
  disc(160, 118, 9, 12); disc(160, 118, 7, 14); disc(160, 118, 3, 3);
  /* obstacles: benches (even) and plants (odd) */
  for (i = 0; i < NOB; i++) {
    i16 ox = obx[i], oy = oby[i];
    if (i & 1) { disc(ox, oy, 6, 5); disc(ox - 2, oy - 2, 2, 13); disc(ox + 2, oy + 2, 2, 13); disc(ox, oy, 2, 9); }
    else for (y = oy - 4; y <= oy + 4; y++) for (x = ox - 6; x <= ox + 5; x++) img[y][x] = ((y - oy) & 1) ? 9 : 7;
  }
}

static u8 nearest(u8 c, const u8 *set, u8 n) { /* nearest palette entry (RGB distance) among set[0..n-1] */
  long best = 1L << 30;
  u8 i, r = set[0];
  for (i = 0; i < n; i++) {
    long dr = PAL[c][0] - PAL[set[i]][0], dg = PAL[c][1] - PAL[set[i]][1], db = PAL[c][2] - PAL[set[i]][2], d = dr * dr + dg * dg + db * db;
    if (d < best) { best = d; r = set[i]; }
  }
  return r;
}

static u8 prio(u8 c) { return c == 0 ? 0 : (c == 1 || c == 15) ? 1 : c == 7 ? 5 : (c == 2 || c == 10 || c == 11) ? 3 : 4; }

static void put_cell(Assets *A, u8 cx, u8 cy, u8 gi, u8 color) {
  u16 i = (u16)cy * 40 + cx;
  picture_glyph_bytes(gi, A->bmp + i * 8);
  A->scr[i] = 0;
  A->col[i] = color;
}
static void put_text(Assets *A, u8 cx, u8 cy, const char *s, u8 color) {
  while (*s) put_cell(A, cx++, cy, glyph_index(*s++), color);
}

void picture_build(Assets *A) {
  static u8 mc[200][160];
  int cx, cy, r, x, k;
  paint();
  for (r = 0; r < 200; r++)
    for (x = 0; x < 160; x++) {
      u8 a = img[r][2 * x], b = img[r][2 * x + 1];
      mc[r][x] = prio(b) > prio(a) ? b : a; /* keep thin trims */
    }
  for (cy = 0; cy < 25; cy++)
    for (cx = 0; cx < 40; cx++) {
      int count[16], row, px;
      u8 chosen[3] = {0, 0, 0}, n = 0, set[4], code[16], cnt;
      memset(count, 0, sizeof count);
      for (row = 0; row < 8; row++) for (px = 0; px < 4; px++) count[mc[cy * 8 + row][cx * 4 + px]]++;
      for (cnt = 0; cnt < 3; cnt++) { /* the three most frequent non-black colours */
        int best = 0, bc = -1, c;
        for (c = 1; c < 16; c++) if (count[c] > best) { best = count[c]; bc = c; }
        if (bc < 0) break;
        chosen[n++] = (u8)bc;
        count[bc] = -1;
      }
      set[0] = 0; for (k = 0; k < n; k++) set[k + 1] = chosen[k];
      for (k = 0; k < 16; k++) code[k] = 0;
      for (k = 0; k < n; k++) code[chosen[k]] = (u8)(k + 1); /* 01 = screen hi nibble, 10 = screen lo nibble, 11 = colour RAM */
      for (row = 0; row < 8; row++) {
        u8 byte = 0;
        for (px = 0; px < 4; px++) {
          u8 c = mc[cy * 8 + row][cx * 4 + px], cc = code[c];
          if (c != 0 && cc == 0) cc = code[nearest(c, set, (u8)(n + 1))]; /* a colour that did not make the cut */
          byte = (u8)((byte << 2) | cc);
        }
        A->bmp[(cy * 40 + cx) * 8 + row] = byte;
      }
      A->scr[cy * 40 + cx] = (u8)((chosen[0] << 4) | chosen[1]);
      A->col[cy * 40 + cx] = chosen[2];
    }
  /* shop names on the shop block, the title in the middle, doors, the side panel labels */
  for (k = 0; k < 6; k++) {
    int len = (int)strlen(SHOPNAME[k]), px = CX + ((SNX[k] * 36) >> 7), py = CY + ((SNY[k] * 36) >> 7);
    put_text(A, (u8)((px - len * 4) / 8), (u8)((py - 4) / 8), SHOPNAME[k], SHOPCOL[shop_kind[k]]);
    put_cell(A, (u8)(doorx[k] >> 3), (u8)(doory[k] >> 3), GL_DOLLAR, SHOPCOL[shop_kind[k]]);
  }
  put_text(A, 16, 11, "CHOPPING", 10);
  put_text(A, 18, 12, "MALL", 3);
  put_text(A, 0, 1, "SCORE", 3); put_text(A, 0, 4, "LEVEL", 3); put_text(A, 0, 7, "LIVES", 3); put_text(A, 0, 10, "HEALTH", 3);
  put_text(A, 35, 1, "TIME", 3); put_text(A, 35, 4, "BOTS", 3); put_text(A, 35, 8, "ARMS", 3);
  put_text(A, 0, 23, "CHOPPING", 4); put_text(A, 0, 24, "MALL 64", 4);
}

/* debug preview: decodes the finished bitmap back to RGB (P6 ppm, 320x200) */
void picture_preview(const Assets *A, unsigned char *rgb) {
  int cx, cy, row, px, y, x;
  for (cy = 0; cy < 25; cy++)
    for (cx = 0; cx < 40; cx++) {
      u8 s = A->scr[cy * 40 + cx], cr = A->col[cy * 40 + cx] & 15;
      u8 cols[4] = {0, (u8)(s >> 4), (u8)(s & 15), cr};
      for (row = 0; row < 8; row++) {
        u8 b = A->bmp[(cy * 40 + cx) * 8 + row];
        for (px = 0; px < 4; px++) {
          u8 c = cols[(b >> (6 - 2 * px)) & 3];
          y = cy * 8 + row; x = cx * 8 + px * 2;
          memcpy(rgb + (y * 320 + x) * 3, PAL[c], 3);
          memcpy(rgb + (y * 320 + x + 1) * 3, PAL[c], 3);
        }
      }
    }
}
