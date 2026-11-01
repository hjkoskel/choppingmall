/* render_bg.c - js: STATIC BACKGROUND (addHex hexP rr drawOb buildBG). */
#include <math.h>
#include <stddef.h>
#include "render.h"
#include "world.h"

#define FILL(css) c2d_fill_style(c, c2d_color(css))
#define STROKE(css) c2d_stroke_style(c, c2d_color(css))

/* js: addHex */
void add_hex(c2d *c, double a) {
  double r = a / D30;
  for (int i = 0; i < 6; i++) {
    double t = (i * PI_D) / 3;
    double x = MALL_CX + r * cos(t), y = MALL_CY + r * sin(t);
    if (i) c2d_line_to(c, x, y); else c2d_move_to(c, x, y);
  }
  c2d_close_path(c);
}
/* js: hexP */
void hex_p(c2d *c, double a) {
  c2d_begin_path(c);
  add_hex(c, a);
}
/* js: rr */
void rr(c2d *c, double x, double y, double w, double h, double r) {
  c2d_begin_path(c);
  c2d_move_to(c, x + r, y);
  c2d_arc_to(c, x + w, y, x + w, y + h, r);
  c2d_arc_to(c, x + w, y + h, x, y + h, r);
  c2d_arc_to(c, x, y + h, x, y, r);
  c2d_arc_to(c, x, y, x + w, y, r);
  c2d_close_path(c);
}

/* js: drawOb */
void draw_ob(c2d *c, const Obstacle *o) {
  c2d_save(c);
  c2d_translate(c, o->x, o->y);
  FILL("rgba(0,0,0,.25)");
  c2d_begin_path(c);
  c2d_ellipse(c, 3, 5, o->r + 3, o->r, 0, 0, TAU, 0);
  c2d_fill(c);
  if (o->type == OB_PLANT) {
    FILL("#8a4b2a");
    c2d_begin_path(c); c2d_arc(c, 0, 0, 11, 0, TAU, 0); c2d_fill(c);
    FILL("#5a2f19");
    c2d_begin_path(c); c2d_arc(c, 0, 0, 8, 0, TAU, 0); c2d_fill(c);
    for (int i = 0; i < 8; i++) {
      double a = (i * TAU) / 8;
      FILL(i % 2 ? "#2f8a3c" : "#3fae4f");
      c2d_begin_path(c);
      c2d_ellipse(c, cos(a) * 8, sin(a) * 8, 10, 5, a, 0, TAU, 0);
      c2d_fill(c);
    }
    FILL("#57c86a");
    c2d_begin_path(c); c2d_arc(c, 0, 0, 6, 0, TAU, 0); c2d_fill(c);
  } else if (o->type == OB_BENCH) {
    c2d_rotate(c, o->rot);
    FILL("#3b2a1c");
    c2d_fill_rect(c, -20, -9, 5, 18);
    c2d_fill_rect(c, 15, -9, 5, 18);
    FILL("#8a5a34");
    c2d_fill_rect(c, -22, -8, 44, 6);
    c2d_fill_rect(c, -22, -1, 44, 6);
    FILL("#a8703f");
    c2d_fill_rect(c, -22, -8, 44, 2);
    c2d_fill_rect(c, -22, -1, 44, 2);
    FILL("#5a3a22");
    c2d_fill_rect(c, -22, 6, 44, 3);
  } else if (o->type == OB_BIN) {
    FILL("#6f7780");
    c2d_begin_path(c); c2d_arc(c, 0, 0, 12, 0, TAU, 0); c2d_fill(c);
    FILL("#2a2e33");
    c2d_begin_path(c); c2d_arc(c, 0, 0, 8, 0, TAU, 0); c2d_fill(c);
    STROKE("#9aa3ad");
    c2d_line_width(c, 2);
    c2d_begin_path(c); c2d_arc(c, 0, 0, 12, 0, TAU, 0); c2d_stroke(c);
  } else { /* kiosk */
    c2d_rotate(c, o->rot);
    FILL("#2a4f7c");
    rr(c, -24, -22, 48, 44, 5);
    c2d_fill(c);
    for (int i = -24; i < 24; i += 12) {
      FILL(((i / 12) % 2) ? "#f2d04a" : "#e24b4b"); /* js: (i / 12) % 2 -- truthy for +-1 */
      c2d_fill_rect(c, i, -22, 12, 14);
    }
    FILL("#cfe6ff");
    c2d_fill_rect(c, -18, -4, 36, 12);
    FILL("#fff");
    c2d_global_alpha(c, 0.4);
    c2d_fill_rect(c, -18, -4, 14, 12);
  }
  c2d_restore(c);
}

/* js: buildBG */
void build_bg(c2d *c) {
  FILL("#120e18");
  c2d_fill_rect(c, 0, 0, SCREEN_W, SCREEN_H);
  hex_p(c, MALL_AO + 30);
  FILL("#352a33");
  c2d_fill(c);
  hex_p(c, MALL_AO + 9);
  STROKE("#c9a24b");
  c2d_line_width(c, 5);
  c2d_stroke(c);
  c2d_save(c);
  c2d_begin_path(c);
  add_hex(c, MALL_AO);
  add_hex(c, MALL_AI);
  c2d_clip(c, C2D_EVENODD);
  FILL("#ebeae4");
  c2d_fill_rect(c, 0, 0, SCREEN_W, SCREEN_H);
  for (int x = 0; x < SCREEN_W; x += 40)
    for (int y = 0; y < SCREEN_H; y += 40)
      if (((x + y) / 40) % 2 == 0) {
        FILL("#f6f5f1");
        c2d_fill_rect(c, x, y, 40, 40);
      }
  STROKE("#cdcbc3");
  c2d_line_width(c, 1);
  c2d_begin_path(c);
  for (int x = 0; x < SCREEN_W; x += 40) { c2d_move_to(c, x, 0); c2d_line_to(c, x, SCREEN_H); }
  for (int y = 0; y < SCREEN_H; y += 40) { c2d_move_to(c, 0, y); c2d_line_to(c, SCREEN_W, y); }
  c2d_stroke(c);
  hex_p(c, 225);
  c2d_line_join(c, C2D_JOIN_MITER);
  c2d_miter_limit(c, 10);
  STROKE("#a3161f");
  c2d_line_width(c, 76);
  c2d_stroke(c);
  { static const double dash[2] = {14, 14}; c2d_line_dash(c, dash, 2); }
  STROKE("#b72530");
  c2d_line_width(c, 44);
  c2d_stroke(c);
  c2d_line_dash(c, NULL, 0);
  STROKE("#d9b24a");
  c2d_line_width(c, 3);
  hex_p(c, 225 + 38);
  c2d_stroke(c);
  hex_p(c, 225 - 38);
  c2d_stroke(c);
  STROKE("#e9cf85");
  c2d_line_width(c, 1);
  hex_p(c, 225 + 32);
  c2d_stroke(c);
  hex_p(c, 225 - 32);
  c2d_stroke(c);
  for (int i = 0; i < 12; i++) {
    double a = (i * TAU) / 12 + 0.2, mx = -1e9;
    for (int k = 0; k < 6; k++) { double d = cos(a) * N[k].x + sin(a) * N[k].y; if (d > mx) mx = d; }
    double r = 225 / mx;
    c2d_paint gr = c2d_radial_gradient(MALL_CX + cos(a) * r, MALL_CY + sin(a) * r, 0, MALL_CX + cos(a) * r, MALL_CY + sin(a) * r, 70);
    c2d_color_stop(&gr, 0, c2d_color("rgba(255,255,230,.35)"));
    c2d_color_stop(&gr, 1, c2d_color("rgba(255,255,230,0)"));
    c2d_fill_style(c, gr);
    c2d_fill_rect(c, 0, 0, SCREEN_W, SCREEN_H);
  }
  c2d_restore(c);
  /* inner shop block */
  hex_p(c, MALL_AI);
  {
    c2d_paint gr = c2d_linear_gradient(0, MALL_CY - 170, 0, MALL_CY + 170);
    c2d_color_stop(&gr, 0, c2d_color("#3a3556"));
    c2d_color_stop(&gr, 1, c2d_color("#221f35"));
    c2d_fill_style(c, gr);
  }
  c2d_fill(c);
  STROKE("#c9a24b");
  c2d_line_width(c, 4);
  c2d_stroke(c);
  hex_p(c, MALL_AI - 40);
  STROKE("rgba(255,255,255,.07)");
  c2d_line_width(c, 2);
  c2d_stroke(c);
  for (int k = 0; k < 6; k++) {
    const Normal *n = &N[k];
    double bx, by;
    polar(k, MALL_AI, 0, &bx, &by);
    c2d_save(c);
    c2d_translate(c, bx, by);
    c2d_rotate(c, n->a);
    FILL("#1b1a26");
    c2d_fill_rect(c, -4, -76, 6, 152);
    for (int i = -72; i < 72; i += 18) {
      FILL(((i / 18) % 2) ? "#f4f4f4" : SH[k].c);
      c2d_fill_rect(c, 0, i, 12, 18);
    }
    FILL("rgba(150,220,255,.55)");
    c2d_fill_rect(c, 0, -24, 7, 48);
    FILL("rgba(255,255,255,.4)");
    c2d_fill_rect(c, 0, -24, 2, 48);
    c2d_restore(c);
    double sx, sy;
    polar(k, MALL_AI - 36, 0, &sx, &sy);
    FILL("rgba(10,8,18,.75)");
    rr(c, sx - 48, sy - 13, 96, 26, 6);
    c2d_fill(c);
    STROKE(SH[k].c);
    c2d_line_width(c, 2);
    c2d_stroke(c);
    FILL(SH[k].c);
    c2d_font(c, 14, 1);
    c2d_text_align(c, C2D_ALIGN_CENTER);
    c2d_text_baseline(c, C2D_BASELINE_MIDDLE);
    c2d_fill_text(c, SH[k].n, sx, sy + 1);
  }
  /* fountain & title in the middle */
  FILL("#6e7f94");
  c2d_begin_path(c); c2d_arc(c, MALL_CX, MALL_CY + 38, 34, 0, TAU, 0); c2d_fill(c);
  FILL("#7fc4ea");
  c2d_begin_path(c); c2d_arc(c, MALL_CX, MALL_CY + 38, 28, 0, TAU, 0); c2d_fill(c);
  FILL("#bfe7fa");
  c2d_begin_path(c); c2d_arc(c, MALL_CX, MALL_CY + 38, 9, 0, TAU, 0); c2d_fill(c);
  c2d_font(c, 26, 1);
  FILL("#ff5ca8");
  c2d_shadow(c, "#ff2a8a", 14);
  c2d_fill_text(c, "CHOPPING", MALL_CX, MALL_CY - 48);
  FILL("#5cd6ff");
  c2d_shadow(c, "#1ab0ff", 14);
  c2d_fill_text(c, "MALL", MALL_CX, MALL_CY - 20);
  c2d_shadow(c, NULL, 0);
  for (int i = 0; i < OB_COUNT; i++) draw_ob(c, &OB[i]);
  {
    c2d_paint v = c2d_radial_gradient(MALL_CX, MALL_CY, 250, MALL_CX, MALL_CY, 560);
    c2d_color_stop(&v, 0, c2d_color("rgba(0,0,0,0)"));
    c2d_color_stop(&v, 1, c2d_color("rgba(0,0,0,.55)"));
    c2d_fill_style(c, v);
    c2d_fill_rect(c, 0, 0, SCREEN_W, SCREEN_H);
  }
}
