/* glow.c - canvas shadowBlur: draw the shape's alpha mask, blur it with a true
 * Gaussian (sigma = blur / 2, in backing-store pixels like Chrome), tint it with
 * shadowColor and composite it under the shape. Static text glows are cached. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "c2d_internal.h"

#define GLOW_W 1000 /* capacity in logical pixels (title text + blur margin) */
#define GLOW_H 260

void c2d_glow_init(c2d *c) {
  int s = c->scale, w = GLOW_W * s, h = GLOW_H * s;
  c2d_buf_init(&c->glow_scratch, w, h);
  c->blur_a = malloc((size_t)w * (size_t)h * sizeof(float));
  c->blur_b = malloc((size_t)w * (size_t)h * sizeof(float));
  c->tmp_path = plutovg_path_create();
  if (c->glow_cache_on)
    for (int i = 0; i < C2D_GLOW_ENTRIES; i++) c2d_buf_init(&c->glow[i].buf, w, h);
}

void c2d_glow_free(c2d *c) {
  c2d_buf_free(&c->glow_scratch);
  free(c->blur_a);
  free(c->blur_b);
  plutovg_path_destroy(c->tmp_path);
  for (int i = 0; i < C2D_GLOW_ENTRIES; i++) c2d_buf_free(&c->glow[i].buf);
}

/* separable Gaussian blur of a w*h float image, in place via tmp */
static void gaussian(float *a, float *tmp, int w, int h, double sigma) {
  int rad = (int)ceil(3 * sigma);
  if (rad < 1) return;
  float k[512];
  if (rad > 255) rad = 255;
  double sum = 0;
  for (int i = -rad; i <= rad; i++) { k[i + rad] = (float)exp(-(i * i) / (2 * sigma * sigma)); sum += k[i + rad]; }
  for (int i = 0; i <= 2 * rad; i++) k[i] = (float)(k[i] / sum);
  for (int y = 0; y < h; y++) { /* horizontal: a -> tmp */
    const float *src = a + (size_t)y * w;
    float *dst = tmp + (size_t)y * w;
    for (int x = 0; x < w; x++) {
      int lo = x - rad < 0 ? -(x - rad) : 0, hi = x + rad >= w ? x + rad - w + 1 : 0;
      float v = 0;
      for (int i = lo; i <= 2 * rad - hi; i++) v += src[x - rad + i] * k[i];
      dst[x] = v;
    }
  }
  for (int x = 0; x < w; x++) { /* vertical: tmp -> a */
    for (int y = 0; y < h; y++) {
      int lo = y - rad < 0 ? -(y - rad) : 0, hi = y + rad >= h ? y + rad - h + 1 : 0;
      float v = 0;
      for (int i = lo; i <= 2 * rad - hi; i++) v += tmp[(size_t)(y - rad + i) * w + x] * k[i];
      a[(size_t)y * w + x] = v;
    }
  }
}

static void composite(c2d *c, plutovg_canvas_t *cv, c2d_buf *b, int bx, int by, int bw, int bh) {
  plutovg_canvas_save(cv);
  plutovg_canvas_reset_matrix(cv);
  plutovg_matrix_t m;
  plutovg_matrix_init_translate(&m, (float)bx, (float)by);
  plutovg_canvas_set_texture(cv, b->surf, PLUTOVG_TEXTURE_TYPE_PLAIN, (float)C2D_CUR(c).alpha, &m);
  plutovg_canvas_set_operator(cv, PLUTOVG_OPERATOR_SRC_OVER);
  plutovg_canvas_fill_rect(cv, (float)bx, (float)by, (float)bw, (float)bh);
  plutovg_canvas_restore(cv);
}

void c2d_glow_draw(c2d *c, plutovg_canvas_t *cv, int is_stroke, uint32_t text_key) {
  const c2d_state *s = &C2D_CUR(c);
  int sc = c->scale;
  /* the compositing below clears the canvas path; keep a copy to restore */
  plutovg_path_reset(c->tmp_path);
  plutovg_path_add_path(c->tmp_path, plutovg_canvas_get_path(cv), NULL);
#define RESTORE_PATH() do { plutovg_canvas_new_path(cv); plutovg_canvas_add_path(cv, c->tmp_path); } while (0)

  if (text_key && c->glow_cache_on)
    for (int i = 0; i < C2D_GLOW_ENTRIES; i++) {
      c2d_glow_entry *e = &c->glow[i];
      if (e->used && e->key == text_key) {
        composite(c, cv, &e->buf, e->x, e->y, e->w, e->h);
        RESTORE_PATH();
        return;
      }
    }

  double sigma = s->blur / 2.0 * sc;
  int pad = (int)ceil(3 * sigma) + 1;
  plutovg_rect_t ex;
  if (is_stroke) plutovg_canvas_stroke_extents(cv, &ex); else plutovg_canvas_fill_extents(cv, &ex);
  int x0 = (int)floorf(ex.x) - pad, y0 = (int)floorf(ex.y) - pad;
  int x1 = (int)ceilf(ex.x + ex.w) + pad, y1 = (int)ceilf(ex.y + ex.h) + pad;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > c->pw) x1 = c->pw;
  if (y1 > c->ph) y1 = c->ph;
  int bw = x1 - x0, bh = y1 - y0;
  if (bw <= 0 || bh <= 0 || bw > c->glow_scratch.cap_w || bh > c->glow_scratch.cap_h) return;

  /* 1. alpha mask of the shape */
  c2d_buf *sb = &c->glow_scratch;
  int W = sb->cap_w;
  for (int r = 0; r < bh; r++) memset(sb->data + (size_t)r * W * 4, 0, (size_t)bw * 4);
  plutovg_matrix_t m;
  plutovg_canvas_get_matrix(cv, &m);
  m.e -= (float)x0; m.f -= (float)y0;
  plutovg_canvas_t *mc = sb->cv;
  plutovg_canvas_set_matrix(mc, &m);
  const c2d_paint *paint = is_stroke ? &s->stroke : &s->fill;
  float ma = paint->kind == 0 ? paint->a : 1.0f;
  plutovg_canvas_set_rgba(mc, 1, 1, 1, ma);
  plutovg_canvas_set_opacity(mc, 1);
  plutovg_canvas_set_operator(mc, PLUTOVG_OPERATOR_SRC_OVER);
  if (is_stroke) {
    plutovg_canvas_set_line_width(mc, (float)s->line_width);
    plutovg_canvas_set_miter_limit(mc, (float)s->miter_limit);
    plutovg_canvas_set_dash(mc, 0, s->ndash ? s->dash : NULL, s->ndash);
  }
  plutovg_canvas_new_path(mc);
  plutovg_canvas_add_path(mc, c->tmp_path);
  if (is_stroke) plutovg_canvas_stroke(mc); else plutovg_canvas_fill(mc);

  /* 2. blur the alpha channel */
  float *a = c->blur_a;
  for (int r = 0; r < bh; r++)
    for (int x = 0; x < bw; x++) a[(size_t)r * bw + x] = sb->data[((size_t)r * W + x) * 4 + 3] / 255.0f;
  gaussian(a, c->blur_b, bw, bh, sigma);

  /* 3. tint with shadowColor (premultiplied BGRA) */
  c2d_buf *dst = sb;
  c2d_glow_entry *entry = NULL;
  if (text_key && c->glow_cache_on) {
    entry = &c->glow[c->glow_next];
    c->glow_next = (c->glow_next + 1) % C2D_GLOW_ENTRIES;
    dst = &entry->buf;
  }
  int DW = dst->cap_w;
  float sr = s->shadow_color.r, sg = s->shadow_color.g, sbl = s->shadow_color.b, sa = s->shadow_color.a;
  for (int r = 0; r < bh; r++) {
    uint8_t *o = dst->data + (size_t)r * DW * 4;
    for (int x = 0; x < bw; x++) {
      float al = a[(size_t)r * bw + x] * sa;
      o[x * 4 + 0] = (uint8_t)(sbl * al * 255.0f + 0.5f);
      o[x * 4 + 1] = (uint8_t)(sg * al * 255.0f + 0.5f);
      o[x * 4 + 2] = (uint8_t)(sr * al * 255.0f + 0.5f);
      o[x * 4 + 3] = (uint8_t)(al * 255.0f + 0.5f);
    }
  }
  if (entry) { entry->used = 1; entry->key = text_key; entry->x = x0; entry->y = y0; entry->w = bw; entry->h = bh; }

  /* 4. composite the glow, then restore the path for the shape itself */
  composite(c, cv, dst, x0, y0, bw, bh);
  RESTORE_PATH();
}
