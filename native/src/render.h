/* render.h - drawing entry points. All drawing goes through c2d. */
#ifndef RENDER_H
#define RENDER_H
#include "c2d/c2d.h"
#include "state.h"
#include "world.h"

/* layers of the per-frame context */
enum { LAYER_W = 0, LAYER_X = 1, LAYER_H = 2, FRAME_LAYERS = 3 };

void txt(c2d *c, const char *s, double x, double y, double size, const char *col, c2d_align al, int bold); /* js: txt */
void add_hex(c2d *c, double a);                             /* js: addHex */
void hex_p(c2d *c, double a);                               /* js: hexP */
void rr(c2d *c, double x, double y, double w, double h, double r); /* js: rr */
void draw_ob(c2d *c, const Obstacle *o);                    /* js: drawOb */
void build_bg(c2d *c);                                      /* js: buildBG (c has one layer) */

/* js: draw(). The caller has already run game_pre_draw(). Draws world layers
 * W and X and the HUD layer H (the caller clears them first). */
void draw_frame(c2d *c, const Game *g);
void draw_hud(c2d *c, const Game *g);                       /* js: hud */
#endif
