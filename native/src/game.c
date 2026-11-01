/* game.c - js: particles, level flow, input edge handling, update, updateGame, player, shops. */
#include "game.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "jsmath.h"
#include "rng.h"
#include "sound/sound.h"

double game_random(Game *g) { return rng_next(&g->rng); }

void game_init(Game *g, uint32_t seed) {
  memset(g, 0, sizeof *g);
  world_init();
  g->rng = seed;
  g->S.mode = MODE_TITLE;
  g->S.lives = 3;
  g->S.level = 1;
  g->P.hp = 100;
  g->R.hp = 30;
  g->R.max = 30;
  g->R.sp = 120;
  g->R.dir = 1;
  g->R.sh = 2;
  g->R.side = 1;
  g->engF = 50;
  storage_load_best(g);
}

/* js: addP */
void add_p(Game *g, double x, double y, double vx, double vy, double life,
           int cr, int cg, int cb, double size, int add) {
  if (g->nparts >= CAP_PARTS) return; /* pool full: drop */
  Particle *p = &g->parts[g->nparts++];
  p->x = x; p->y = y; p->vx = vx; p->vy = vy;
  p->life = life; p->max = life; p->size = size; p->add = add;
  p->cr = (uint8_t)cr; p->cg = (uint8_t)cg; p->cb = (uint8_t)cb;
  if (g->nparts > g->max_parts) g->max_parts = g->nparts;
}

/* js: burst (sp default 120 is always passed explicitly by callers) */
void burst(Game *g, double x, double y, int n, int cr, int cg, int cb, double sp, int add) {
  for (int i = 0; i < n; i++) {
    double a = game_random(g) * TAU;
    double s = game_random(g) * sp;
    double vx = cos(a) * s, vy = sin(a) * s;
    double life = 0.3 + game_random(g) * 0.5;
    double size = 2 + game_random(g) * 3;
    add_p(g, x, y, vx, vy, life, cr, cg, cb, size, add);
  }
}

/* js: toast */
void toast(Game *g, const char *s) {
  snprintf(g->S.toast, sizeof g->S.toast, "%s", s);
  g->S.tt = 1.8;
}

/* js: spawn */
static void spawn(Game *g) {
  polar(5, 225, 0, &g->P.x, &g->P.y);
  polar(2, 225, 0, &g->R.x, &g->R.y);
  g->P.face = PI_D * 0.9;
  g->R.face = 0;
  g->P.inv = 2.5;
  g->P.aim = 0;
  g->R.pause = 1.6;
  g->R.rev = 0;
  g->R.stk = 0;
}

/* js: startLevel */
static void start_level(Game *g) {
  int l = g->S.level;
  g->P.hp = 100;
  g->R.max = g->R.hp = CFG.robotHp[l - 1];
  g->R.sp = CFG.robotSpeed[l - 1];
  g->R.dead = 0;
  g->R.slow = 0;
  g->R.cd = 0;
  g->nbul = 0;
  g->nfires = 0;
  g->nparts = 0;
  g->nmols = 0;
  g->nrb = 0;
  g->R.sh = 1.8;
  g->R.wind = 0;
  for (int k = 0; k < 6; k++) g->S.cool[k] = 0;
  g->S.lt = 0;
  spawn(g);
  g->S.mode = MODE_INTRO;
  g->S.mt = 2.4;
}

/* js: newGame */
void new_game(Game *g) {
  snd_new_game(); /* startMusic(); jingleStop(); if (mus.paused) mlResume(); */
  g->S.score = 0;
  g->S.lives = 3;
  g->S.level = 1;
  g->S.time = 0;
  g->S.kills = 0;
  g->S.w = 0;
  g->S.ammo[0] = g->S.ammo[1] = g->S.ammo[2] = 0;
  start_level(g);
}

/* js: keydown handler (the part after K[e.code] = 1) */
void game_key_down(Game *g, int key) {
  if (key == GK_M) snd_toggle_mute();
  if (key == GK_R) new_game(g);
  else if ((key == GK_ENTER || key == GK_SPACE) && g->S.mode == MODE_TITLE) new_game(g);
  if (key == GK_DIGIT1) g->S.w = 0;
  else if (key == GK_DIGIT2) g->S.w = 1;
  else if (key == GK_DIGIT3) g->S.w = 2;
}

/* js: blur handler */
void game_clear_keys(Game *g) { memset(&g->K, 0, sizeof g->K); }

/* js: move */
void move_entity(double *ex, double *ey, double dx, double dy, double r) {
  if (valid(*ex + dx, *ey, r)) *ex += dx;
  if (valid(*ex, *ey + dy, r)) *ey += dy;
}

/* js: angTo - turns angle a toward b by at most rate radians */
double ang_to(double a, double b, double rate) {
  double d = b - a;
  while (d > PI_D) d -= TAU;
  while (d < -PI_D) d += TAU;
  return fabs(d) < rate ? b : a + js_sign(d) * rate;
}

/* js: hurtPlayer */
void hurt_player(Game *g, double n) {
  g->P.hp -= n;
  g->S.shake = 0.35;
  snd_sfx(140, 0.2, SND_SAWTOOTH, 0.1, 60);
}

/* js: killRobot. NB: JS reassigns bul/rb to new arrays. A bullet loop that is
 * running keeps iterating the OLD array; weapons.c handles that by iterating a
 * snapshot count. */
void kill_robot(Game *g) {
  g->R.dead = 1;
  g->S.kills++;
  g->S.score += 1000 * g->S.level + js_max(0, floor(90 - g->S.lt)) * 10;
  for (int i = 0; i < 5; i++) {
    if (i % 2) burst(g, g->R.x, g->R.y, 18, 255, 160, 40, 260, 1);
    else burst(g, g->R.x, g->R.y, 18, 200, 200, 200, 260, 1);
  }
  burst(g, g->R.x, g->R.y, 25, 90, 96, 108, 200, 0);
  snd_sfx(90, 0.8, SND_SAWTOOTH, 0.14, 25);
  g->S.shake = 0.6;
  snd_play_jingle(JINGLE_WIN);
  g->S.mode = MODE_CLEAR;
  g->S.mt = 2.6;
  g->nbul = 0;
  g->nrb = 0;
  if (g->S.level == 3) save_best(g);
}

/* js: update */
void update(Game *g, double dt) {
  g->engV = 0;
  /* js: updateGame - written as one function with the same early returns */
  update_game(g, dt);
  snd_engine(g->engV, g->engF); /* js: engineApply */
}

/* js: updateGame */
void update_game(Game *g, double dt) {
  State *S = &g->S;
  Player *P = &g->P;
  Robot *R = &g->R;
  Keys *K = &g->K;

  g->T += dt;
  S->shake = js_max(0, S->shake - dt);
  S->tt -= dt;
  /* particles, bullets, fires always live */
  for (int i = 0; i < g->nparts; i++) {
    Particle *p = &g->parts[i];
    p->x += p->vx * dt;
    p->y += p->vy * dt;
    p->life -= dt;
  }
  { /* parts = parts.filter((p) => p.life > 0) */
    int n = 0;
    for (int i = 0; i < g->nparts; i++)
      if (g->parts[i].life > 0) g->parts[n++] = g->parts[i];
    g->nparts = n;
  }
  for (int i = 0; i < g->nfires; i++) {
    Fire *f = &g->fires[i];
    f->life -= dt;
    if (game_random(g) < 0.9) {
      double x = f->x + (game_random(g) - 0.5) * f->r * 1.6;
      double y = f->y + (game_random(g) - 0.5) * f->r * 1.2;
      double vx = (game_random(g) - 0.5) * 20;
      double vy = -30 - game_random(g) * 40;
      double life = 0.5 + game_random(g) * 0.4;
      int c1 = game_random(g) < 0.5; /* '255,140,30' : '255,70,20' */
      double size = 4 + game_random(g) * 5;
      add_p(g, x, y, vx, vy, life, 255, c1 ? 140 : 70, c1 ? 30 : 20, size, 1);
    }
  }
  { /* fires = fires.filter((f) => f.life > 0) */
    int n = 0;
    for (int i = 0; i < g->nfires; i++)
      if (g->fires[i].life > 0) g->fires[n++] = g->fires[i];
    g->nfires = n;
  }
  if (S->mode == MODE_TITLE) return;
  if (S->mode == MODE_OVER || S->mode == MODE_WIN) S->mt -= dt;
  if (S->mode == MODE_INTRO) {
    S->mt -= dt;
    if (S->mt <= 0) S->mode = MODE_PLAY;
    return;
  }
  if (S->mode == MODE_LOST) {
    S->mt -= dt;
    if (S->mt <= 0) {
      spawn(g);
      P->hp = 100;
      g->nfires = 0;
      g->nmols = 0;
      g->nbul = 0;
      g->nrb = 0;
      R->wind = 0;
      R->sh = 1.8;
      S->mode = MODE_PLAY;
    }
    return;
  }
  if (S->mode == MODE_CLEAR) {
    S->mt -= dt;
    if (S->mt <= 0) {
      if (S->level >= 3) {
        S->mode = MODE_WIN;
        S->mt = 1.2;
      } else {
        S->level++;
        start_level(g);
      }
    }
    return;
  }
  if (S->mode != MODE_PLAY) return;
  S->time += dt;
  S->lt += dt;
  S->score += dt * 5;
  P->cd -= dt;
  P->inv -= dt;
  P->flash -= dt;
  /* player */
  int fire = K->Space;
  double mx = ((K->ArrowRight || K->KeyD) ? 1 : 0) - ((K->ArrowLeft || K->KeyA) ? 1 : 0);
  double my = ((K->ArrowDown || K->KeyS) ? 1 : 0) - ((K->ArrowUp || K->KeyW) ? 1 : 0);
  /* (touch joystick not ported: joy is always 0) */
  if (fire) {
    P->aim += dt;
    P->face = ang_to(P->face, atan2(R->y - P->y, R->x - P->x), 9 * dt);
    if (P->aim > CFG.aimDelay && P->cd <= 0) shoot(g);
  } else {
    P->aim = 0;
    if (mx != 0 || my != 0) {
      double l = js_hypot(mx, my);
      P->face = atan2(my, mx);
      move_entity(&P->x, &P->y, (mx / l) * CFG.playerSpeed * dt, (my / l) * CFG.playerSpeed * dt, 11);
    }
  }
  /* shops */
  for (int k = 0; k < 6; k++) {
    S->cool[k] -= dt;
    if (S->cool[k] > 0) continue;
    double x, y;
    polar(k, MALL_AI + 26, 0, &x, &y);
    if (js_hypot(P->x - x, P->y - y) > 42) continue;
    int t = SH[k].k;
    int ok = 0;
    char msg[64] = "";
    if (t == 3) {
      if (P->hp < 100) {
        P->hp = js_min(100, P->hp + CFG.medkit);
        ok = 1;
        snprintf(msg, sizeof msg, "+%d HEALTH", CFG.medkit);
      }
    } else if (S->ammo[t] < MAXA[t]) {
      int a = CFG.shopRestock[t];
      S->ammo[t] = (int)js_min(MAXA[t], S->ammo[t] + a);
      ok = 1;
      static const char *const nm[3] = {"PISTOL AMMO", "SHELLS", "GAS CANISTERS"};
      snprintf(msg, sizeof msg, "+%d %s", a, nm[t]);
    }
    if (ok) {
      S->cool[k] = CFG.shopCooldown;
      S->score += 50;
      toast(g, msg);
      snd_sfx(600, 0.12, SND_TRIANGLE, 0.07, 1000);
      burst(g, x, y, 10, 255, 230, 120, 80, 1);
    }
  }
  molotovs_step(g, dt);
  bullets_step(g, dt);
  if (S->mode != MODE_PLAY) return;
  bolts_step(g, dt);
  int killed = 0;
  fires_damage(g, dt, &killed);
  if (killed) return;
  robot_step(g, dt);
}

/* js: the state changes draw() makes before/while drawing:
 *  - two Math.random() calls for the screen-shake offset (only when shaking),
 *  - one flame particle per molotov in flight.
 * Done here so rendering stays free of game-state mutation. Call once per frame
 * after update() and before drawing. */
void game_pre_draw(Game *g, double *shake_x, double *shake_y) {
  *shake_x = 0;
  *shake_y = 0;
  if (g->S.shake > 0) {
    *shake_x = (game_random(g) - 0.5) * 10 * g->S.shake;
    *shake_y = (game_random(g) - 0.5) * 10 * g->S.shake;
  }
  for (int i = 0; i < g->nmols; i++) {
    const Molotov *m = &g->mols[i];
    double t = m->t;
    double x = m->sx + (m->tx - m->sx) * t;
    double y = m->sy + (m->ty - m->sy) * t - sin(PI_D * t) * 45;
    add_p(g, x, y, 0, 0, 0.25, 255, 160, 40, 3, 1);
  }
}
