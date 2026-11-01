/* c2d.c - Canvas-2D-style API over PlutoVG (see c2d.h). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "c2d_internal.h"

extern const unsigned char font_regular_ttf[], font_bold_ttf[];
extern const unsigned int font_regular_ttf_len, font_bold_ttf_len;
static plutovg_font_face_t *face_regular, *face_bold;
static int faces_refs;

/* ---------------------------------------------------------------- buffers */
int c2d_buf_init(c2d_buf *b, int w, int h) {
  memset(b, 0, sizeof *b);
  b->data = calloc((size_t)w * (size_t)h * 4, 1);
  if (!b->data) return 0;
  b->cap_w = w; b->cap_h = h;
  b->surf = plutovg_surface_create_for_data(b->data, w, h, w * 4);
  b->cv = plutovg_canvas_create(b->surf);
  return b->surf && b->cv;
}
void c2d_buf_free(c2d_buf *b) {
  if (b->cv) plutovg_canvas_destroy(b->cv);
  if (b->surf) plutovg_surface_destroy(b->surf);
  free(b->data);
  memset(b, 0, sizeof *b);
}

/* ---------------------------------------------------------------- lifecycle */
c2d *c2d_create(int w, int h, int scale, int nlayers, int glow_cache) {
  c2d *c = calloc(1, sizeof *c);
  if (!c) return NULL;
  c->w = w; c->h = h; c->scale = scale; c->pw = w * scale; c->ph = h * scale;
  c->nlayers = nlayers; c->add_layer = nlayers > 1 ? 1 : 0;
  c->glow_cache_on = glow_cache;
  for (int i = 0; i < nlayers; i++) {
    c->surf[i] = plutovg_surface_create(c->pw, c->ph);
    c->cv[i] = plutovg_canvas_create(c->surf[i]);
    plutovg_canvas_scale(c->cv[i], (float)scale, (float)scale);
  }
  if (!faces_refs++) {
    face_regular = plutovg_font_face_load_from_data(font_regular_ttf, font_regular_ttf_len, 0, NULL, NULL);
    face_bold = plutovg_font_face_load_from_data(font_bold_ttf, font_bold_ttf_len, 0, NULL, NULL);
  }
  c->add_tile = 128 * scale;
  c2d_buf_init(&c->add_scratch, c->add_tile, c->add_tile);
  c2d_glow_init(c);
  /* canvas defaults */
  c2d_state *s = &c->st[0];
  s->fill = s->stroke = c2d_rgba(0, 0, 0, 1);
  s->line_width = 1; s->miter_limit = 10; s->alpha = 1; s->join = C2D_JOIN_MITER;
  s->font_px = 10; s->align = C2D_ALIGN_LEFT; s->baseline = C2D_BASELINE_ALPHABETIC;
  return c;
}

void c2d_destroy(c2d *c) {
  if (!c) return;
  c2d_glow_free(c);
  c2d_buf_free(&c->add_scratch);
  for (int i = 0; i < c->nlayers; i++) {
    plutovg_canvas_destroy(c->cv[i]);
    plutovg_surface_destroy(c->surf[i]);
  }
  if (--faces_refs == 0) {
    plutovg_font_face_destroy(face_regular);
    plutovg_font_face_destroy(face_bold);
  }
  free(c);
}

int c2d_scale(const c2d *c) { return c->scale; }
int c2d_layer_width(const c2d *c) { return c->pw; }
int c2d_layer_height(const c2d *c) { return c->ph; }
const uint8_t *c2d_layer_data(const c2d *c, int l) { return plutovg_surface_get_data(c->surf[l]); }
int c2d_layer_stride(const c2d *c, int l) { return plutovg_surface_get_stride(c->surf[l]); }
void c2d_set_layer(c2d *c, int layer) { c->base_layer = layer; }
void c2d_set_additive_layer(c2d *c, int layer) { c->add_layer = layer; }
void c2d_clear_layer(c2d *c, int layer) {
  memset(plutovg_surface_get_data(c->surf[layer]), 0, (size_t)plutovg_surface_get_stride(c->surf[layer]) * (size_t)c->ph);
}

/* ---------------------------------------------------------------- state */
#define EACH(c, i) for (int i = 0; i < (c)->nlayers; i++)
void c2d_save(c2d *c) {
  if (c->sp + 1 >= C2D_MAX_STATES) return;
  c->st[c->sp + 1] = c->st[c->sp];
  c->sp++;
  EACH(c, i) plutovg_canvas_save(c->cv[i]);
}
void c2d_restore(c2d *c) {
  if (c->sp == 0) return;
  c->sp--;
  EACH(c, i) plutovg_canvas_restore(c->cv[i]);
}
void c2d_translate(c2d *c, double x, double y) { EACH(c, i) plutovg_canvas_translate(c->cv[i], (float)x, (float)y); }
void c2d_rotate(c2d *c, double a) { EACH(c, i) plutovg_canvas_rotate(c->cv[i], (float)a); }
void c2d_fill_style(c2d *c, c2d_paint p) { C2D_CUR(c).fill = p; }
void c2d_stroke_style(c2d *c, c2d_paint p) { C2D_CUR(c).stroke = p; }
void c2d_line_width(c2d *c, double w) { C2D_CUR(c).line_width = w; }
void c2d_line_join(c2d *c, c2d_join j) { C2D_CUR(c).join = j; }
void c2d_miter_limit(c2d *c, double m) { C2D_CUR(c).miter_limit = m; }
void c2d_line_dash(c2d *c, const double *d, int n) {
  c2d_state *s = &C2D_CUR(c);
  s->ndash = (n >= 2) ? 2 : 0; /* the game only uses [a, b] patterns */
  if (s->ndash) { s->dash[0] = (float)d[0]; s->dash[1] = (float)d[1]; }
}
void c2d_global_alpha(c2d *c, double a) { C2D_CUR(c).alpha = a; }
void c2d_composite(c2d *c, c2d_comp op) { c->comp = op; }
void c2d_shadow(c2d *c, const char *css, double blur) {
  c2d_state *s = &C2D_CUR(c);
  s->blur = blur;
  if (css) { c2d_paint p = c2d_color(css); s->shadow_color = (plutovg_color_t){p.r, p.g, p.b, p.a}; }
}
void c2d_font(c2d *c, double px, int bold) { C2D_CUR(c).font_px = px; C2D_CUR(c).bold = bold; }
void c2d_text_align(c2d *c, c2d_align a) { C2D_CUR(c).align = a; }
void c2d_text_baseline(c2d *c, c2d_baseline b) { C2D_CUR(c).baseline = b; }

/* ---------------------------------------------------------------- paints */
typedef struct { char key[40]; c2d_paint p; int used; } color_slot;
static color_slot color_cache[256];

static uint32_t fnv(const char *s) { uint32_t h = 2166136261u; while (*s) h = (h ^ (uint8_t)*s++) * 16777619u; return h; }

c2d_paint c2d_color(const char *css) {
  size_t n = strlen(css);
  uint32_t h = fnv(css);
  if (n < sizeof color_cache[0].key) {
    for (int i = 0; i < 4; i++) {
      color_slot *s = &color_cache[(h + (uint32_t)i) & 255];
      if (s->used && strcmp(s->key, css) == 0) return s->p;
    }
  }
  plutovg_color_t pc;
  c2d_paint p = {0};
  if (plutovg_color_parse(&pc, css, (int)n) < 0) { fprintf(stderr, "c2d: bad colour '%s'\n", css); pc = (plutovg_color_t){1, 0, 1, 1}; }
  p.kind = 0; p.r = pc.r; p.g = pc.g; p.b = pc.b; p.a = pc.a;
  if (n < sizeof color_cache[0].key) {
    for (int i = 0; i < 4; i++) {
      color_slot *s = &color_cache[(h + (uint32_t)i) & 255];
      if (!s->used || i == 3) { strcpy(s->key, css); s->p = p; s->used = 1; break; }
    }
  }
  return p;
}
c2d_paint c2d_rgba(int r, int g, int b, double a) {
  c2d_paint p = {0};
  p.r = (float)r / 255.0f; p.g = (float)g / 255.0f; p.b = (float)b / 255.0f; p.a = (float)a;
  return p;
}
c2d_paint c2d_linear_gradient(double x0, double y0, double x1, double y1) {
  c2d_paint p = {0};
  p.kind = 1; p.x0 = x0; p.y0 = y0; p.x1 = x1; p.y1 = y1;
  return p;
}
c2d_paint c2d_radial_gradient(double x0, double y0, double r0, double x1, double y1, double r1) {
  c2d_paint p = {0};
  p.kind = 2; p.x0 = x0; p.y0 = y0; p.r0 = r0; p.x1 = x1; p.y1 = y1; p.r1 = r1;
  return p;
}
void c2d_color_stop(c2d_paint *g, double off, c2d_paint col) {
  if (g->nstops >= C2D_MAX_STOPS) return;
  c2d_stop *s = &g->stops[g->nstops++];
  s->off = (float)off; s->r = col.r; s->g = col.g; s->b = col.b; s->a = col.a;
}

/* Canvas interpolates gradient colours as NON-premultiplied RGBA; PlutoVG
 * interpolates premultiplied. Expand each segment into dense stops of
 * non-premultiplied interpolated colour so the two agree (matters where the colour
 * changes while alpha falls, e.g. the fire gradient's last stop). */
#define SUBDIV 16
static int dense_stops(const c2d_paint *p, plutovg_gradient_stop_t *out) {
  int n = 0;
  for (int i = 0; i < p->nstops; i++) {
    const c2d_stop *a = &p->stops[i];
    if (i == 0) out[n++] = (plutovg_gradient_stop_t){a->off, {a->r, a->g, a->b, a->a}};
    if (i + 1 < p->nstops) {
      const c2d_stop *b = &p->stops[i + 1];
      for (int k = 1; k <= SUBDIV; k++) {
        float t = (float)k / SUBDIV;
        out[n++] = (plutovg_gradient_stop_t){a->off + (b->off - a->off) * t,
          {a->r + (b->r - a->r) * t, a->g + (b->g - a->g) * t, a->b + (b->b - a->b) * t, a->a + (b->a - a->a) * t}};
      }
    }
  }
  return n;
}

/* sets paint + stroke parameters + opacity on a layer canvas */
void c2d_apply_state(c2d *c, plutovg_canvas_t *cv, const c2d_paint *p, int is_stroke) {
  const c2d_state *s = &C2D_CUR(c);
  if (p->kind == 0) plutovg_canvas_set_rgba(cv, p->r, p->g, p->b, p->a);
  else {
    plutovg_gradient_stop_t st[C2D_MAX_STOPS * SUBDIV + 2];
    int n = dense_stops(p, st);
    if (p->kind == 1)
      plutovg_canvas_set_linear_gradient(cv, (float)p->x0, (float)p->y0, (float)p->x1, (float)p->y1, PLUTOVG_SPREAD_METHOD_PAD, st, n, NULL);
    else /* canvas: start circle (x0,y0,r0), end circle (x1,y1,r1); PlutoVG: end = (cx,cy,cr), start = focal (fx,fy,fr) */
      plutovg_canvas_set_radial_gradient(cv, (float)p->x1, (float)p->y1, (float)p->r1, (float)p->x0, (float)p->y0, (float)p->r0,
                                         PLUTOVG_SPREAD_METHOD_PAD, st, n, NULL);
  }
  plutovg_canvas_set_opacity(cv, (float)s->alpha);
  plutovg_canvas_set_operator(cv, PLUTOVG_OPERATOR_SRC_OVER);
  if (is_stroke) {
    plutovg_canvas_set_line_width(cv, (float)s->line_width);
    plutovg_canvas_set_line_join(cv, s->join == C2D_JOIN_ROUND ? PLUTOVG_LINE_JOIN_ROUND : s->join == C2D_JOIN_BEVEL ? PLUTOVG_LINE_JOIN_BEVEL : PLUTOVG_LINE_JOIN_MITER);
    plutovg_canvas_set_line_cap(cv, PLUTOVG_LINE_CAP_BUTT);
    plutovg_canvas_set_miter_limit(cv, (float)s->miter_limit);
    if (s->ndash) plutovg_canvas_set_dash(cv, 0, s->dash, s->ndash);
    else plutovg_canvas_set_dash(cv, 0, NULL, 0);
  }
}

/* ---------------------------------------------------------------- paths */
#define ACT(c) ((c)->cv[c2d_active(c)])
void c2d_begin_path(c2d *c) { plutovg_canvas_new_path(ACT(c)); }
void c2d_close_path(c2d *c) { plutovg_canvas_close_path(ACT(c)); }
void c2d_move_to(c2d *c, double x, double y) { plutovg_canvas_move_to(ACT(c), (float)x, (float)y); }
static int path_empty(plutovg_canvas_t *cv);
/* canvas: lineTo on an empty path starts a subpath at the point (like moveTo) */
void c2d_line_to(c2d *c, double x, double y) {
  plutovg_canvas_t *cv = ACT(c);
  if (path_empty(cv)) plutovg_canvas_move_to(cv, (float)x, (float)y);
  else plutovg_canvas_line_to(cv, (float)x, (float)y);
}
void c2d_rect(c2d *c, double x, double y, double w, double h) { plutovg_canvas_rect(ACT(c), (float)x, (float)y, (float)w, (float)h); }
void c2d_arc(c2d *c, double x, double y, double r, double a0, double a1, int ccw) {
  plutovg_canvas_arc(ACT(c), (float)x, (float)y, (float)r, (float)a0, (float)a1, ccw != 0);
}

static int path_empty(plutovg_canvas_t *cv) {
  const plutovg_path_element_t *e;
  return plutovg_path_get_elements(plutovg_canvas_get_path(cv), &e) == 0;
}

/* canvas arcTo (HTML spec): tangent arc between the lines (p0,p1) and (p1,p2) */
void c2d_arc_to(c2d *c, double x1, double y1, double x2, double y2, double r) {
  plutovg_canvas_t *cv = ACT(c);
  if (path_empty(cv)) { plutovg_canvas_move_to(cv, (float)x1, (float)y1); return; }
  float fx, fy;
  plutovg_canvas_get_current_point(cv, &fx, &fy);
  double x0 = fx, y0 = fy;
  double v1x = x0 - x1, v1y = y0 - y1, v2x = x2 - x1, v2y = y2 - y1;
  double l1 = hypot(v1x, v1y), l2 = hypot(v2x, v2y);
  double cross = v1x * v2y - v1y * v2x;
  if (l1 == 0 || l2 == 0 || r == 0 || fabs(cross) < 1e-12 * l1 * l2) { plutovg_canvas_line_to(cv, (float)x1, (float)y1); return; }
  v1x /= l1; v1y /= l1; v2x /= l2; v2y /= l2;
  double cosang = v1x * v2x + v1y * v2y;
  if (cosang > 1) cosang = 1; else if (cosang < -1) cosang = -1;
  double half = acos(cosang) / 2;
  double d = r / tan(half);                 /* tangent length */
  double t1x = x1 + v1x * d, t1y = y1 + v1y * d, t2x = x1 + v2x * d, t2y = y1 + v2y * d;
  double bx = v1x + v2x, by = v1y + v2y, bl = hypot(bx, by);
  double dc = r / sin(half);
  double cx = x1 + bx / bl * dc, cy = y1 + by / bl * dc;
  double a0 = atan2(t1y - cy, t1x - cx), a1 = atan2(t2y - cy, t2x - cx);
  int ccw = ((x1 - x0) * (y2 - y1) - (y1 - y0) * (x2 - x1)) < 0;
  plutovg_canvas_line_to(cv, (float)t1x, (float)t1y);
  plutovg_canvas_arc(cv, (float)cx, (float)cy, (float)r, (float)a0, (float)a1, ccw);
}

/* canvas ellipse(): built from cubic segments (exact for the kappa formula) */
void c2d_ellipse(c2d *c, double cx, double cy, double rx, double ry, double rot, double a0, double a1, int ccw) {
  plutovg_canvas_t *cv = ACT(c);
  double da = a1 - a0;
  if (!ccw && da >= TAU_C) da = TAU_C;
  else if (ccw && da <= -TAU_C) da = -TAU_C;
  else if (!ccw && da < 0) da = fmod(da, TAU_C) + TAU_C;
  else if (ccw && da > 0) da = fmod(da, TAU_C) - TAU_C;
  int n = (int)ceil(fabs(da) / (TAU_C / 4));
  if (n == 0) return;
  double seg = da / n, k = 4.0 / 3.0 * tan(seg / 4), cr = cos(rot), sr = sin(rot);
#define EP(t, X, Y) do { double _x = rx * cos(t), _y = ry * sin(t); X = cx + cr * _x - sr * _y; Y = cy + sr * _x + cr * _y; } while (0)
#define ET(t, X, Y) do { double _x = -rx * sin(t), _y = ry * cos(t); X = cr * _x - sr * _y; Y = sr * _x + cr * _y; } while (0)
  double px, py;
  EP(a0, px, py);
  if (path_empty(cv)) plutovg_canvas_move_to(cv, (float)px, (float)py);
  else plutovg_canvas_line_to(cv, (float)px, (float)py);
  for (int i = 0; i < n; i++) {
    double ta = a0 + seg * i, tb = a0 + seg * (i + 1);
    double ax, ay, bx, by, tax, tay, tbx, tby;
    EP(ta, ax, ay); EP(tb, bx, by); ET(ta, tax, tay); ET(tb, tbx, tby);
    plutovg_canvas_cubic_to(cv, (float)(ax + k * tax), (float)(ay + k * tay), (float)(bx - k * tbx), (float)(by - k * tby), (float)bx, (float)by);
  }
}

/* ---------------------------------------------------------------- painting */
static void add_tiles(c2d *c, plutovg_canvas_t *cv, const c2d_paint *paint, int is_stroke);

static void paint_path(c2d *c, int is_stroke, uint32_t text_key) {
  plutovg_canvas_t *cv = ACT(c);
  const c2d_state *s = &C2D_CUR(c);
  const c2d_paint *paint = is_stroke ? &s->stroke : &s->fill;
  c2d_apply_state(c, cv, paint, is_stroke);
  if (c->comp == C2D_LIGHTER) { add_tiles(c, cv, paint, is_stroke); return; }
  if (s->blur > 0 && s->shadow_color.a > 0) c2d_glow_draw(c, cv, is_stroke, text_key);
  c2d_apply_state(c, cv, paint, is_stroke); /* glow drawing changed the paint */
  if (is_stroke) plutovg_canvas_stroke_preserve(cv);
  else plutovg_canvas_fill_preserve(cv);
}
void c2d_fill(c2d *c) { paint_path(c, 0, 0); }
void c2d_stroke(c2d *c) { paint_path(c, 1, 0); }

void c2d_clip(c2d *c, c2d_rule rule) {
  /* clip_path() resets the canvas path first, so never pass the canvas's own path */
  plutovg_path_reset(c->tmp_path);
  plutovg_path_add_path(c->tmp_path, plutovg_canvas_get_path(ACT(c)), NULL);
  const plutovg_path_t *path = c->tmp_path;
  EACH(c, i) {
    plutovg_canvas_set_fill_rule(c->cv[i], rule == C2D_EVENODD ? PLUTOVG_FILL_RULE_EVEN_ODD : PLUTOVG_FILL_RULE_NON_ZERO);
    plutovg_canvas_clip_path(c->cv[i], path);
    plutovg_canvas_set_fill_rule(c->cv[i], PLUTOVG_FILL_RULE_NON_ZERO);
  }
}
void c2d_fill_rect(c2d *c, double x, double y, double w, double h) { c2d_begin_path(c); c2d_rect(c, x, y, w, h); c2d_fill(c); }
void c2d_stroke_rect(c2d *c, double x, double y, double w, double h) { c2d_begin_path(c); c2d_rect(c, x, y, w, h); c2d_stroke(c); }

/* 'lighter': render the current path into a small scratch surface, then ADD it
 * (premultiplied, clamped) into the additive layer. PlutoVG has no add operator. */
static void add_tiles(c2d *c, plutovg_canvas_t *cv, const c2d_paint *paint, int is_stroke) {
  plutovg_rect_t e;
  if (is_stroke) plutovg_canvas_stroke_extents(cv, &e); else plutovg_canvas_fill_extents(cv, &e);
  int x0 = (int)floorf(e.x) - 1, y0 = (int)floorf(e.y) - 1, x1 = (int)ceilf(e.x + e.w) + 1, y1 = (int)ceilf(e.y + e.h) + 1;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > c->pw) x1 = c->pw;
  if (y1 > c->ph) y1 = c->ph;
  if (x1 <= x0 || y1 <= y0) return;
  const plutovg_path_t *path = plutovg_canvas_get_path(cv);
  plutovg_canvas_t *sc = c->add_scratch.cv;
  uint8_t *dstb = plutovg_surface_get_data(c->surf[c2d_active(c)]);
  int dstride = plutovg_surface_get_stride(c->surf[c2d_active(c)]), T = c->add_tile;
  plutovg_matrix_t m0;
  plutovg_canvas_get_matrix(cv, &m0);
  for (int ty = y0; ty < y1; ty += T)
    for (int tx = x0; tx < x1; tx += T) {
      int tw = x1 - tx < T ? x1 - tx : T, th = y1 - ty < T ? y1 - ty : T;
      for (int r = 0; r < th; r++) memset(c->add_scratch.data + (size_t)r * T * 4, 0, (size_t)tw * 4);
      plutovg_matrix_t m = m0;
      m.e -= (float)tx; m.f -= (float)ty;
      plutovg_canvas_set_matrix(sc, &m);
      c2d_apply_state(c, sc, paint, is_stroke);
      plutovg_canvas_new_path(sc);
      plutovg_canvas_add_path(sc, path);
      if (is_stroke) plutovg_canvas_stroke(sc); else plutovg_canvas_fill(sc);
      for (int r = 0; r < th; r++) {
        const uint8_t *s = c->add_scratch.data + (size_t)r * T * 4;
        uint8_t *d = dstb + (size_t)(ty + r) * (size_t)dstride + (size_t)tx * 4;
        for (int i = 0; i < tw * 4; i++) { unsigned v = d[i] + s[i]; d[i] = v > 255 ? 255 : (uint8_t)v; }
      }
    }
}

/* ---------------------------------------------------------------- text */
void c2d_fill_text(c2d *c, const char *text, double x, double y) {
  plutovg_canvas_t *cv = ACT(c);
  const c2d_state *s = &C2D_CUR(c);
  plutovg_font_face_t *face = s->bold ? face_bold : face_regular;
  int len = (int)strlen(text);
  float asc, desc, gap;
  plutovg_font_face_get_metrics(face, (float)s->font_px, &asc, &desc, &gap, NULL);
  double xx = x, yy = y;
  if (s->align == C2D_ALIGN_CENTER)
    xx -= plutovg_font_face_text_extents(face, (float)s->font_px, text, len, PLUTOVG_TEXT_ENCODING_UTF8, NULL) / 2.0;
  if (s->baseline == C2D_BASELINE_MIDDLE) /* Blink: (rounded ascent - rounded descent) / 2 below y */
    yy += (floor(asc + 0.5) - floor(-desc + 0.5)) / 2.0;
  plutovg_canvas_set_font(cv, face, (float)s->font_px);
  plutovg_canvas_new_path(cv);
  plutovg_canvas_add_text(cv, text, len, PLUTOVG_TEXT_ENCODING_UTF8, (float)xx, (float)yy);
  uint32_t key = 0;
  if (c->glow_cache_on && s->blur > 0) { /* static text glows are cached */
    key = fnv(text) ^ (uint32_t)(s->font_px * 7) ^ (uint32_t)(x * 31) ^ ((uint32_t)(y * 17) << 8) ^ (uint32_t)(s->blur * 131)
          ^ (uint32_t)(s->shadow_color.r * 255 * 3 + s->shadow_color.g * 255 * 5 + s->shadow_color.b * 255 * 7) ^ ((uint32_t)s->bold << 3);
    if (!key) key = 1;
  }
  paint_path(c, 0, key);
}
