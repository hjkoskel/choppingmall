/* game.h - game logic entry points. Rendering reads Game; only logic mutates it
 * (plus game_pre_draw, which reproduces the state changes JS draw() performed). */
#ifndef GAME_H
#define GAME_H
#include "state.h"
#include "world.h"

void game_init(Game *g, uint32_t seed);   /* zero state, world_init, load best */
double game_random(Game *g);              /* js: Math.random */
void new_game(Game *g);                   /* js: newGame */
void update(Game *g, double dt);          /* js: update */
void update_game(Game *g, double dt);     /* js: updateGame */
void game_key_down(Game *g, int key);     /* js: keydown handler (edge part) */
void game_clear_keys(Game *g);            /* js: blur handler */
void game_pre_draw(Game *g, double *shake_x, double *shake_y); /* js: state changes inside draw() */

/* shared helpers (game.c) */
void add_p(Game *g, double x, double y, double vx, double vy, double life,
           int cr, int cg, int cb, double size, int add);       /* js: addP */
void burst(Game *g, double x, double y, int n, int cr, int cg, int cb, double sp, int add);
void toast(Game *g, const char *s);
void hurt_player(Game *g, double n);      /* js: hurtPlayer */
void kill_robot(Game *g);                 /* js: killRobot */
void move_entity(double *ex, double *ey, double dx, double dy, double r); /* js: move */
double ang_to(double a, double b, double rate);                           /* js: angTo */

/* weapons.c */
void shoot(Game *g);
void molotovs_step(Game *g, double dt);
void bullets_step(Game *g, double dt);
void fires_damage(Game *g, double dt, int *killed);

/* robot.c */
int clear_shot(double x1, double y1, double x2, double y2);
void bolts_step(Game *g, double dt);
void robot_step(Game *g, double dt);

/* storage.c */
void storage_load_best(Game *g);
void save_best(Game *g);                  /* js: saveBest */

/* key ids for game_key_down (edge events) */
enum { GK_R, GK_ENTER, GK_SPACE, GK_M, GK_DIGIT1, GK_DIGIT2, GK_DIGIT3 };
#endif
