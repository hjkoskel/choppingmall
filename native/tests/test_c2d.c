/* test_c2d.c - numeric checks of the canvas layer against the HTML canvas specification. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "c2d/c2d.h"
#include "world.h"
#define M_PI_C 3.14159265358979323846

static int fails, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
typedef struct { int b, g, r, a; } Px; /* premultiplied */
static Px px(const c2d *c, int layer, int x, int y) {
  const uint8_t *p = c2d_layer_data(c, layer) + (size_t)y * c2d_layer_stride(c, layer) + (size_t)x * 4;
  return (Px){p[0], p[1], p[2], p[3]};
}
static long count_alpha(const c2d *c, int layer, int thr) {
  long n = 0;
  for (int y = 0; y < c2d_layer_height(c); y++) for (int x = 0; x < c2d_layer_width(c); x++) if (px(c, layer, x, y).a >= thr) n++;
  return n;
}

static void test_gradients(void) {
  c2d *c = c2d_create(100, 100, 1, 1, 0);
  c2d_paint g = c2d_radial_gradient(50, 50, 0, 50, 50, 40);
  c2d_color_stop(&g, 0, c2d_rgba(255, 0, 0, 1));
  c2d_color_stop(&g, 1, c2d_rgba(0, 0, 255, 0));
  c2d_fill_style(c, g);
  c2d_fill_rect(c, 0, 0, 100, 100);
  /* canvas interpolates NON-premultiplied: at t = .5 colour (127.5,0,127.5), alpha .5 -> premult (64,0,64,128) */
  Px p = px(c, 0, 70, 50);
  CHECK(abs(p.r - 64) <= 4 && abs(p.b - 64) <= 4 && abs(p.a - 128) <= 3, "radial non-premultiplied midpoint r=%d b=%d a=%d (want 64,64,128)", p.r, p.b, p.a);
  p = px(c, 0, 50 + 30, 50); /* t = .75: colour (64,0,191), alpha .25 -> premult (16,0,48,64) */
  CHECK(abs(p.r - 16) <= 4 && abs(p.b - 48) <= 4 && abs(p.a - 64) <= 3, "radial t=.75 r=%d b=%d a=%d (want 16,48,64)", p.r, p.b, p.a);
  p = px(c, 0, 99, 99); /* beyond the end circle: padded with the last stop (alpha 0) */
  CHECK(p.a == 0, "radial pad beyond r1 alpha=%d", p.a);
  /* inner radius r0 = 20: inside it the first stop colour is padded */
  c2d_clear_layer(c, 0);
  g = c2d_radial_gradient(50, 50, 20, 50, 50, 40);
  c2d_color_stop(&g, 0, c2d_rgba(0, 255, 0, 1));
  c2d_color_stop(&g, 1, c2d_rgba(0, 255, 0, 0));
  c2d_fill_style(c, g);
  c2d_fill_rect(c, 0, 0, 100, 100);
  CHECK(px(c, 0, 55, 50).a == 255, "radial inside r0 is the first stop");
  p = px(c, 0, 80, 50); /* pixel centre 80.5 -> d = 30.5, t = (30.5 - 20) / 20 */
  CHECK(abs(p.a - (int)lround((1 - 10.5 / 20) * 255)) <= 3, "radial with r0: alpha=%d want %ld", p.a, lround((1 - 10.5 / 20) * 255));
  c2d_clear_layer(c, 0);
  g = c2d_linear_gradient(0, 0, 0, 100);
  c2d_color_stop(&g, 0, c2d_rgba(255, 255, 255, 1));
  c2d_color_stop(&g, 1, c2d_rgba(0, 0, 0, 1));
  c2d_fill_style(c, g);
  c2d_fill_rect(c, 0, 0, 100, 100);
  p = px(c, 0, 10, 25);
  CHECK(abs(p.r - 191) <= 4, "linear gradient at 25%% r=%d (want 191)", p.r);
  c2d_destroy(c);
}

static void test_paths(void) {
  c2d *c = c2d_create(120, 100, 1, 1, 0);
  c2d_fill_style(c, c2d_rgba(255, 255, 255, 1));
  /* arcTo rounded rectangle: area = w*h - (4 - pi) r^2 */
  c2d_begin_path(c);
  c2d_move_to(c, 30, 10);
  c2d_arc_to(c, 90, 10, 90, 70, 20); c2d_arc_to(c, 90, 70, 10, 70, 20); c2d_arc_to(c, 10, 70, 10, 10, 20); c2d_arc_to(c, 10, 10, 90, 10, 20);
  c2d_close_path(c);
  c2d_fill(c);
  double area = 80.0 * 60 - (4 - M_PI_C) * 400, got = (double)count_alpha(c, 0, 128);
  CHECK(fabs(got - area) / area < 0.01, "arcTo rounded rect area %.0f want %.0f", got, area);
  CHECK(px(c, 0, 11, 11).a == 0 && px(c, 0, 50, 40).a == 255 && px(c, 0, 50, 11).a == 255, "arcTo corner cut / edge / interior");
  c2d_clear_layer(c, 0);
  /* lineTo on an empty path starts a subpath (the robot hexagon relies on it) */
  c2d_begin_path(c);
  c2d_line_to(c, 10, 10); c2d_line_to(c, 60, 10); c2d_line_to(c, 60, 60); c2d_line_to(c, 10, 60); c2d_close_path(c);
  c2d_fill(c);
  CHECK(labs(count_alpha(c, 0, 128) - 2500) < 40, "lineTo-first polygon area %ld", count_alpha(c, 0, 128));
  c2d_clear_layer(c, 0);
  /* even-odd clip: ring between two rectangles */
  c2d_save(c);
  c2d_begin_path(c); c2d_rect(c, 10, 10, 80, 80); c2d_rect(c, 30, 30, 40, 40);
  c2d_clip(c, C2D_EVENODD);
  c2d_fill_rect(c, 0, 0, 120, 100);
  c2d_restore(c);
  CHECK(labs(count_alpha(c, 0, 128) - (6400 - 1600)) < 10, "evenodd clip ring area %ld want 4800", count_alpha(c, 0, 128));
  CHECK(px(c, 0, 50, 50).a == 0 && px(c, 0, 20, 20).a == 255, "evenodd hole / ring");
  c2d_clear_layer(c, 0);
  /* transforms apply in canvas order; save/restore */
  c2d_save(c);
  c2d_translate(c, 40, 30); c2d_rotate(c, M_PI_C / 2);
  c2d_fill_rect(c, 0, 0, 20, 10); /* rotated 90deg: occupies x in [30,40], y in [30,50] */
  c2d_restore(c);
  CHECK(px(c, 0, 35, 40).a == 255 && px(c, 0, 50, 35).a == 0, "translate+rotate order");
  c2d_fill_rect(c, 100, 80, 5, 5);
  CHECK(px(c, 0, 102, 82).a == 255, "restore resets the transform");
  c2d_clear_layer(c, 0);
  /* full ellipse area = pi rx ry (rotation must not change it) */
  c2d_begin_path(c); c2d_ellipse(c, 60, 50, 30, 12, 0.7, 0, 6.283185307179586, 0); c2d_fill(c);
  CHECK(fabs((double)count_alpha(c, 0, 128) - M_PI_C * 30 * 12) / (M_PI_C * 360) < 0.02, "ellipse area %ld", count_alpha(c, 0, 128));
  c2d_destroy(c);
}

static void test_additive(void) {
  c2d *c = c2d_create(60, 60, 1, 2, 0);
  c2d_set_layer(c, 0); c2d_set_additive_layer(c, 1);
  c2d_composite(c, C2D_LIGHTER);
  c2d_fill_style(c, c2d_rgba(150, 100, 50, 1));
  c2d_begin_path(c); c2d_arc(c, 30, 30, 15, 0, 6.283185307179586, 0); c2d_fill(c);
  Px p = px(c, 1, 30, 30);
  CHECK(p.r == 150 && p.g == 100 && p.b == 50, "single additive fill = colour (%d,%d,%d)", p.r, p.g, p.b);
  CHECK(px(c, 0, 30, 30).a == 0, "additive drawing does not touch the base layer");
  c2d_begin_path(c); c2d_arc(c, 30, 30, 15, 0, 6.283185307179586, 0); c2d_fill(c);
  p = px(c, 1, 30, 30);
  CHECK(p.r == 255 && p.g == 200 && p.b == 100, "two additive fills add and clamp (%d,%d,%d) want (255,200,100)", p.r, p.g, p.b);
  /* lighter with a gradient and a stroke */
  c2d_clear_layer(c, 1);
  c2d_stroke_style(c, c2d_rgba(100, 100, 100, 1)); c2d_line_width(c, 4);
  c2d_begin_path(c); c2d_move_to(c, 5, 30); c2d_line_to(c, 55, 30); c2d_stroke(c);
  CHECK(px(c, 1, 30, 30).r == 100 && px(c, 1, 30, 20).a == 0, "additive stroke");
  c2d_composite(c, C2D_SOURCE_OVER);
  c2d_fill_rect(c, 0, 0, 10, 10);
  CHECK(px(c, 0, 5, 5).a == 255, "source-over returns to the base layer");
  c2d_destroy(c);
}

static void test_glow(void) {
  /* a large half-plane: shadow alpha at distance d outside the edge = 0.5 erfc(d / (sigma sqrt 2)), sigma = blur / 2 */
  c2d *c = c2d_create(300, 300, 1, 1, 0);
  c2d_fill_style(c, c2d_rgba(255, 255, 255, 1));
  c2d_shadow(c, "rgba(255,0,0,1)", 20);
  c2d_fill_rect(c, 150, 60, 140, 140);
  double sigma = 10;
  for (int d = 5; d <= 25; d += 10) {
    double want = 0.5 * erfc(d / (sigma * sqrt(2.0))) * 255, got = px(c, 0, 150 - d - 1, 130).a + 0.0;
    /* pixel centre is d + 0.5 from the edge */
    want = 0.5 * erfc((d + 0.5) / (sigma * sqrt(2.0))) * 255;
    CHECK(fabs(got - want) <= 8, "glow alpha at %d px = %.0f want %.0f", d, got, want);
  }
  CHECK(px(c, 0, 200, 130).r == 255 && px(c, 0, 200, 130).g == 255, "the shape is drawn over its own shadow");
  c2d_destroy(c);
}

static void test_text(void) {
  c2d *c = c2d_create(300, 100, 1, 1, 0);
  c2d_fill_style(c, c2d_rgba(255, 255, 255, 1));
  c2d_font(c, 26, 1); c2d_text_align(c, C2D_ALIGN_CENTER); c2d_text_baseline(c, C2D_BASELINE_MIDDLE);
  c2d_fill_text(c, "MALL", 150, 50);
  long xmin = 999, xmax = -1, ymin = 999, ymax = -1;
  for (int y = 0; y < 100; y++) for (int x = 0; x < 300; x++) if (px(c, 0, x, y).a > 100) { if (x < xmin) xmin = x; if (x > xmax) xmax = x; if (y < ymin) ymin = y; if (y > ymax) ymax = y; }
  CHECK(xmax > xmin && fabs((xmin + xmax) / 2.0 - 150) < 3, "centred text spans %ld..%ld", xmin, xmax);
  CHECK(fabs((ymin + ymax) / 2.0 - 50) < 4, "middle baseline: glyph rows %ld..%ld around y=50", ymin, ymax);
  c2d_clear_layer(c, 0);
  c2d_font(c, 26, 0); c2d_text_align(c, C2D_ALIGN_LEFT);
  c2d_fill_text(c, "\xe2\x99\xa5 \xc2\xb7 \xe2\x80\x94 \xc3\x97", 10, 50); /* heart, middle dot, em dash, times: all glyphs present */
  CHECK(count_alpha(c, 0, 128) > 150, "special glyphs rendered (%ld px)", count_alpha(c, 0, 128));
  c2d_destroy(c);
}

int main(void) {
  world_init();
  test_gradients(); test_paths(); test_additive(); test_glow(); test_text();
  printf("c2d: %d checks, %d failed\n", checks, fails);
  return fails != 0;
}
