/* bot.c - autopilot: fetch ammo and health at the shop doors, then hunt the robot and shoot when it has a clear line. */
#include "bot.h"
#include "core.h"

static const u8 BITS8[8] = {IN_RIGHT, IN_RIGHT | IN_DOWN, IN_DOWN, IN_LEFT | IN_DOWN, IN_LEFT, IN_LEFT | IN_UP, IN_UP, IN_RIGHT | IN_UP};
static i8 bdir = 1;
static i16 lastx, lasty;
static u8 stuck, side;
static i8 sidesgn = 1;

static u8 to_bits(i16 dx, i16 dy, u8 away) {
  u8 a = ang32(dx, dy);
  if (away) a = (u8)((a + 16) & 31);
  return BITS8[((a + 2) >> 2) & 7];
}

void bot_reset(void) { bdir = 1; lastx = lasty = 0; stuck = 0; side = 0; sidesgn = 1; }

u8 bot_input(u8 *ev) {
  u8 want, bits, k, best = 255;
  i16 px, py, rx, ry, tx, ty, wx, wy, dx, dy, ax, ay;
  *ev = 0;
  if (G.mode == M_TITLE) { *ev = EV_START; return 0; }
  if ((G.mode == M_OVER || G.mode == M_WIN) && G.mt == 0) { *ev = EV_START; return 0; }
  if (G.mode != M_PLAY) return 0;
  px = G.px >> FP; py = G.py >> FP; rx = G.rx >> FP; ry = G.ry >> FP;
  dx = rx - px; dy = ry - py;
  ax = dx < 0 ? -dx : dx; ay = dy < 0 ? -dy : dy;
  /* weapon choice */
  want = G.w;
  if (G.ammo[1] && ax + ay < 70) want = WP_SHOTGUN;
  else if (G.ammo[2] && ax + ay > 40 && ax + ay < 90) want = WP_MOLOTOV;
  else if (G.ammo[0]) want = WP_PISTOL;
  else if (G.ammo[1]) want = WP_SHOTGUN;
  else if (G.ammo[2]) want = WP_MOLOTOV;
  if (want != G.w) *ev = want == 0 ? EV_W1 : want == 1 ? EV_W2 : EV_W3;
  /* shoot when armed and the robot is in sight and near */
  if (G.ammo[G.w] && G.los && ax + ay < 120) return IN_FIRE;
  /* where to go */
  if (G.ammo[0] + G.ammo[1] * 3 < 10) { /* nearest door that restocks */
    i16 bd = 32000;
    for (k = 0; k < 6; k++) {
      i16 ex, ey;
      if (G.cool[k] || shop_kind[k] == 3 || !shop_open(shop_kind[k])) continue;
      ex = doorx[k] - px; ey = doory[k] - py;
      if (ex < 0) ex = -ex;
      if (ey < 0) ey = -ey;
      if (ex + ey < bd) { bd = ex + ey; best = k; }
    }
  } else if (G.php < 500 && G.cool[2] == 0) best = 2;
  if (best != 255) { tx = doorx[best]; ty = doory[best]; } else { tx = rx; ty = ry; }
  nav_target(px, py, tx, ty, &bdir, &wx, &wy);
  bits = to_bits(wx - px, wy - py, best == 255 && ax + ay < 36);
  /* sidestep when wedged */
  if (px == lastx && py == lasty) stuck++; else stuck = 0;
  lastx = px; lasty = py;
  if (stuck > 12 && side == 0) { side = 30; sidesgn = (i8)-sidesgn; stuck = 0; }
  if (side) {
    side--;
    bits = to_bits(-(wy - py) * sidesgn, (wx - px) * sidesgn, 0);
  }
  return bits;
}
