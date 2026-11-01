/* render.c - js: txt, drawPlayer, drawRobot, draw. Draws only through c2d. */
#include <math.h>
#include <stdio.h>
#include "render.h"
#include "jsmath.h"
#include "world.h"

#define FILL(css) c2d_fill_style(c, c2d_color(css))
#define STROKE(css) c2d_stroke_style(c, c2d_color(css))

/* js: txt */
void txt(c2d *c, const char *s, double x, double y, double size, const char *col, c2d_align al, int bold) {
  c2d_font(c, size, bold);
  c2d_text_align(c, al);
  c2d_text_baseline(c, C2D_BASELINE_MIDDLE);
  FILL(col);
  c2d_fill_text(c, s, x, y);
}

/* js: drawPlayer */
static void draw_player(c2d *c, const Game *g) {
  const State *S = &g->S;
  const Player *P = &g->P;
  if (P->inv > 0 && S->mode == MODE_PLAY && ((long)floor(g->T * 14)) % 2 != 0) return;
  c2d_save(c);
  c2d_translate(c, P->x, P->y);
  FILL("rgba(0,0,0,.3)");
  c2d_begin_path(c);
  c2d_ellipse(c, 2, 5, 13, 10, 0, 0, TAU, 0);
  c2d_fill(c);
  c2d_rotate(c, P->face);
  FILL("#1d4f9c");
  c2d_begin_path(c);
  c2d_ellipse(c, 0, 0, 7, 13, 0, 0, TAU, 0);
  c2d_fill(c);
  FILL("#2f6fd1");
  c2d_begin_path(c);
  c2d_ellipse(c, 0, 0, 6, 11, 0, 0, TAU, 0);
  c2d_fill(c);
  FILL("#f0c59a");
  c2d_fill_rect(c, 2, -12, 12, 4);
  c2d_fill_rect(c, 2, 8, 10, 4);
  if (S->w == 0) {
    FILL("#22262b");
    c2d_fill_rect(c, 10, -2, 12, 4);
    FILL("#454c55");
    c2d_fill_rect(c, 10, -2, 8, 2);
  } else if (S->w == 1) {
    FILL("#4a3524");
    c2d_fill_rect(c, 8, -2, 10, 5);
    FILL("#22262b");
    c2d_fill_rect(c, 16, -2, 15, 4);
    c2d_fill_rect(c, 16, 2, 15, 2);
  } else {
    FILL("#3fae6a");
    c2d_begin_path(c);
    c2d_arc(c, 15, 0, 5, 0, TAU, 0);
    c2d_fill(c);
    FILL("#fff");
    c2d_fill_rect(c, 14, -2, 6, 2);
  }
  FILL("#f0c59a");
  c2d_begin_path(c);
  c2d_arc(c, 0, 0, 7, 0, TAU, 0);
  c2d_fill(c);
  FILL("#3a2a1e");
  c2d_begin_path(c);
  c2d_arc(c, -1.5, 0, 6.5, PI_D * 0.4, PI_D * 1.6, 0);
  c2d_fill(c);
  if (P->flash > 0) {
    FILL("#ffe9a0");
    c2d_shadow(c, "#ffb030", 16);
    c2d_begin_path(c);
    c2d_arc(c, S->w == 1 ? 34 : 25, 0, 7, 0, TAU, 0);
    c2d_fill(c);
    c2d_shadow(c, NULL, 0);
  }
  c2d_restore(c);
  if (S->mode == MODE_PLAY && g->K.Space) {
    double p = js_min(1, P->aim / CFG.aimDelay);
    STROKE(p < 1 ? "rgba(255,220,80,.9)" : "rgba(255,60,60,.95)");
    c2d_line_width(c, 3);
    c2d_begin_path(c);
    c2d_arc(c, P->x, P->y, 19, -PI_D / 2, -PI_D / 2 + TAU * p, 0);
    c2d_stroke(c);
    { static const double dash[2] = {5, 7}; c2d_line_dash(c, dash, 2); }
    STROKE("rgba(255,60,60,.5)");
    c2d_line_width(c, 1.5);
    c2d_begin_path(c);
    c2d_move_to(c, P->x, P->y);
    double len = S->w == 2 ? 230 : S->w == 1 ? 260 : 520;
    c2d_line_to(c, P->x + cos(P->face) * len, P->y + sin(P->face) * len);
    c2d_stroke(c);
    c2d_line_dash(c, NULL, 0);
  }
}

/* js: drawRobot */
static void draw_robot(c2d *c, const Game *g) {
  const Robot *R = &g->R;
  if (R->dead) return;
  c2d_save(c);
  c2d_translate(c, R->x, R->y);
  FILL("rgba(0,0,0,.3)");
  c2d_begin_path(c);
  c2d_ellipse(c, 2, 6, 20, 15, 0, 0, TAU, 0);
  c2d_fill(c);
  c2d_rotate(c, R->face);
  FILL("#22262d");
  c2d_fill_rect(c, -15, -19, 30, 8);
  c2d_fill_rect(c, -15, 11, 30, 8);
  FILL("#3a414b");
  for (int i = -12; i < 14; i += 6) {
    c2d_fill_rect(c, i, -19, 2, 8);
    c2d_fill_rect(c, i, 11, 2, 8);
  }
  FILL("#5d6672");
  c2d_fill_rect(c, 4, -16, 18, 5);
  c2d_fill_rect(c, 4, 11, 18, 5);
  FILL("#c0c8d4");
  c2d_fill_rect(c, 20, -17, 5, 3);
  c2d_fill_rect(c, 20, 14, 5, 3);
  FILL("#8a94a3");
  STROKE("#2b3038");
  c2d_line_width(c, 2);
  c2d_begin_path(c);
  for (int i = 0; i < 6; i++) {
    double a = (i * PI_D) / 3;
    c2d_line_to(c, cos(a) * 16, sin(a) * 16);
  }
  c2d_close_path(c);
  c2d_fill(c);
  c2d_stroke(c);
  FILL("#a9b3c1");
  c2d_begin_path(c);
  c2d_arc(c, -2, 0, 9, 0, TAU, 0);
  c2d_fill(c);
  double pulse = 0.6 + 0.4 * sin(g->T * 10);
  FILL("#ff2a2a");
  c2d_shadow(c, "#ff0000", 10 + 8 * pulse);
  c2d_fill_rect(c, 6, -4, 7, 8);
  c2d_shadow(c, NULL, 0);
  if (R->wind > 0) {
    double cc = 1 - R->wind / 0.4;
    FILL("rgba(255,120,70,.9)");
    c2d_shadow(c, "#ff3a20", 16);
    c2d_begin_path(c);
    c2d_arc(c, 25, 0, 3 + cc * 7, 0, TAU, 0);
    c2d_fill(c);
    c2d_shadow(c, NULL, 0);
  }
  c2d_restore(c);
  double w = 36, x = R->x - w / 2, y = R->y - 30;
  FILL("rgba(0,0,0,.7)");
  c2d_fill_rect(c, x - 1, y - 1, w + 2, 6);
  FILL("#e23b3b");
  c2d_fill_rect(c, x, y, w * js_max(0, R->hp / R->max), 4);
}

/* js: draw (world part). game_pre_draw() has already applied draw()'s state changes.
 * The caller composites the background first, then W and X (shifted by the shake
 * offset), then H. */
void draw_frame(c2d *c, const Game *g) {
  const State *S = &g->S;
  c2d_clear_layer(c, LAYER_W);
  c2d_clear_layer(c, LAYER_X);
  c2d_clear_layer(c, LAYER_H);
  c2d_set_layer(c, LAYER_W);
  c2d_set_additive_layer(c, LAYER_X);
  c2d_composite(c, C2D_SOURCE_OVER);
  c2d_save(c); /* js: g.save() at the top of draw() */
  /* shop markers */
  for (int k = 0; k < 6; k++) {
    double x, y;
    polar(k, MALL_AI + 26, 0, &x, &y);
    int ready = S->cool[k] <= 0;
    c2d_save(c);
    if (ready) {
      c2d_global_alpha(c, 0.3 + 0.2 * sin(g->T * 4 + k));
      FILL(SH[k].c);
      c2d_begin_path(c);
      c2d_arc(c, x, y, 24, 0, TAU, 0);
      c2d_fill(c);
      c2d_global_alpha(c, 1);
      STROKE(SH[k].c);
      c2d_line_width(c, 2);
      c2d_begin_path(c);
      c2d_arc(c, x, y, 24, 0, TAU, 0);
      c2d_stroke(c);
      txt(c, "$", x, y + 1, 18, "#fff", C2D_ALIGN_CENTER, 1);
    } else {
      STROKE("rgba(80,80,90,.8)");
      c2d_line_width(c, 4);
      c2d_begin_path(c);
      c2d_arc(c, x, y, 20, -PI_D / 2, -PI_D / 2 + TAU * (1 - S->cool[k] / CFG.shopCooldown), 0);
      c2d_stroke(c);
      txt(c, "CLOSED", x, y + 1, 8, "#555", C2D_ALIGN_CENTER, 1);
    }
    c2d_restore(c);
  }
  for (int i = 0; i < g->nfires; i++) {
    const Fire *f = &g->fires[i];
    double a = js_min(1, f->life / 1.5);
    c2d_paint gr = c2d_radial_gradient(f->x, f->y, 2, f->x, f->y, f->r + 6);
    c2d_color_stop(&gr, 0, c2d_rgba(255, 200, 70, 0.75 * a));
    c2d_color_stop(&gr, 0.6, c2d_rgba(255, 100, 20, 0.5 * a));
    c2d_color_stop(&gr, 1, c2d_color("rgba(60,20,10,0)"));
    c2d_fill_style(c, gr);
    c2d_begin_path(c);
    c2d_arc(c, f->x, f->y, f->r + 6, 0, TAU, 0);
    c2d_fill(c);
  }
  for (int i = 0; i < g->nmols; i++) {
    const Molotov *m = &g->mols[i];
    double t = m->t;
    double x = m->sx + (m->tx - m->sx) * t;
    double y = m->sy + (m->ty - m->sy) * t - sin(PI_D * t) * 45;
    FILL("rgba(0,0,0,.25)");
    c2d_begin_path(c);
    c2d_ellipse(c, m->sx + (m->tx - m->sx) * t, m->sy + (m->ty - m->sy) * t, 6, 4, 0, 0, TAU, 0);
    c2d_fill(c);
    c2d_save(c);
    c2d_translate(c, x, y);
    c2d_rotate(c, g->T * 14);
    FILL("#3fae6a");
    c2d_fill_rect(c, -3, -6, 6, 12);
    FILL("#ddd");
    c2d_fill_rect(c, -1, -9, 2, 4);
    c2d_restore(c);
    /* js: addP(...) trail particle -> done in game_pre_draw() */
  }
  draw_robot(c, g);
  draw_player(c, g);
  c2d_composite(c, C2D_LIGHTER);
  for (int i = 0; i < g->nrb; i++) {
    const Bolt *b = &g->rb[i];
    c2d_paint gr = c2d_radial_gradient(b->x, b->y, 0, b->x, b->y, 14);
    c2d_color_stop(&gr, 0, c2d_color("rgba(255,240,200,1)"));
    c2d_color_stop(&gr, 0.35, c2d_color("rgba(255,70,40,.8)"));
    c2d_color_stop(&gr, 1, c2d_color("rgba(255,40,20,0)"));
    c2d_fill_style(c, gr);
    c2d_begin_path(c);
    c2d_arc(c, b->x, b->y, 14, 0, TAU, 0);
    c2d_fill(c);
    STROKE("rgba(255,90,60,.7)");
    c2d_line_width(c, 4);
    c2d_begin_path(c);
    c2d_move_to(c, b->x, b->y);
    c2d_line_to(c, b->x - b->vx * 0.05, b->y - b->vy * 0.05);
    c2d_stroke(c);
  }
  c2d_composite(c, C2D_SOURCE_OVER);
  for (int i = 0; i < g->nbul; i++) {
    const Bullet *b = &g->bul[i];
    STROKE(b->dmg > 3.1 && b->life < 0.4 ? "#ffd27a" : "#fff2b0");
    c2d_line_width(c, 2);
    c2d_begin_path(c);
    c2d_move_to(c, b->x, b->y);
    c2d_line_to(c, b->x - b->vx * 0.02, b->y - b->vy * 0.02);
    c2d_stroke(c);
  }
  for (int i = 0; i < g->nparts; i++) {
    const Particle *p = &g->parts[i];
    c2d_composite(c, p->add ? C2D_LIGHTER : C2D_SOURCE_OVER);
    c2d_fill_style(c, c2d_rgba(p->cr, p->cg, p->cb, js_max(0, p->life / p->max) * (p->add ? 0.8 : 1)));
    c2d_begin_path(c);
    c2d_arc(c, p->x, p->y, p->size * (p->add ? p->life / p->max + 0.3 : 1), 0, TAU, 0);
    c2d_fill(c);
  }
  c2d_composite(c, C2D_SOURCE_OVER);
  c2d_restore(c); /* js: g.restore() */
  c2d_set_layer(c, LAYER_H);
  draw_hud(c, g);
  c2d_set_layer(c, LAYER_W);
}
