/* robot.c - js: robotTarget clearShot, robot bolts, and the robot part of updateGame. */
#include <math.h>
#include "game.h"
#include "jsmath.h"
#include "sound/sound.h"

/* js: robotTarget - the point the robot steers toward */
static void robot_target(Game *g, double *ox, double *oy) {
  Player *P = &g->P;
  Robot *R = &g->R;
  int blocked = 0;
  for (int i = 1; i <= 12; i++) {
    if (max_dot(R->x + ((P->x - R->x) * i) / 12, R->y + ((P->y - R->y) * i) / 12) < MALL_AI + 24) {
      blocked = 1;
      break;
    }
  }
  if (!blocked) { *ox = P->x; *oy = P->y; return; }
  double ar = atan2(R->y - MALL_CY, R->x - MALL_CX), ap = atan2(P->y - MALL_CY, P->x - MALL_CX);
  double d = ap - ar;
  while (d > PI_D) d -= TAU;
  while (d < -PI_D) d += TAU;
  if (fabs(d) < 2.7) {
    double sg = js_sign(d);
    R->dir = (sg != 0) ? sg : 1; /* js: Math.sign(d) || 1 */
  }
  double a = ar + R->dir * 0.5, ux = cos(a), uy = sin(a);
  double m = -9;
  for (int i = 0; i < 6; i++) m = js_max(m, ux * N[i].x + uy * N[i].y);
  *ox = MALL_CX + (ux * 225) / m;
  *oy = MALL_CY + (uy * 225) / m;
}

/* js: clearShot */
int clear_shot(double x1, double y1, double x2, double y2) {
  for (int i = 1; i < 16; i++) {
    double x = x1 + ((x2 - x1) * i) / 16, y = y1 + ((y2 - y1) * i) / 16;
    if (max_dot(x, y) < MALL_AI + 2) return 0;
    for (int j = 0; j < OB_COUNT; j++)
      if (js_hypot(x - OB[j].x, y - OB[j].y) < OB[j].r) return 0;
  }
  return 1;
}

/* js: "robot bolts" */
void bolts_step(Game *g, double dt) {
  Player *P = &g->P;
  for (int i = 0; i < g->nrb; i++) {
    Bolt *b = &g->rb[i];
    b->x += b->vx * dt;
    b->y += b->vy * dt;
    b->life -= dt;
    if (b->life <= 0) b->dead = 1;
    if (P->inv <= 0 && js_hypot(b->x - P->x, b->y - P->y) < 15) {
      b->dead = 1;
      hurt_player(g, 12);
      P->inv = 0.5;
      burst(g, P->x, P->y, 10, 255, 90, 60, 150, 1);
      continue;
    }
    double m = max_dot(b->x, b->y);
    if (m > MALL_AO || m < MALL_AI) {
      b->dead = 1;
      burst(g, b->x, b->y, 5, 255, 120, 80, 90, 1);
      continue;
    }
    for (int j = 0; j < OB_COUNT; j++)
      if (js_hypot(b->x - OB[j].x, b->y - OB[j].y) < OB[j].r) {
        b->dead = 1;
        burst(g, b->x, b->y, 5, 255, 120, 80, 90, 1);
        break;
      }
  }
  int n = 0; /* rb = rb.filter((b) => !b.dead) */
  for (int i = 0; i < g->nrb; i++)
    if (!g->rb[i].dead) g->rb[n++] = g->rb[i];
  g->nrb = n;
}

/* js: "// robot" section through the end of updateGame */
void robot_step(Game *g, double dt) {
  State *S = &g->S;
  Player *P = &g->P;
  Robot *R = &g->R;

  R->slow -= dt;
  R->cd -= dt;
  R->sh -= dt;
  if (R->wind > 0) {
    R->wind -= dt;
    R->face = ang_to(R->face, atan2(P->y - R->y, P->x - R->x), 12 * dt);
    if (R->wind <= 0) {
      double a = R->face;
      if (g->nrb < CAP_RB) {
        Bolt *b = &g->rb[g->nrb++];
        b->x = R->x + cos(a) * 24;
        b->y = R->y + sin(a) * 24;
        b->vx = cos(a) * 340;
        b->vy = sin(a) * 340;
        b->life = 2.2;
        b->dead = 0;
        if (g->nrb > g->max_rb) g->max_rb = g->nrb;
      }
      snd_sfx(700, 0.18, SND_SAWTOOTH, 0.06, 150);
    }
  } else if (R->sh <= 0 && R->pause <= 0 && js_hypot(P->x - R->x, P->y - R->y) < 520 &&
             clear_shot(R->x, R->y, P->x, P->y)) {
    R->wind = 0.4;
    R->sh = CFG.robotShotInterval[S->level - 1];
    snd_sfx(300, 0.4, SND_TRIANGLE, 0.04, 900);
  }
  double sp = R->sp * (R->slow > 0 ? 0.55 : 1);
  if (R->pause > 0) {
    R->pause -= dt;
    sp = 0;
  }
  if (R->wind > 0) sp = 0;
  if (sp > 0) {
    if (R->rev > 0) {
      double st = js_min(R->rev, sp * 0.8 * dt);
      double nx = R->x - cos(R->face) * st, ny = R->y - sin(R->face) * st;
      if (valid(nx, ny, 15)) {
        R->x = nx;
        R->y = ny;
        R->rev -= st;
      } else R->rev = 0;
      if (R->rev <= 0.5) {
        R->rev = 0;
        R->side = -R->side;
        R->stk = 0;
      }
    } else {
      double tx, ty;
      robot_target(g, &tx, &ty);
      double dx = tx - R->x, dy = ty - R->y;
      double dl = js_hypot(dx, dy);
      if (dl == 0) dl = 1; /* js: || 1 */
      dx /= dl;
      dy /= dl;
      double tdx = dx, tdy = dy;
      const Normal *bn = &N[0];
      double bm = -1e9;
      for (int i = 0; i < 6; i++) {
        double d = (R->x - MALL_CX) * N[i].x + (R->y - MALL_CY) * N[i].y;
        if (d > bm) { bm = d; bn = &N[i]; }
      }
      double cw = (dl > 220 ? 0.8 : 0.25) * js_max(-1, js_min(1, (225 - bm) / 60));
      dx += bn->x * cw;
      dy += bn->y * cw;
      double base = atan2(dy, dx), ox = R->x, oy = R->y;
      static const double offA[7] = {0, 0.5, -0.5, 1, -1, 1.6, -1.6};
      static const double offB[7] = {0, -0.5, 0.5, -1, 1, -1.6, 1.6};
      const double *offs = R->side > 0 ? offA : offB;
      for (int i = 0; i < 7; i++) {
        double a = base + offs[i];
        double nx = R->x + cos(a) * sp * dt, ny = R->y + sin(a) * sp * dt;
        if (valid(nx, ny, 15)) {
          R->x = nx;
          R->y = ny;
          R->face = ang_to(R->face, a, 10 * dt);
          break;
        }
      }
      double prog = ((R->x - ox) * tdx + (R->y - oy) * tdy) / (sp * dt);
      if (prog < 0.25) R->stk += dt;
      else R->stk = js_max(0, R->stk - dt * 2);
      if (R->stk > 0.35) {
        R->rev = 96;
        R->stk = 0;
        snd_sfx(820, 0.12, SND_SQUARE, 0.04, 820);
      }
    }
  } else R->face = ang_to(R->face, atan2(P->y - R->y, P->x - R->x), 6 * dt);
  {
    double dd = js_hypot(P->x - R->x, P->y - R->y), prox = js_max(0, 1 - dd / 650);
    if (sp > 0) {
      g->engV = 0.05 + 0.11 * prox;
      g->engF = 44 + sp * 0.2;
    } else g->engV = 0.025;
    R->ck -= dt;
    if (dd < 700 && R->ck <= 0 && clear_shot(R->x, R->y, P->x, P->y)) {
      R->ck = 1.8;
      snd_clank(0.4 + 0.6 * prox);
    }
  }
  if (js_hypot(P->x - R->x, P->y - R->y) < 28 && P->inv <= 0 && R->cd <= 0) {
    hurt_player(g, 22);
    P->inv = 1;
    R->cd = 0.9;
    R->pause = 0.45;
    double a = atan2(P->y - R->y, P->x - R->x);
    for (double d = 34; d > 0; d -= 6)
      if (valid(P->x + cos(a) * d, P->y + sin(a) * d, 11)) {
        P->x += cos(a) * d;
        P->y += sin(a) * d;
        break;
      }
    burst(g, P->x, P->y, 10, 255, 60, 60, 150, 1);
  }
  if (P->hp <= 0) {
    P->hp = 0;
    S->lives--;
    burst(g, P->x, P->y, 25, 255, 60, 60, 220, 1);
    snd_sfx(110, 0.7, SND_SAWTOOTH, 0.12, 30);
    snd_play_jingle(JINGLE_LOSE);
    if (S->lives <= 0) {
      S->mode = MODE_OVER;
      S->mt = 1.2;
      save_best(g);
    } else {
      S->mode = MODE_LOST;
      S->mt = 1.8;
    }
  }
}
