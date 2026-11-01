/* layers.c - converting c2d layers for upload / testing. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "c2d_internal.h"

/* PlutoVG: premultiplied BGRA bytes (little endian ARGB32). raylib wants RGBA bytes; keep
 * premultiplied and only swap R and B (no unpremultiply: blending uses (ONE, ONE_MINUS_SRC_ALPHA)). */
void c2d_layer_to_rgba(const c2d *c, int layer, uint8_t *dst) {
  const uint8_t *src = c2d_layer_data(c, layer);
  int stride = c2d_layer_stride(c, layer);
  for (int y = 0; y < c->ph; y++) {
    const uint8_t *s = src + (size_t)y * stride;
    uint8_t *d = dst + (size_t)y * c->pw * 4;
    for (int x = 0; x < c->pw; x++, s += 4, d += 4) { d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = s[3]; }
  }
}

/* PNG of a layer composited over opaque black (for tests and diffs). */
int c2d_layer_write_png(const c2d *c, int layer, const char *path) {
  plutovg_surface_t *s = plutovg_surface_create(c->pw, c->ph);
  if (!s) return 0;
  uint8_t *d = plutovg_surface_get_data(s);
  const uint8_t *src = c2d_layer_data(c, layer);
  int ss = c2d_layer_stride(c, layer), ds = plutovg_surface_get_stride(s);
  for (int y = 0; y < c->ph; y++) {
    for (int x = 0; x < c->pw; x++) { /* premultiplied over black = same bytes, alpha forced opaque */
      const uint8_t *p = src + (size_t)y * ss + (size_t)x * 4;
      uint8_t *q = d + (size_t)y * ds + (size_t)x * 4;
      q[0] = p[0]; q[1] = p[1]; q[2] = p[2]; q[3] = 255;
    }
  }
  int ok = plutovg_surface_write_to_png(s, path);
  plutovg_surface_destroy(s);
  return ok;
}

/* CPU reference compositor (tests and screenshots): background, then W (source-over)
 * and X (additive), both bilinearly shifted by (sx, sy) physical pixels like the
 * background, then H (source-over). Output: opaque RGBA, tightly packed. This mirrors
 * the GPU path in main.c. */
static void sample(const uint8_t *d, int stride, int w, int h, double fx, double fy, float out[4]) {
  int x0 = (int)floor(fx), y0 = (int)floor(fy);
  double ax = fx - x0, ay = fy - y0;
  float acc[4] = {0, 0, 0, 0};
  for (int j = 0; j < 2; j++)
    for (int i = 0; i < 2; i++) {
      int x = x0 + i, y = y0 + j;
      if (x < 0) x = 0;
      if (y < 0) y = 0;
      if (x >= w) x = w - 1;
      if (y >= h) y = h - 1;
      double wgt = (i ? ax : 1 - ax) * (j ? ay : 1 - ay);
      const uint8_t *p = d + (size_t)y * stride + (size_t)x * 4;
      acc[0] += (float)(p[0] * wgt); acc[1] += (float)(p[1] * wgt); acc[2] += (float)(p[2] * wgt); acc[3] += (float)(p[3] * wgt);
    }
  for (int k = 0; k < 4; k++) out[k] = acc[k];
}

void c2d_compose_frame(const c2d *bg, const c2d *fr, int layer_w, int layer_x, int layer_h, double sx, double sy, uint8_t *rgba) {
  int w = fr->pw, h = fr->ph;
  const uint8_t *B = c2d_layer_data(bg, 0), *W = c2d_layer_data(fr, layer_w), *X = c2d_layer_data(fr, layer_x), *H = c2d_layer_data(fr, layer_h);
  int bs = c2d_layer_stride(bg, 0), ws = c2d_layer_stride(fr, layer_w), xs = c2d_layer_stride(fr, layer_x), hs = c2d_layer_stride(fr, layer_h);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      float b[4], wv[4], xv[4];
      sample(B, bs, w, h, x - sx, y - sy, b);
      sample(W, ws, w, h, x - sx, y - sy, wv);
      sample(X, xs, w, h, x - sx, y - sy, xv);
      const uint8_t *hp = H + (size_t)y * hs + (size_t)x * 4;
      float out[3];
      for (int k = 0; k < 3; k++) {
        float v = b[k] * (1 - wv[3] / 255.0f) + wv[k];
        v += xv[k];
        if (v > 255) v = 255;
        v = v * (1 - hp[3] / 255.0f) + hp[k];
        if (v > 255) v = 255;
        out[k] = v;
      }
      uint8_t *o = rgba + ((size_t)y * w + x) * 4; /* BGR order in -> RGB out */
      o[0] = (uint8_t)(out[2] + 0.5f); o[1] = (uint8_t)(out[1] + 0.5f); o[2] = (uint8_t)(out[0] + 0.5f); o[3] = 255;
    }
}
