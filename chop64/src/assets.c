/* assets.c - generates the C64 character set, background screen and sprite shapes from the same
 * geometry the game rules use (core.c). Portable C: it runs on the HOST at build time
 * (tools/gen_assets.c writes src/assets_gen.h); the C64 program only copies the finished tables.
 * (Generating them on the 6502 at start-up took ~45 s.) */
#include <string.h>
#include "assets.h"
#include "core.h"
#include "tables_gen.h"

#define SPRMEM (A->spr)
static Assets *A;

static void sp_px(u8 *b, i16 x, i16 y) { if (x >= 0 && x < 24 && y >= 0 && y < 21) b[y * 3 + (x >> 3)] |= (u8)(0x80 >> (x & 7)); }
static void sp_clr(u8 *b, i16 x, i16 y) { if (x >= 0 && x < 24 && y >= 0 && y < 21) b[y * 3 + (x >> 3)] &= (u8)~(0x80 >> (x & 7)); }
static void sp_disc(u8 *b, i16 cx, i16 cy, i16 r, u8 set) {
  i16 x, y;
  for (y = cy - r; y <= cy + r; y++)
    for (x = cx - r; x <= cx + r; x++)
      if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) { if (set) sp_px(b, x, y); else sp_clr(b, x, y); }
}
static void sp_line(u8 *b, u8 dir, i16 len, i16 start) {
  i16 t;
  for (t = start; t <= len; t++) {
    i16 x = 12 + ((COS32[dir] * t) >> 7), y = 10 + ((SIN32[dir] * t) >> 7);
    sp_px(b, x, y); sp_px(b, x + 1, y); sp_px(b, x, y + 1);
  }
}
/* The robot is a multicolour sprite too: pixel code 10 = body and arms (the sprite's own colour, dark grey), 01 = the red eye (shared colour $D025).
 * Body: a rectangle (half-length 9 along the facing, half-width 5) rotated to direction dir (32 steps); arms: a bar across the middle sticking out 5 pixels on each side. */
static void sp_robot(u8 *b, u8 dir) {
  i16 mx, y, c = COS32[dir], sn = SIN32[dir];
  for (y = 0; y < 21; y++)
    for (mx = 0; mx < 12; mx++) {
      i16 dx = (i16)(2 * mx + 1 - 12), dy = (i16)(y - 10);
      int32_t u = (int32_t)dx * c + (int32_t)dy * sn, v = -(int32_t)dx * sn + (int32_t)dy * c; /* Q7 */
      u8 code = 0;
      if (u >= -8 * 128 && u <= 8 * 128 && v >= -4 * 128 && v <= 4 * 128) code = 2;      /* body */
      if (u >= -(3 * 64) && u <= 3 * 64 && v >= -10 * 128 && v <= 10 * 128) code = 2;    /* arms out to the sides */
      if (code) b[y * 3 + (mx >> 2)] |= (u8)(code << (6 - 2 * (mx & 3)));
    }
  { /* red eye: a 1 x 2 pixel dot near the front */
    i16 x = (i16)(12 + ((c * 6) >> 7)), yy = (i16)(10 + ((sn * 6) >> 7)), m = (i16)(x >> 1), k;
    for (k = 0; k < 2; k++) {
      i16 y2 = (i16)(yy + k);
      if (m >= 0 && m < 12 && y2 >= 0 && y2 < 21) {
        u8 sh = (u8)(6 - 2 * (m & 3));
        b[y2 * 3 + (m >> 2)] = (u8)((b[y2 * 3 + (m >> 2)] & ~(3 << sh)) | (1 << sh));
      }
    }
  }
}
/* The player is a MULTICOLOUR sprite (12 double-wide pixels x 21; 3 colours from one hardware sprite, no overlay sprite needed):
 * pixel code 10 = body (the sprite's own colour), 01 = head (shared colour $D025), 11 = gun barrel (shared colour $D026). */
static void sp_player(u8 *b, u8 dir) {
  i16 mx, y, t;
  for (y = 0; y < 21; y++)
    for (mx = 0; mx < 12; mx++) {
      i16 dx = (i16)(2 * mx + 1 - 12), dy = (i16)(y - 10), r2 = (i16)(dx * dx + dy * dy);
      u8 code = r2 <= 42 ? 2 : 0;   /* body: disc of radius 6.5 */
      if (r2 <= 10) code = 1;       /* head: a solid skin-coloured disc in the middle of the body (no hole) */
      if (code) b[y * 3 + (mx >> 2)] |= (u8)(code << (6 - 2 * (mx & 3)));
    }
  for (t = 4; t <= 11; t++) { /* gun barrel, pointing the way the player aims */
    i16 x = (i16)(12 + ((COS32[dir] * t) >> 7)), y = (i16)(10 + ((SIN32[dir] * t) >> 7)), m = (i16)(x >> 1);
    if (x < 0 || x > 23 || y < 0 || y > 20) continue;
    b[y * 3 + (m >> 2)] |= (u8)(3 << (6 - 2 * (m & 3)));
  }
}
static void build_sprites(void) {
  u8 d, f;
  u8 *b;
  for (d = 0; d < 16; d++) {
    u8 dir = (u8)(d * 2);
    b = SPRMEM + (SH_PLAYER + d) * 64; /* player: multicolour body, head and gun barrel */
    sp_player(b, dir);
    b = SPRMEM + (SH_ROBOT + d) * 64;  /* robot: multicolour: grey rectangle with arms out to the sides and a red eye */
    sp_robot(b, dir);
  }
  b = SPRMEM + SH_BULLET * 64; sp_disc(b, 12, 10, 1, 1);
  b = SPRMEM + SH_BOLT * 64; sp_disc(b, 12, 10, 3, 1);
  b = SPRMEM + SH_MOLOTOV * 64; { i16 x, y; for (y = 5; y < 16; y++) for (x = 9; x < 15; x++) if (y > 7 || (x > 10 && x < 13)) sp_px(b, x, y); }
  for (f = 0; f < 2; f++) { /* fire: a ragged disc, two animation frames */
    i16 x, y;
    b = SPRMEM + (SH_FIRE0 + f) * 64;
    sp_disc(b, 12, 10, 10, 1);
    for (y = 0; y < 21; y++) for (x = 0; x < 24; x++) if (((x * 5 + y * 3 + f * 7) % 5) == 0) sp_clr(b, x, y);
  }
}

void assets_build(Assets *out) {
  A = out;
  memset(A, 0, sizeof *A);
  picture_build(A);
  build_sprites();
  { i16 x, y; /* precompute the collision test (the very same valid() the game uses) */
    for (y = 0; y < 200; y++)
      for (x = WALK_X0; x < WALK_X0 + WALK_W; x++)
        if (valid((i16)(x << FP), (i16)(y << FP), 5)) A->walk[y * (WALK_W / 8) + ((x - WALK_X0) >> 3)] |= (u8)(0x80 >> ((x - WALK_X0) & 7));
  }
}
