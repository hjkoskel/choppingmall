/* assets.h - character set / background / sprite assets shared by the generator and the C64 platform layer. */
#ifndef ASSETS_H
#define ASSETS_H
#include "types.h"

enum { SH_PLAYER = 0, SH_ROBOT = 16, SH_BULLET = 32, SH_BOLT = 33, SH_MOLOTOV = 34, SH_FIRE0 = 35, SH_FIRE1 = 36, SH_COUNT = 37 };

typedef struct {
  u8 bmp[8000];      /* multicolour bitmap of the whole static background, cell after cell (8 bytes per 4x8 cell) */
  u8 scr[1000], col[1000]; /* per cell colours: screen RAM (codes 01 / 10 as hi / lo nibble) and colour RAM (code 11) */
  u8 spr[SH_COUNT * 64];
  u8 walk[200 * 27]; /* valid(r = 5) for every pixel of the hexagon's bounding box, 1 bit each */
} Assets;

void assets_build(Assets *out);
void picture_build(Assets *A);                       /* picture.c */
void picture_glyph_bytes(u8 gi, u8 *out);            /* one font glyph as an 8-byte multicolour cell */
void picture_preview(const Assets *A, unsigned char *rgb); /* debug: decode the bitmap to 320x200 RGB */
#endif
