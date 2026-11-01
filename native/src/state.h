/* state.h - js: GAME STATE. S, P, R, K and the projectile lists in one struct. */
#ifndef STATE_H
#define STATE_H
#include <stdint.h>
#include "config.h"

typedef enum { MODE_TITLE, MODE_INTRO, MODE_PLAY, MODE_LOST, MODE_CLEAR, MODE_OVER, MODE_WIN } Mode;

typedef struct { double x, y, vx, vy, life, dmg, sl; int dead; } Bullet;      /* js: bul */
typedef struct { double x, y, vx, vy, life; int dead; } Bolt;                  /* js: rb */
typedef struct { double sx, sy, tx, ty, t; int done; } Molotov;                /* js: mols */
typedef struct { double x, y, life, max, r; } Fire;                            /* js: fires */
typedef struct {                                                               /* js: parts */
  double x, y, vx, vy, life, max, size;
  uint8_t cr, cg, cb; /* colour string "r,g,b" parsed at creation */
  int add;
} Particle;

typedef struct { int valid; long score, time, kills; } Best; /* js: best */

typedef struct {
  Mode mode;
  double mt;
  double score;
  int lives, level;
  double time, lt;
  int kills, w;
  int ammo[3];
  double cool[6];
  char toast[96];
  double tt, shake;
} State; /* js: S */

typedef struct { double x, y, hp, face, aim, cd, inv, flash; } Player; /* js: P */

typedef struct {                                                       /* js: R */
  double x, y, hp, max, sp, face, slow, cd, pause;
  double dir;
  int dead;
  double sh, wind, ck, rev, side, stk;
} Robot;

/* js: K - keys currently held */
typedef struct {
  int ArrowUp, ArrowDown, ArrowLeft, ArrowRight, KeyW, KeyA, KeyS, KeyD, Space;
} Keys;

typedef struct Game {
  State S;
  Player P;
  Robot R;
  Keys K;
  Best best;
  double T;           /* js: T, running time */
  double engV, engF;  /* js: engV, engF (consumed by the engine hum) */
  uint32_t rng;       /* js: Math.random */
  Bullet bul[CAP_BUL];     int nbul;
  Bolt rb[CAP_RB];         int nrb;
  Molotov mols[CAP_MOLS];  int nmols;
  Fire fires[CAP_FIRES];   int nfires;
  Particle parts[CAP_PARTS]; int nparts;
  /* statistics for pool sizing (only ever grow) */
  int max_bul, max_rb, max_mols, max_fires, max_parts;
} Game;

#endif
