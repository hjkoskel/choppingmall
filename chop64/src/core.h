/* core.h - portable game core for the Commodore 64 port: all game rules, no hardware, no floats.
 * World: 320x200 pixel playfield, regular hexagon (vertices left/right) centred at (160,100),
 * outer apothem AO = 96 px, inner shop block AI = 48 px (the original's 300 / 150, scaled by .32).
 * Positions are in 1/16 pixel (FP = 4). One tick = one 50 Hz frame. */
#ifndef CORE_H
#define CORE_H
#include "types.h"

#define FP 4
#define CX 160
#define CY 100
#define AO 96
#define AI 48
#define NOB 6
#define OBH 6 /* obstacle half size, px */
#define WALK_X0 54   /* walkability bitmap: columns WALK_X0 .. WALK_X0 + WALK_W - 1, 200 rows */
#define WALK_W 216

enum { M_TITLE, M_INTRO, M_PLAY, M_LOST, M_CLEAR, M_OVER, M_WIN };
enum { IN_UP = 1, IN_DOWN = 2, IN_LEFT = 4, IN_RIGHT = 8, IN_FIRE = 16 };
enum { EV_START = 1, EV_RESTART = 2, EV_W1 = 4, EV_W2 = 8, EV_W3 = 16 };
enum { PK_NONE, PK_BULLET, PK_PELLET, PK_MOLOTOV };
enum { WP_PISTOL, WP_SHOTGUN, WP_MOLOTOV };

/* weapon availability: level 1 pistol only, level 2 + shotgun, level 3 + molotov. Shop kinds 0..2 restock weapon 0..2, kind 3 is first aid (always open). */
#define W_AVAIL(w) ((w) < G.level)
#define shop_open(t) ((t) == 3 || (t) < G.level)

typedef struct { i16 x, y, vx, vy, tx, ty; u8 life, kind, dmg, slow; } Proj; /* js: bul / mols */
typedef struct { i16 x, y, vx, vy; u8 life; } Bolt;                           /* js: rb (one at a time) */
typedef struct { i16 x, y; u16 life; } Fire;                                  /* js: fires (two at a time) */

typedef struct {
  u8 mode;
  u16 mt;               /* mode timer, frames */
  u32 score;
  u8 lives, level, kills, w;
  u8 ammo[3];
  u16 cool[6];          /* shop door cooldowns, frames */
  const char *toast;    /* at most 5 characters */
  u8 tt;
  u16 tsec; u8 tfr;     /* run time */
  u16 lt;               /* level time, frames */
  u8 tick;              /* free running frame counter */
  /* player */
  i16 px, py, php;      /* position 1/16 px; health in tenths (1000 = full) */
  u8 pface, paim, pcd, pinv, pflash;
  /* robot */
  i16 rx, ry, rhp, rmax;
  u8 rface, rslow, rcd, rpause, rdead, rsh, rwind, rstk;
  u16 rrev;             /* reverse distance left, 1/16 px */
  i8 rdir, rside;
  u8 los, blocked;      /* cached line-of-sight tests */
  i16 rwx, rwy;         /* cached steering waypoint, px */
  u8 rbase;             /* cached steering direction */
  u8 rang, paimang;     /* cached angles: robot to player, player aim */
  /* projectiles */
  Proj pr[3];
  Bolt bolt;
  Fire fire[2];
} Game;

extern Game G;
extern i16 obx[NOB], oby[NOB];     /* obstacle centres, px */
extern i16 doorx[6], doory[6];     /* shop door positions, px */
extern const u8 shop_kind[6];      /* 0 pistol ammo, 1 shells, 2 gas, 3 first aid */

void game_init(u16 seed);
void game_tick(u8 input, u8 events);
/* helpers also used by the bot / platform code */
u8 ang32(i16 dx, i16 dy);
u8 valid(i16 x16, i16 y16, u8 r);
void core_set_walkmap(const u8 *map); /* enable the precomputed valid(r = 5) bitmap (26 bytes per row) */
u16 game_hash(void); /* 16-bit hash of the whole state (parity tests) */
u16 mdot(i16 dx, i16 dy); /* hexagon distance from the centre (apothem), px */
u8 clear_shot(i16 x1, i16 y1, i16 x2, i16 y2); /* px coordinates */
u8 nav_target(i16 sx, i16 sy, i16 tx, i16 ty, i8 *dir, i16 *wx, i16 *wy); /* all px; returns 1 if it detours */
#endif
