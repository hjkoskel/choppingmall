/* core.c - portable game core (see core.h). C89-style for cc65: declarations first, no floats, and
 * every intermediate stays within +-32767 so int16 (C64) and int32 (host) agree. */
#include "core.h"
#include "plat.h"
#include "tables_gen.h"

Game G;
i16 obx[NOB], oby[NOB], doorx[6], doory[6];
static u8 obg[1000];     /* 1 where an 8x8 cell is within reach of an obstacle: valid() only runs the exact test there */
static u16 rowoff[25];
static const u8 *walkmap;   /* optional 1 bit per pixel: valid(r = 5) precomputed at build time (see assets.c) */
static u16 rowoff26[200];
static const u8 BITM[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
const u8 shop_kind[6] = {1, 2, 3, 0, 1, 0}; /* SHELLS GAS FIRST AID AMMO SHELLS AMMO */

static i16 clx[32], cly[32]; /* centre-line waypoints per direction, px offsets from the centre */
static const i8 NX[6] = {111, 0, -111, -111, 0, 111}; /* hexagon side normals (30 + 60k degrees), Q7 */
static const i8 NY[6] = {64, 127, 64, -64, -127, -64};
static const u8 DIRTAB[16] = {255, 24, 8, 255, 16, 20, 12, 255, 0, 28, 4, 255, 255, 255, 255, 255}; /* up,down,left,right bits */
static const i16 RHP[3] = {300, 450, 600};   /* robot health, tenths */
static const u8 RSPD[3] = {12, 13, 15};      /* robot speed, 1/16 px per frame */
static const u8 RINT[3] = {130, 110, 90};    /* robot shot interval, frames */
static const i8 OFFA[7] = {0, 3, -3, 5, -5, 8, -8};
static const i8 OFFB[7] = {0, -3, 3, -5, 5, -8, 8};
static const u8 AMMO_MAX[3] = {15, 8, 6};
/* per-direction velocity tables: a signed 16x8 multiply costs ~600 cycles on a 6502, a lookup ~20 */
static i16 pvx[32], pvy[32];                  /* player, 17 / frame */
static i16 b97x[32], b97y[32], b86x[32], b86y[32]; /* pistol bullet, shotgun pellet */
static i16 rvx[32], rvy[32], rsx[32], rsy[32], rbx[32], rby[32]; /* robot: normal, slowed, reverse; set per level */
static u8 rsp_n, rsp_s, rrev_step;
static const u8 RESTOCK[3] = {6, 2, 2}; /* given per shop visit (hard mode; the original game gave 18 / 6 / 2) */

#ifdef CORE_PROF /* section timing for the C64 performance probe (plat_c64.c provides the timer) */
extern u16 prof_timer(void);
extern void prof_rec(u8 k, u16 t0);
#define PROF(k, stmt) do { u16 t0_ = prof_timer(); stmt; prof_rec(k, t0_); } while (0)
#else
#define PROF(k, stmt) stmt
#endif
#define PSPEED 17 /* player, 1/16 px per frame (165 px/s * .32 / 50) */
#define IABS(v) ((v) < 0 ? -(v) : (v))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))


/* ------------------------------------------------------------------ geometry */
/* js: maxDot -- hexagon "apothem distance" of an offset from the centre: max(|dy|, .866|dx| + .5|dy|).
 * Table lookup instead of a multiply: this is the hottest function on a 6502. */
u16 mdot(i16 dx, i16 dy) {
  u8 ax, ay;
  u16 m;
  if (dx < 0) dx = -dx;
  if (dy < 0) dy = -dy;
  ax = dx > 250 ? 250 : (u8)dx;
  ay = dy > 250 ? 250 : (u8)dy;
  m = (u16)(T87[ax] + (ay >> 1));
  return ay > m ? ay : m;
}

/* js: valid -- is a circle of radius r (px) at (x16,y16) inside the corridor and clear of obstacles.
 * The hexagon test comes first (it also guarantees x, y are on screen); the obstacle loop only runs for
 * cells flagged in the coverage grid. */
u8 valid(i16 x16, i16 y16, u8 r) {
  i16 x = x16 >> FP, y = y16 >> FP;
  u16 m, xi;
  u8 i, lim;
  if (r == 5 && walkmap) { /* exactly the same answer as the code below, from the precomputed bitmap */
    if (y < 0 || y > 199 || x < WALK_X0 || x >= WALK_X0 + WALK_W) return 0;
    xi = (u16)(x - WALK_X0);
    return (u8)((walkmap[rowoff26[y] + (xi >> 3)] & BITM[xi & 7]) != 0);
  }
  m = mdot(x - CX, y - CY);
  if (m > (u16)(AO - r) || m < (u16)(AI + r)) return 0;
  if (!obg[rowoff[y >> 3] + (x >> 3)]) return 1;
  lim = (u8)(OBH + r);
  for (i = 0; i < NOB; i++) /* |x - ox| < lim as a single unsigned compare */
    if ((u16)(x - obx[i] + lim) < (u16)(2 * lim) && (u16)(y - oby[i] + lim) < (u16)(2 * lim)) return 0;
  return 1;
}

static u8 oct(u8 n, u8 d) { /* atan(n/d) in 11.25 degree steps (0..4), 0 <= n <= d: an early-out chain of 8-bit compares */
  if (n < ANG_T1[d]) return 0;
  if (n < ANG_T2[d]) return 1;
  if (n < ANG_T3[d]) return 2;
  if (n < ANG_T4[d]) return 3;
  return 4;
}
/* js: Math.atan2(dy, dx) quantised to 32 directions, clockwise from +x */
u8 ang32(i16 dx, i16 dy) {
  u16 ax = (u16)(dx < 0 ? -dx : dx), ay = (u16)(dy < 0 ? -dy : dy);
  u8 a, x, y;
  if (!ax && !ay) return 0;
  while ((ax | ay) > 255) { ax >>= 1; ay >>= 1; } /* scale into 8 bits, keeping the ratio */
  x = (u8)ax; y = (u8)ay;
  a = x >= y ? oct(y, x) : (u8)(8 - oct(x, y));
  if (dx >= 0) return dy >= 0 ? a : (u8)((32 - a) & 31);
  return dy >= 0 ? (u8)(16 - a) : (u8)(16 + a);
}
/* js: angTo -- turn cur toward tgt by at most step */
static u8 turn_toward(u8 cur, u8 tgt, u8 step) {
  u8 d = (u8)((tgt - cur) & 31);
  if (d == 0) return cur;
  if (d <= 16) return d <= step ? tgt : (u8)((cur + step) & 31);
  return (u8)(32 - d) <= step ? tgt : (u8)((cur + 32 - step) & 31);
}
static u16 dist_px(i16 dx, i16 dy) { /* octagonal distance estimate (max + min/2), px */
  u16 ax = (u16)IABS(dx), ay = (u16)IABS(dy);
  return ax > ay ? ax + (ay >> 1) : ay + (ax >> 1);
}
static void move_ent(i16 *x, i16 *y, i16 dx, i16 dy, u8 r) { /* js: move (per-axis sliding) */
  if (dx && valid(*x + dx, *y, r)) *x += dx;
  if (dy && valid(*x, *y + dy, r)) *y += dy;
}

/* js: clearShot (px coordinates). Samples every 1/8 of the segment, stepping incrementally (no multiply or
 * divide per sample) and only testing obstacles that lie near the segment. */
u8 clear_shot(i16 x1, i16 y1, i16 x2, i16 y2) {
  i16 sx = x2 - x1, sy = y2 - y1, x16 = x1 << FP, y16 = y1 << FP, x, y;
  i16 lox = (x1 < x2 ? x1 : x2) - OBH, hix = (x1 < x2 ? x2 : x1) + OBH;
  i16 loy = (y1 < y2 ? y1 : y2) - OBH, hiy = (y1 < y2 ? y2 : y1) + OBH;
  u8 cand[NOB], nc = 0, i, j;
  for (j = 0; j < NOB; j++)
    if (obx[j] >= lox && obx[j] <= hix && oby[j] >= loy && oby[j] <= hiy) cand[nc++] = j;
  sx <<= 1; sy <<= 1; /* step of 2/16 of the segment, in 1/16 px */
  for (i = 1; i < 8; i++) {
    x16 += sx; y16 += sy;
    x = x16 >> FP; y = y16 >> FP;
    if (mdot(x - CX, y - CY) < AI + 2) return 0;
    for (j = 0; j < nc; j++)
      if ((u16)(x - obx[cand[j]] + OBH) < (u16)(2 * OBH) && (u16)(y - oby[cand[j]] + OBH) < (u16)(2 * OBH)) return 0;
  }
  return 1;
}

/* js: robotTarget -- if the straight line crosses the shop block, steer along the corridor centre line */
u8 nav_target(i16 sx, i16 sy, i16 tx, i16 ty, i8 *dir, i16 *wx, i16 *wy) {
  i16 dx = (tx - sx), dy = (ty - sy), x16 = sx << FP, y16 = sy << FP, d;
  u8 i, ar, ap, a;
  dx <<= 1; dy <<= 1; /* 12 samples: step = 1/8 of the segment (in 1/16 px: delta * 16 / 8) */
  for (i = 1; i < 8; i++) {
    x16 += dx; y16 += dy;
    if (mdot((x16 >> FP) - CX, (y16 >> FP) - CY) < AI + 8) break;
  }
  if (i >= 8) { *wx = tx; *wy = ty; return 0; }
  ar = ang32(sx - CX, sy - CY);
  ap = ang32(tx - CX, ty - CY);
  d = (i16)((ap - ar) & 31);
  if (d > 16) d -= 32;
  if (IABS(d) <= 13) *dir = d < 0 ? -1 : 1; /* js: Math.sign(d) || 1 */
  a = (u8)((ar + *dir * 3) & 31);
  *wx = CX + clx[a];
  *wy = CY + cly[a];
  return 1;
}

void core_set_walkmap(const u8 *map) { walkmap = map; }

static void memset_obg(void) { u16 i; for (i = 0; i < 1000; i++) obg[i] = 0; }

static void world_init(void) {
  u8 k, i;
  i16 d, t, cx, cy;
  for (k = 0; k < 6; k++) {
    doorx[k] = CX + ((NX[k] * 57) >> 7); /* js: polar(k, AI + 26) scaled */
    doory[k] = CY + ((NY[k] * 57) >> 7);
    d = (k & 1) ? 66 : 78; /* near the centre line (72): >= 12 px stays free on both sides */
    t = (k & 1) ? 24 : -24;
    obx[k] = CX + ((NX[k] * d - NY[k] * t) >> 7);
    oby[k] = CY + ((NY[k] * d + NX[k] * t) >> 7);
  }
  for (i = 0; i < 25; i++) rowoff[i] = (u16)(i * 40);
  for (cx = 0; cx < 200; cx++) rowoff26[cx] = (u16)(cx * (WALK_W / 8));
  memset_obg();
  for (k = 0; k < NOB; k++)
    for (cy = (oby[k] - OBH - 6) >> 3; cy <= (oby[k] + OBH + 6) >> 3; cy++)
      for (cx = (obx[k] - OBH - 6) >> 3; cx <= (obx[k] + OBH + 6) >> 3; cx++) obg[rowoff[cy] + cx] = 1;
  for (i = 0; i < 32; i++) {
    pvx[i] = (i16)((PSPEED * COS32[i]) >> 7); pvy[i] = (i16)((PSPEED * SIN32[i]) >> 7);
    b97x[i] = (i16)((97 * COS32[i]) >> 7); b97y[i] = (i16)((97 * SIN32[i]) >> 7);
    b86x[i] = (i16)((86 * COS32[i]) >> 7); b86y[i] = (i16)((86 * SIN32[i]) >> 7);
  }
  for (i = 0; i < 32; i++) {
    i16 ux = COS32[i], uy = SIN32[i];
    u16 m = mdot(ux, uy); /* in Q7 units */
    clx[i] = (i16)((ux * 72) / (i16)m); /* centre line: apothem 72 = (AO + AI) / 2 */
    cly[i] = (i16)((uy * 72) / (i16)m);
  }
}

/* ------------------------------------------------------------------ level flow */
static void polar16(u8 k, i16 d, i16 *x, i16 *y) {
  *x = (CX + ((NX[k] * d) >> 7)) << FP;
  *y = (CY + ((NY[k] * d) >> 7)) << FP;
}
static void spawn(void) { /* js: spawn */
  polar16(5, 72, &G.px, &G.py);
  polar16(2, 72, &G.rx, &G.ry);
  G.pface = 14; /* js: Math.PI * .9 */
  G.rface = 0;
  G.pinv = 125;
  G.paim = 0;
  G.rpause = 80;
  G.rrev = 0;
  G.rstk = 0;
  G.rwx = G.rwy = 0;
}
static void start_level(void) { /* js: startLevel */
  u8 i, l = (u8)(G.level - 1);
  rsp_n = RSPD[l]; rsp_s = (u8)((rsp_n * 9) >> 4); rrev_step = (u8)((rsp_n * 13) >> 4);
  for (i = 0; i < 32; i++) {
    /* NB (i16) casts: cc65 promotes an unsigned char operand to UNSIGNED int, which would turn the product and the
     * arithmetic '>> 7' unsigned (a -3 became 509 on the C64). Always widen to i16 before multiplying by a signed table. */
    rvx[i] = (i16)(((i16)rsp_n * COS32[i]) >> 7); rvy[i] = (i16)(((i16)rsp_n * SIN32[i]) >> 7);
    rsx[i] = (i16)(((i16)rsp_s * COS32[i]) >> 7); rsy[i] = (i16)(((i16)rsp_s * SIN32[i]) >> 7);
    rbx[i] = (i16)(((i16)rrev_step * COS32[i]) >> 7); rby[i] = (i16)(((i16)rrev_step * SIN32[i]) >> 7);
  }
  if (G.w >= G.level) G.w = 0; /* a weapon the level does not offer */
  G.php = 1000;
#ifdef DEBUG_HP
  G.php = DEBUG_HP; /* debug: start every life with this much health (tenths) */
#endif
  G.rmax = G.rhp = RHP[l];
  G.rdead = 0; G.rslow = 0; G.rcd = 0; G.rwind = 0; G.rsh = 90;
  G.bolt.life = 0;
  for (i = 0; i < 3; i++) G.pr[i].kind = PK_NONE;
  for (i = 0; i < 2; i++) G.fire[i].life = 0;
  for (i = 0; i < 6; i++) G.cool[i] = 0;
  G.lt = 0;
  G.rside = 1;
  spawn();
  G.mode = M_INTRO;
  G.mt = 120;
}
static void new_game(void) { /* js: newGame */
  snd_new_game();
  G.score = 0; G.lives = 3; G.level = 1; G.tsec = 0; G.tfr = 0; G.kills = 0; G.w = 0;
  G.ammo[0] = G.ammo[1] = G.ammo[2] = 0;
  G.toast = 0; G.tt = 0;
  start_level();
}
void game_init(u16 seed) {
  (void)seed; /* the core is deterministic: no randomness is used */
  world_init();
  G.mode = M_TITLE; G.lives = 3; G.level = 1; G.php = 1000; G.rdir = 1; G.rside = 1;
  G.rmax = G.rhp = RHP[0];
  polar16(5, 72, &G.px, &G.py);
  polar16(2, 72, &G.rx, &G.ry);
}

static void toast(const char *s) { G.toast = s; G.tt = 90; }

static void kill_robot(void) { /* js: killRobot */
  i16 sec = (i16)(G.lt / 50);
  u8 i;
  G.rdead = 1;
  G.kills++;
  G.score += 1000UL * G.level + (sec < 90 ? (u32)(90 - sec) * 10 : 0);
  snd_sfx(90, 40, W_SAW, 25);
  snd_jingle(JINGLE_WIN);
  G.mode = M_CLEAR;
  G.mt = 130;
  for (i = 0; i < 3; i++) G.pr[i].kind = PK_NONE;
  G.bolt.life = 0;
}

static void hurt_player(i16 tenths) { /* js: hurtPlayer */
  G.php -= tenths;
  snd_sfx(140, 10, W_SAW, 60);
}

/* ------------------------------------------------------------------ weapons */
static Proj *free_proj(void) {
  u8 i;
  for (i = 0; i < 3; i++) if (G.pr[i].kind == PK_NONE) return &G.pr[i];
  return 0;
}
static u8 nfree_proj(void) {
  u8 i, n = 0;
  for (i = 0; i < 3; i++) if (G.pr[i].kind == PK_NONE) n++;
  return n;
}
static void shoot(void) { /* js: shoot */
  u8 w = G.w, f = G.pface, i;
  i16 mx, my, d, tx, ty;
  Proj *p;
  if (G.ammo[w] == 0) {
    G.pcd = 18;
    snd_sfx(120, 3, W_SQUARE, 120);
    toast("EMPTY");
    return;
  }
  mx = G.px + ((COS32[f] * 96) >> 7); /* muzzle 6 px ahead */
  my = G.py + ((SIN32[f] * 96) >> 7);
  if (w == WP_PISTOL) {
    p = free_proj();
    if (!p) return;
    G.ammo[0]--;
    p->kind = PK_BULLET; p->x = mx; p->y = my; p->vx = b97x[f]; p->vy = b97y[f];
    p->life = 42; p->dmg = 30; p->slow = 6;
    G.pcd = 16;
    snd_sfx(900, 5, W_SQUARE, 200);
  } else if (w == WP_SHOTGUN) {
    if (nfree_proj() < 3) return;
    G.ammo[1]--;
    for (i = 0; i < 3; i++) {
      u8 a = (u8)((f + 31 + i) & 31); /* f-1, f, f+1 */
      p = free_proj();
      p->kind = PK_PELLET; p->x = mx; p->y = my; p->vx = b86x[a]; p->vy = b86y[a];
      p->life = 17; p->dmg = 32; p->slow = 18;
    }
    G.pcd = 42;
    snd_sfx(160, 12, W_SAW, 40);
  } else {
    p = free_proj();
    if (!p) return;
    G.ammo[2]--;
    d = (i16)dist_px((G.rx - G.px) >> FP, (G.ry - G.py) >> FP); /* js: clamp(90..230) scaled */
    if (d < 29) d = 29;
    if (d > 74) d = 74;
    for (;;) {
      tx = G.px + ((COS32[f] * d) >> 3);
      ty = G.py + ((SIN32[f] * d) >> 3);
      if (valid(tx, ty, 1) || d <= 10) break;
      d -= 4;
    }
    p->kind = PK_MOLOTOV; p->x = mx; p->y = my; p->tx = tx; p->ty = ty; p->life = 27;
    p->vx = (tx - mx) / 27; p->vy = (ty - my) / 27;
    G.pcd = 65;
    snd_sfx(400, 10, W_TRI, 700);
  }
}

static void add_fire(i16 x, i16 y) {
  u8 i, slot = 0;
  for (i = 0; i < 2; i++) { if (G.fire[i].life == 0) { slot = i; break; } if (G.fire[i].life < G.fire[slot].life) slot = i; }
  G.fire[slot].x = x; G.fire[slot].y = y; G.fire[slot].life = 350;
}

static void projectiles(void) { /* js: molotovs in flight + bullets */
  u8 i, s;
  Proj *p;
  for (i = 0; i < 3; i++) {
    p = &G.pr[i];
    if (p->kind == PK_NONE) continue;
    if (p->kind == PK_MOLOTOV) {
      p->x += p->vx; p->y += p->vy;
      if (--p->life == 0) {
        add_fire(p->tx, p->ty);
        snd_sfx(200, 20, W_SAW, 50);
        p->kind = PK_NONE;
      }
      continue;
    }
    for (s = 0; s < 2 && p->kind != PK_NONE; s++) {
      /* two sub-steps that add up to exactly v (an explicit shift: cc65 rounds a signed '/ 2' differently from gcc) */
      p->x += s ? (i16)(p->vx - (p->vx >> 1)) : (i16)(p->vx >> 1);
      p->y += s ? (i16)(p->vy - (p->vy >> 1)) : (i16)(p->vy >> 1);
      if (!G.rdead && IABS((p->x - G.rx) >> FP) < 6 && IABS((p->y - G.ry) >> FP) < 6) {
        p->kind = PK_NONE;
        G.rhp -= p->dmg;
        if (p->slow > G.rslow) G.rslow = p->slow;
        G.score += 10;
        snd_sfx(300, 3, W_SQUARE, 300);
        if (G.rhp <= 0) { kill_robot(); return; }
        break;
      }
      if (!valid(p->x, p->y, 0)) p->kind = PK_NONE; /* wall or obstacle */
    }
    if (p->kind != PK_NONE && --p->life == 0) p->kind = PK_NONE;
  }
}

/* ------------------------------------------------------------------ robot */
static void robot(void) { /* js: the robot part of updateGame */
  i16 dx = (G.px - G.rx) >> FP, dy = (G.py - G.ry) >> FP, nx, ny;
  u8 sp;
  u16 dd = dist_px(dx, dy);
  u8 i, base, cand;
  const i8 *offs;
  if (G.rslow) G.rslow--;
  if (G.rcd) G.rcd--;
  if (G.rsh) G.rsh--;
  if ((G.tick & 15) == 3) PROF(4, G.los = clear_shot(G.rx >> FP, G.ry >> FP, G.px >> FP, G.py >> FP));
  if (G.rwind) {
    G.rwind--;
    if ((G.tick & 1) == 1) G.rang = ang32(dx, dy);
    G.rface = turn_toward(G.rface, G.rang, 2);
    if (G.rwind == 0 && G.bolt.life == 0) { /* js: bolt leaves 24 px (7.7 scaled) ahead at 340 px/s (2.2 px/frame) */
      G.bolt.x = G.rx + ((COS32[G.rface] * 123) >> 7);
      G.bolt.y = G.ry + ((SIN32[G.rface] * 123) >> 7);
      G.bolt.vx = (35 * COS32[G.rface]) >> 7;
      G.bolt.vy = (35 * SIN32[G.rface]) >> 7;
      G.bolt.life = 110;
      snd_sfx(700, 9, W_SAW, 150);
    }
  } else if (G.rsh == 0 && G.rpause == 0 && dd < 166 && G.los) {
    G.rwind = 20;
    G.rsh = RINT[G.level - 1];
    snd_sfx(300, 20, W_TRI, 900);
  }
  sp = G.rslow ? rsp_s : rsp_n;
  if (G.rpause) { G.rpause--; sp = 0; }
  if (G.rwind) sp = 0;
  if (sp > 0) {
    if (G.rrev > 0) {
      nx = G.rx - rbx[G.rface];
      ny = G.ry - rby[G.rface];
      if (valid(nx, ny, 5)) { G.rx = nx; G.ry = ny; G.rrev = G.rrev > rrev_step ? (u16)(G.rrev - rrev_step) : 0; } else G.rrev = 0;
      if (G.rrev <= 3) { G.rrev = 0; G.rside = (i8)-G.rside; G.rstk = 0; }
    } else {
      if ((G.tick & 7) == 0 || (G.rwx == 0 && G.rwy == 0)) /* the waypoint only needs refreshing a few times per second */
        PROF(5, nav_target(G.rx >> FP, G.ry >> FP, G.px >> FP, G.py >> FP, &G.rdir, &G.rwx, &G.rwy));
      if ((G.tick & 3) == 1) PROF(6, G.rbase = ang32(G.rwx - (G.rx >> FP), G.rwy - (G.ry >> FP))); /* cached steering angle */
      base = G.rbase;
      offs = G.rside > 0 ? OFFA : OFFB;
      for (i = 0; i < 7; i++) {
        cand = (u8)((base + 32 + offs[i]) & 31);
        nx = G.rx + (G.rslow ? rsx[cand] : rvx[cand]);
        ny = G.ry + (G.rslow ? rsy[cand] : rvy[cand]);
        if (valid(nx, ny, 5)) {
          G.rx = nx; G.ry = ny;
          G.rface = turn_toward(G.rface, cand, 1);
          break;
        }
      }
      /* js: prog < .25 means the heading is more than ~75 degrees off the target: the +-8 step detours (or no move at all) */
      if (i >= 5) G.rstk++;
      else G.rstk = G.rstk > 2 ? G.rstk - 2 : 0;
      if (G.rstk > 17) { G.rrev = 490; G.rstk = 0; snd_sfx(820, 6, W_SQUARE, 820); }
    }
  } else if (!G.rwind) {
    if ((G.tick & 1) == 1) G.rang = ang32(dx, dy);
    G.rface = turn_toward(G.rface, G.rang, 1);
  }
  if ((G.tick & 63) == 0 && dd < 224 && G.los) snd_clank();
  /* contact */
  if (IABS(dx) < 9 && IABS(dy) < 9 && G.pinv == 0 && G.rcd == 0) {
    u8 a = ang32(dx, dy);
    i16 d, kx, ky;
    hurt_player(220);
    G.pinv = 50; G.rcd = 45; G.rpause = 22;
    for (d = 11 * 16; d > 0; d -= 96) { /* knock the player back up to 11 px */
      kx = G.px + ((COS32[a] * d) >> 7);
      ky = G.py + ((SIN32[a] * d) >> 7);
      if (valid(kx, ky, 5)) { G.px = kx; G.py = ky; break; }
    }
  }
  if (G.php <= 0) { /* js: player death */
    G.php = 0;
    G.lives--;
    snd_sfx(110, 35, W_SAW, 30);
    snd_jingle(JINGLE_LOSE);
    if (G.lives == 0) { G.mode = M_OVER; G.mt = 60; }
    else { G.mode = M_LOST; G.mt = 90; }
  }
}

/* ------------------------------------------------------------------ main update */
static void player_update(u8 inp) {
  u8 d = DIRTAB[inp & 15];
  if (inp & IN_FIRE) {
    if (G.paim < 255) G.paim++;
    if ((G.tick & 3) == 3 || G.paim == 1) G.paimang = ang32((G.rx - G.px) >> FP, (G.ry - G.py) >> FP); /* cached: the robot moves slowly */
    G.pface = turn_toward(G.pface, G.paimang, 1);
    if (G.paim > 15 && G.pcd == 0) shoot();
  } else {
    G.paim = 0;
    if (d != 255) {
      G.pface = d;
      move_ent(&G.px, &G.py, pvx[d], pvy[d], 5);
    }
  }
}

static void shops_update(void) { /* js: shop doors */
  u8 k, t, i;
  i16 dx, dy;
  for (k = 0; k < 6; k++) { /* called every 4th tick: cooldowns count down in steps of 4 */
    if (G.cool[k]) { G.cool[k] = G.cool[k] > 4 ? (u16)(G.cool[k] - 4) : 0; continue; }
    dx = (G.px >> FP) - doorx[k];
    dy = (G.py >> FP) - doory[k];
    if (IABS(dx) > 12 || IABS(dy) > 12) continue;
    t = shop_kind[k];
    if (!shop_open(t)) continue; /* ammo for a weapon this level does not have */
    if (t == 3) {
      if (G.php < 1000) {
        G.php += 450;
        if (G.php > 1000) G.php = 1000;
        toast("+HEAL");
        G.cool[k] = 1100; G.score += 50; snd_sfx(600, 6, W_TRI, 1000);
      }
    } else if (G.ammo[t] < AMMO_MAX[t]) {
      i = (u8)(G.ammo[t] + RESTOCK[t]);
      G.ammo[t] = i > AMMO_MAX[t] ? AMMO_MAX[t] : i;
      toast(t == 0 ? "+AMMO" : t == 1 ? "+SHEL" : "+GAS");
      G.cool[k] = 1100; G.score += 50; snd_sfx(600, 6, W_TRI, 1000);
    }
  }
}

static void play(u8 inp) {
  u8 i;
  Fire *f;
  G.tfr++;
  if (G.tfr >= 50) { G.tfr = 0; G.tsec++; }
  G.lt++;
  /* score only grows from robot hits (+10), kills (bonus) and shop pick-ups (+50): no passive time score */
  if (G.pcd) G.pcd--;
  if (G.pinv) G.pinv--;
  if (G.pflash) G.pflash--;
  PROF(0, player_update(inp));
  if ((G.tick & 3) == 2) PROF(1, shops_update()); /* a phase of its own: nav_target / clear_shot / angle refreshes run on the others */
  PROF(2, projectiles());
  if (G.mode != M_PLAY) return;
  if (G.bolt.life) { /* js: robot bolts */
    G.bolt.x += G.bolt.vx; G.bolt.y += G.bolt.vy; G.bolt.life--;
    if (G.pinv == 0 && IABS((G.bolt.x - G.px) >> FP) < 5 && IABS((G.bolt.y - G.py) >> FP) < 5) {
      G.bolt.life = 0; hurt_player(120); G.pinv = 25;
    } else if (!valid(G.bolt.x, G.bolt.y, 0)) G.bolt.life = 0;
  }
  for (i = 0; i < 2; i++) { /* js: fire damage */
    f = &G.fire[i];
    if (!f->life) continue;
    if (dist_px((G.px - f->x) >> FP, (G.py - f->y) >> FP) < 12) G.php -= 3;
    if (!G.rdead && dist_px((G.rx - f->x) >> FP, (G.ry - f->y) >> FP) < 15) {
      G.rhp -= (G.tick & 1) ? 3 : 2;
      if (G.rhp <= 0) { kill_robot(); return; }
    }
  }
  PROF(3, robot());
}

void game_tick(u8 inp, u8 ev) {
  u8 i;
  G.tick++;
  if (ev & EV_W1) G.w = 0;
  if ((ev & EV_W2) && W_AVAIL(1)) G.w = 1;
  if ((ev & EV_W3) && W_AVAIL(2)) G.w = 2;
  if (G.tt) G.tt--;
  for (i = 0; i < 2; i++) if (G.fire[i].life) G.fire[i].life--;
  if (ev & EV_RESTART) { new_game(); return; }
  switch (G.mode) {
  case M_TITLE: if (ev & EV_START) new_game(); break;
  case M_INTRO: if (G.mt) G.mt--; if (G.mt == 0) G.mode = M_PLAY; break;
  case M_LOST:
    if (G.mt) G.mt--;
    if (G.mt == 0) {
      spawn(); G.php = 1000;
      G.fire[0].life = G.fire[1].life = 0;
      for (i = 0; i < 3; i++) G.pr[i].kind = PK_NONE;
      G.bolt.life = 0; G.rwind = 0; G.rsh = 90;
      G.mode = M_PLAY;
    }
    break;
  case M_CLEAR:
    if (G.mt) G.mt--;
    if (G.mt == 0) {
      if (G.level >= 3) { G.mode = M_WIN; G.mt = 60; }
      else { G.level++; start_level(); }
    }
    break;
  case M_OVER:
  case M_WIN:
    if (G.mt) G.mt--;
    else if (ev & EV_START) new_game();
    break;
  default: play(inp); break;
  }
}

/* A 16-bit hash of the whole game state, for host-vs-6502 parity tests (identical arithmetic on both). */
#ifndef HASH_MASK
#define HASH_MASK 0xFF
#endif
u16 game_hash(void) {
  u16 h = 0x1234;
  u8 i;
#define H(v) h = (u16)((u16)(h * 31u) + (u16)(v))
  if (HASH_MASK & 1) { H(G.mode); H(G.mt); H((u16)G.score); H((u16)(G.score >> 16)); H(G.lives); H(G.level); H(G.kills); H(G.w);
    H(G.ammo[0]); H(G.ammo[1]); H(G.ammo[2]); for (i = 0; i < 6; i++) H(G.cool[i]); H(G.tsec); H(G.lt); }
  if (HASH_MASK & 2) { H(G.px); H(G.py); H(G.php); H(G.pface); H(G.paim); H(G.pcd); H(G.pinv); }
  if (HASH_MASK & 4) { H(G.rx); H(G.ry); H(G.rhp); H(G.rface); H(G.rslow); H(G.rcd); H(G.rpause); H(G.rdead); H(G.rsh); H(G.rwind);
    H(G.rrev); H(G.rstk); H(G.rdir); H(G.rside); H(G.rwx); H(G.rwy); }
  if (HASH_MASK & 8) {
    for (i = 0; i < 3; i++) { H(G.pr[i].kind); H(G.pr[i].x); H(G.pr[i].y); H(G.pr[i].life); }
    H(G.bolt.life); H(G.bolt.x); H(G.bolt.y);
    for (i = 0; i < 2; i++) { H(G.fire[i].life); H(G.fire[i].x); H(G.fire[i].y); }
  }
#undef H
  return h;
}
