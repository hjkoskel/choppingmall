/* weapons.c - js: shoot and the bullet, molotov and fire loops of updateGame. */
#include <math.h>
#include <stdio.h>
#include "game.h"
#include "jsmath.h"
#include "sound/sound.h"

/* js: shoot */
void shoot(Game *g) {
  State *S = &g->S;
  Player *P = &g->P;
  Robot *R = &g->R;
  int w = S->w;
  double fx = cos(P->face), fy = sin(P->face);
  double mx = P->x + fx * 20, my = P->y + fy * 20;
  if (S->ammo[w] <= 0) {
    char buf[96];
    P->cd = 0.35;
    snd_sfx(120, 0.06, SND_SQUARE, 0.04, 120);
    snprintf(buf, sizeof buf, "OUT OF %s \xe2\x80\x94 visit a shop!", WN[w]);
    toast(g, buf);
    return;
  }
  S->ammo[w]--;
  P->flash = 0.06;
  if (w == 0) {
    if (g->nbul < CAP_BUL) {
      Bullet *b = &g->bul[g->nbul++];
      b->x = mx; b->y = my; b->vx = fx * 950; b->vy = fy * 950;
      b->life = 0.85; b->dmg = 3; b->sl = 0.12; b->dead = 0;
    }
    P->cd = 0.32;
    snd_sfx(900, 0.1, SND_SQUARE, 0.07, 200);
  } else if (w == 1) {
    for (int i = 0; i < 6; i++) {
      double a = P->face + (game_random(g) - 0.5) * 0.4;
      double s = 780 + game_random(g) * 120;
      if (g->nbul < CAP_BUL) {
        Bullet *b = &g->bul[g->nbul++];
        b->x = mx; b->y = my; b->vx = cos(a) * s; b->vy = sin(a) * s;
        b->life = 0.34; b->dmg = 3.2; b->sl = 0.35; b->dead = 0;
      }
    }
    P->cd = 0.85;
    snd_sfx(160, 0.25, SND_SAWTOOTH, 0.1, 40);
    burst(g, mx, my, 8, 255, 200, 90, 140, 1);
  } else {
    double dx = R->x - P->x, dy = R->y - P->y;
    double d = js_max(90, js_min(230, js_hypot(dx, dy)));
    double tx = P->x + fx * d, ty = P->y + fy * d, q = d;
    while (!valid(tx, ty, 2) && q > 30) {
      q -= 12;
      tx = P->x + fx * q;
      ty = P->y + fy * q;
    }
    if (g->nmols < CAP_MOLS) {
      Molotov *m = &g->mols[g->nmols++];
      m->sx = mx; m->sy = my; m->tx = tx; m->ty = ty; m->t = 0; m->done = 0;
    }
    P->cd = 1.3;
    snd_sfx(400, 0.2, SND_TRIANGLE, 0.05, 700);
  }
  if (g->nbul > g->max_bul) g->max_bul = g->nbul;
  if (g->nmols > g->max_mols) g->max_mols = g->nmols;
}

/* js: "molotovs in flight" */
void molotovs_step(Game *g, double dt) {
  for (int i = 0; i < g->nmols; i++) {
    Molotov *m = &g->mols[i];
    m->t += dt / 0.55;
    if (m->t >= 1) {
      m->done = 1;
      if (g->nfires < CAP_FIRES) {
        Fire *f = &g->fires[g->nfires++];
        f->x = m->tx; f->y = m->ty; f->life = 7; f->max = 7; f->r = 38;
      }
      if (g->nfires > g->max_fires) g->max_fires = g->nfires;
      burst(g, m->tx, m->ty, 16, 255, 150, 40, 160, 1);
      snd_sfx(200, 0.4, SND_SAWTOOTH, 0.08, 50);
    }
  }
  int n = 0; /* mols = mols.filter((m) => !m.done) */
  for (int i = 0; i < g->nmols; i++)
    if (!g->mols[i].done) g->mols[n++] = g->mols[i];
  g->nmols = n;
}

/* js: "bullets". kill_robot() empties the bullet list while this loop is still
 * walking the old array (JS reassigns `bul = []`), so iterate a snapshot count:
 * the remaining old bullets are still processed (they still burst particles,
 * consuming random numbers) but the final filter then runs on the new, empty list. */
void bullets_step(Game *g, double dt) {
  Robot *R = &g->R;
  int n0 = g->nbul;
  for (int i = 0; i < n0; i++) {
    Bullet *b = &g->bul[i];
    for (int s = 0; s < 3 && !b->dead; s++) {
      b->x += (b->vx * dt) / 3;
      b->y += (b->vy * dt) / 3;
      if (!R->dead && js_hypot(b->x - R->x, b->y - R->y) < 17) {
        b->dead = 1;
        R->hp -= b->dmg;
        R->slow = js_max(R->slow, b->sl);
        g->S.score += 10;
        burst(g, b->x, b->y, 4, 255, 230, 120, 90, 1);
        snd_sfx(300, 0.05, SND_SQUARE, 0.03, 300);
        if (R->hp <= 0) kill_robot(g);
        break;
      }
      double m = max_dot(b->x, b->y);
      if (m > MALL_AO || m < MALL_AI) {
        b->dead = 1;
        burst(g, b->x, b->y, 3, 200, 200, 200, 60, 0);
        break;
      }
      for (int j = 0; j < OB_COUNT; j++)
        if (js_hypot(b->x - OB[j].x, b->y - OB[j].y) < OB[j].r) {
          b->dead = 1;
          burst(g, b->x, b->y, 3, 230, 230, 230, 60, 0);
          break;
        }
    }
    b->life -= dt;
    if (b->life <= 0) b->dead = 1;
  }
  int n = 0; /* bul = bul.filter((b) => !b.dead) -- on the CURRENT list */
  for (int i = 0; i < g->nbul; i++)
    if (!g->bul[i].dead) g->bul[n++] = g->bul[i];
  g->nbul = n;
}

/* js: "fire damage". *killed = 1 when the robot burned to death (updateGame returns). */
void fires_damage(Game *g, double dt, int *killed) {
  Player *P = &g->P;
  Robot *R = &g->R;
  *killed = 0;
  for (int i = 0; i < g->nfires; i++) {
    Fire *f = &g->fires[i];
    if (js_hypot(P->x - f->x, P->y - f->y) < f->r) {
      P->hp -= 14 * dt;
      if (game_random(g) < 0.2) snd_sfx(90, 0.08, SND_SAWTOOTH, 0.02, 90);
    }
    if (!R->dead && js_hypot(R->x - f->x, R->y - f->y) < f->r + 8) {
      R->hp -= 12 * dt;
      if (R->hp <= 0) {
        kill_robot(g);
        *killed = 1;
        return;
      }
    }
  }
}
