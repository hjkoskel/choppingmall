/* c2d_internal.h - shared by c2d.c, glow.c and layers.c. Not part of the public API. */
#ifndef C2D_INTERNAL_H
#define C2D_INTERNAL_H
#include <plutovg.h>
#include "c2d.h"

#define C2D_MAX_STATES 16
#define C2D_GLOW_ENTRIES 3

typedef struct {
  c2d_paint fill, stroke;
  double line_width, miter_limit, alpha, blur, font_px;
  c2d_join join;
  float dash[2];
  int ndash;
  plutovg_color_t shadow_color;
  int bold;
  c2d_align align;
  c2d_baseline baseline;
} c2d_state;

typedef struct { /* a pixel buffer with a PlutoVG surface wrapped around it */
  uint8_t *data;
  plutovg_surface_t *surf;
  plutovg_canvas_t *cv;
  int cap_w, cap_h;
} c2d_buf;

typedef struct { /* cached, already blurred and tinted glow of one text draw */
  int used;
  uint32_t key;
  int x, y, w, h; /* device-space placement of the valid region */
  c2d_buf buf;
} c2d_glow_entry;

struct c2d {
  int w, h, scale, pw, ph, nlayers;
  plutovg_surface_t *surf[C2D_MAX_LAYERS];
  plutovg_canvas_t *cv[C2D_MAX_LAYERS];
  int base_layer, add_layer;
  c2d_comp comp;
  c2d_state st[C2D_MAX_STATES];
  int sp;
  c2d_buf add_scratch; /* additive ('lighter') tiles */
  c2d_buf glow_scratch; /* blur mask / glow image */
  float *blur_a, *blur_b;
  int glow_cache_on, glow_next;
  c2d_glow_entry glow[C2D_GLOW_ENTRIES];
  int add_tile;
  plutovg_path_t *tmp_path; /* keeps the current path while a glow is composited */
};

#define C2D_CUR(c) ((c)->st[(c)->sp])
static inline int c2d_active(const c2d *c) { return c->comp == C2D_LIGHTER ? c->add_layer : c->base_layer; }

/* buffers */
int c2d_buf_init(c2d_buf *b, int w, int h);
void c2d_buf_free(c2d_buf *b);
/* glow.c */
void c2d_glow_init(c2d *c);
void c2d_glow_free(c2d *c);
/* draws the shadow of the path currently on layer canvas `cv` (fill or stroke).
 * text_key != 0 enables the glow cache for static text. */
void c2d_glow_draw(c2d *c, plutovg_canvas_t *cv, int is_stroke, uint32_t text_key);
/* paint application (c2d.c) */
void c2d_apply_state(c2d *c, plutovg_canvas_t *cv, const c2d_paint *p, int is_stroke);
#endif
