/* c2d.h - a Canvas-2D-style drawing API over PlutoVG.
 * Mirrors the subset of CanvasRenderingContext2D that index.html uses so the
 * drawing code ports statement by statement.
 *
 * A context owns up to C2D_MAX_LAYERS premultiplied-ARGB surfaces of
 * (w*scale) x (h*scale) pixels. All drawing uses logical coordinates; the base
 * transform is scale(scale, scale). Layer rules:
 *   - c2d_set_layer() chooses the layer that receives normal (source-over) drawing;
 *   - c2d_composite(C2D_LIGHTER) redirects drawing to the additive layer
 *     (set with c2d_set_additive_layer) and adds pixel values (clamped), exactly
 *     like canvas 'lighter';
 *   - save/restore, translate/rotate and clip apply to all layers at once, but
 *     the current path lives on one layer: switch layers only between shapes.
 */
#ifndef C2D_H
#define C2D_H
#include <stdint.h>

#define C2D_MAX_LAYERS 4
#define C2D_MAX_STOPS 4
#define TAU_C 6.283185307179586

typedef struct c2d c2d;
typedef enum { C2D_NONZERO, C2D_EVENODD } c2d_rule;
typedef enum { C2D_SOURCE_OVER, C2D_LIGHTER } c2d_comp;
typedef enum { C2D_ALIGN_LEFT, C2D_ALIGN_CENTER } c2d_align;
typedef enum { C2D_BASELINE_ALPHABETIC, C2D_BASELINE_MIDDLE } c2d_baseline;
typedef enum { C2D_JOIN_MITER, C2D_JOIN_ROUND, C2D_JOIN_BEVEL } c2d_join;

typedef struct {
  float off, r, g, b, a; /* straight (non-premultiplied) colour, 0..1 */
} c2d_stop;

typedef struct { /* a fill/stroke style: colour or gradient (value type) */
  int kind;      /* 0 colour, 1 linear gradient, 2 radial gradient */
  float r, g, b, a;
  double x0, y0, r0, x1, y1, r1;
  int nstops;
  c2d_stop stops[C2D_MAX_STOPS];
} c2d_paint;

/* lifecycle */
c2d *c2d_create(int w, int h, int scale, int nlayers, int glow_cache);
void c2d_destroy(c2d *c);
int c2d_scale(const c2d *c);
void c2d_clear_layer(c2d *c, int layer);               /* to transparent */
void c2d_set_layer(c2d *c, int layer);                 /* base layer for source-over */
void c2d_set_additive_layer(c2d *c, int layer);        /* layer used by C2D_LIGHTER */
const uint8_t *c2d_layer_data(const c2d *c, int layer);/* premultiplied BGRA bytes */
int c2d_layer_stride(const c2d *c, int layer);
int c2d_layer_width(const c2d *c);
int c2d_layer_height(const c2d *c);
/* converts a layer to premultiplied RGBA bytes (dst has w*h*4 bytes, tightly packed) */
void c2d_layer_to_rgba(const c2d *c, int layer, uint8_t *dst);
/* writes a layer, composited over opaque black, to a PNG (testing) */
int c2d_layer_write_png(const c2d *c, int layer, const char *path);

void c2d_compose_frame(const c2d *bg, const c2d *fr, int layer_w, int layer_x, int layer_h, double sx, double sy, uint8_t *rgba);

/* state */
void c2d_save(c2d *c);
void c2d_restore(c2d *c);
void c2d_translate(c2d *c, double x, double y);
void c2d_rotate(c2d *c, double a);
void c2d_fill_style(c2d *c, c2d_paint p);
void c2d_stroke_style(c2d *c, c2d_paint p);
void c2d_line_width(c2d *c, double w);
void c2d_line_join(c2d *c, c2d_join j);
void c2d_miter_limit(c2d *c, double m);
void c2d_line_dash(c2d *c, const double *dash, int n); /* n = 0 clears */
void c2d_global_alpha(c2d *c, double a);
void c2d_composite(c2d *c, c2d_comp op);
void c2d_shadow(c2d *c, const char *css_color, double blur); /* blur 0 disables; glows (shape + 3 sigma margin)
                                                                larger than 1000 x 260 logical px are skipped */
void c2d_font(c2d *c, double px, int bold);
void c2d_text_align(c2d *c, c2d_align a);
void c2d_text_baseline(c2d *c, c2d_baseline b);

/* paints */
c2d_paint c2d_color(const char *css);
c2d_paint c2d_rgba(int r, int g, int b, double a);
c2d_paint c2d_linear_gradient(double x0, double y0, double x1, double y1);
c2d_paint c2d_radial_gradient(double x0, double y0, double r0, double x1, double y1, double r1);
void c2d_color_stop(c2d_paint *g, double offset, c2d_paint color);

/* paths */
void c2d_begin_path(c2d *c);
void c2d_close_path(c2d *c);
void c2d_move_to(c2d *c, double x, double y);
void c2d_line_to(c2d *c, double x, double y);
void c2d_arc(c2d *c, double x, double y, double r, double a0, double a1, int ccw);
void c2d_arc_to(c2d *c, double x1, double y1, double x2, double y2, double r);
void c2d_ellipse(c2d *c, double x, double y, double rx, double ry, double rot, double a0, double a1, int ccw);
void c2d_rect(c2d *c, double x, double y, double w, double h);
void c2d_fill(c2d *c);
void c2d_stroke(c2d *c);
void c2d_clip(c2d *c, c2d_rule rule);
void c2d_fill_rect(c2d *c, double x, double y, double w, double h);
void c2d_stroke_rect(c2d *c, double x, double y, double w, double h);
void c2d_fill_text(c2d *c, const char *utf8, double x, double y);
#endif
