/* hud.c - js: fmt, hud (top bar, weapon bar, hints, banners, title and end screens).
 * Desktop build: touchUI is always false. */
#include <math.h>
#include <stdio.h>
#include "render.h"
#include "jsmath.h"
#include "world.h"

#define FILL(css) c2d_fill_style(c, c2d_color(css))
#define STROKE(css) c2d_stroke_style(c, c2d_color(css))
#define L C2D_ALIGN_LEFT
#define CT C2D_ALIGN_CENTER

/* js: fmt - mm:ss */
static void fmt(char *out, size_t n, double s) {
  snprintf(out, n, "%02d:%02d", (int)floor(s / 60), (int)floor(fmod(s, 60)));
}

static void band(c2d *c, const char *a, const char *b, const char *col) {
  FILL("rgba(8,6,14,.7)");
  c2d_fill_rect(c, 0, 300, SCREEN_W, 170);
  txt(c, a, SCREEN_W / 2, 360, 56, col, CT, 1);
  if (b) txt(c, b, SCREEN_W / 2, 425, 20, "#d8d2ee", CT, 0);
}

/* js: hud */
void draw_hud(c2d *c, const Game *g) {
  const State *S = &g->S;
  const Player *P = &g->P;
  const Best *best = &g->best;
  char buf[256], t1[32];
  FILL("rgba(10,8,16,.9)");
  c2d_fill_rect(c, 0, 0, SCREEN_W, 80);
  FILL("#c9a24b");
  c2d_fill_rect(c, 0, 78, SCREEN_W, 2);
  txt(c, "SCORE", 24, 24, 13, "#9a93b0", L, 1);
  snprintf(buf, sizeof buf, "%06ld", (long)floor(S->score));
  txt(c, buf, 24, 50, 30, "#ffd45c", L, 1);
  txt(c, "LEVEL", 200, 24, 13, "#9a93b0", L, 1);
  snprintf(buf, sizeof buf, "%d / 3", S->level);
  txt(c, buf, 200, 50, 26, "#fff", L, 1);
  txt(c, "LIVES", 310, 24, 13, "#9a93b0", L, 1);
  for (int i = 0; i < 3; i++)
    txt(c, "\xe2\x99\xa5", 312 + i * 28, 52, 28, i < S->lives ? "#ff4466" : "#3a3345", L, 1);
  txt(c, "HEALTH", 430, 24, 13, "#9a93b0", L, 1);
  FILL("#2a2535");
  c2d_fill_rect(c, 430, 40, 150, 18);
  FILL(P->hp > 35 ? "#4ad07a" : "#e23b3b");
  c2d_fill_rect(c, 430, 40, (150 * P->hp) / 100, 18);
  STROKE("#6a6285");
  c2d_stroke_rect(c, 430, 40, 150, 18);
  txt(c, "TIME", 620, 24, 13, "#9a93b0", L, 1);
  fmt(buf, sizeof buf, S->time);
  txt(c, buf, 620, 50, 26, "#fff", L, 1);
  txt(c, "ROBOTS", 710, 24, 13, "#9a93b0", L, 1);
  snprintf(buf, sizeof buf, "%d", S->kills);
  txt(c, buf, 710, 50, 26, "#fff", L, 1);
  txt(c, "BEST", 800, 24, 13, "#9a93b0", L, 1);
  if (best->valid) {
    snprintf(buf, sizeof buf, "%06ld", best->score);
    txt(c, buf, 800, 45, 16, "#ffd45c", L, 1);
    fmt(t1, sizeof t1, (double)best->time);
    snprintf(buf, sizeof buf, "%s \xc2\xb7 %ld bots", t1, best->kills);
    txt(c, buf, 800, 63, 12, "#b9b2cf", L, 1);
  } else txt(c, "\xe2\x80\x94", 800, 50, 20, "#555", L, 1);
  /* weapon bar */
  for (int i = 0; i < 3; i++) {
    double x = SCREEN_W / 2.0 - 195 + i * 135;
    int sel = S->w == i, low = S->ammo[i] == 0;
    FILL(sel ? "rgba(255,212,92,.2)" : "rgba(10,8,16,.8)");
    rr(c, x, 712, 125, 40, 8);
    c2d_fill(c);
    STROKE(sel ? "#ffd45c" : "#4a4560");
    c2d_line_width(c, sel ? 3 : 1.5);
    c2d_stroke(c);
    snprintf(buf, sizeof buf, "%d", i + 1);
    txt(c, buf, x + 14, 732, 20, sel ? "#ffd45c" : "#8d86a6", CT, 1);
    txt(c, WN[i], x + 28, 725, 11, "#cfc9e6", L, 1);
    snprintf(buf, sizeof buf, "\xc3\x97 %d", S->ammo[i]);
    txt(c, buf, x + 28, 741, 15, low ? "#ff5a5a" : "#fff", L, 1);
  }
  txt(c, "WASD / ARROWS move   \xc2\xb7   hold SPACE to stop, aim & fire   \xc2\xb7   1 2 3 weapons   \xc2\xb7   $ doors restock   \xc2\xb7   M mute",
      SCREEN_W / 2, 696, 12, "#a89fc7", CT, 0);
  if (S->tt > 0 && S->mode == MODE_PLAY) {
    c2d_global_alpha(c, js_min(1, S->tt));
    txt(c, S->toast, SCREEN_W / 2, 112, 22, "#fff", CT, 1);
    c2d_global_alpha(c, 1);
  }
  if (S->mode == MODE_INTRO) {
    char sub[160];
    snprintf(buf, sizeof buf, "LEVEL %d", S->level);
    if (S->level == 1) snprintf(sub, sizeof sub, "You are unarmed \xe2\x80\x94 grab ammo from a shop ($) before the robot gets you!");
    else snprintf(sub, sizeof sub, "Robot #%d is online \xe2\x80\x94 stay moving, find a gap, then shoot.", S->level);
    band(c, buf, sub, "#ffd45c");
  } else if (S->mode == MODE_LOST) {
    char a[32];
    snprintf(a, sizeof a, "%d left", S->lives);
    band(c, "LIFE LOST", a, "#ff4466");
  } else if (S->mode == MODE_CLEAR) {
    snprintf(buf, sizeof buf, "+%d and a speed bonus", 1000 * S->level);
    band(c, "ROBOT DESTROYED", buf, "#4ad07a");
  } else if (S->mode == MODE_OVER || S->mode == MODE_WIN || S->mode == MODE_TITLE) {
    FILL("rgba(8,6,14,.82)");
    c2d_fill_rect(c, 0, 80, SCREEN_W, 680);
    if (S->mode == MODE_TITLE) {
      c2d_shadow(c, "#ff2a8a", 24);
      txt(c, "CHOPPING", SCREEN_W / 2, 215, 92, "#ff5ca8", CT, 1);
      c2d_shadow(c, "#1ab0ff", 24);
      txt(c, "MALL", SCREEN_W / 2, 305, 92, "#5cd6ff", CT, 1);
      c2d_shadow(c, NULL, 0);
      txt(c, "A killer robot is loose in the mall. Survive three levels.", SCREEN_W / 2, 385, 20, "#e6e1f7", CT, 0);
      static const char *const lines[6] = {
          "WASD / ARROWS \xe2\x80\x94 move",
          "SPACE \xe2\x80\x94 stop, aim and fire",
          "1 PISTOL   2 SHOTGUN   3 MOLOTOV",
          "You start with NO ammo \xe2\x80\x94 get it at the shop doors ($)",
          "Fire burns the robot \xe2\x80\x94 and you",
          "The robot fires energy bolts \xe2\x80\x94 dodge them or break line of sight",
      };
      for (int i = 0; i < 6; i++) txt(c, lines[i], SCREEN_W / 2, 440 + i * 28, 17, "#b9b2cf", CT, 0);
      txt(c, "PRESS ENTER OR SPACE TO START", SCREEN_W / 2, 625, 26, "#ffd45c", CT, 1);
    } else {
      txt(c, S->mode == MODE_WIN ? "MALL SECURED" : "GAME OVER", SCREEN_W / 2, 230, 72,
          S->mode == MODE_WIN ? "#4ad07a" : "#ff4466", CT, 1);
      snprintf(buf, sizeof buf, "SCORE %06ld", (long)floor(S->score));
      txt(c, buf, SCREEN_W / 2, 320, 38, "#ffd45c", CT, 1);
      fmt(t1, sizeof t1, S->time);
      snprintf(buf, sizeof buf, "Time %s   \xc2\xb7   Robots destroyed %d / 3", t1, S->kills);
      txt(c, buf, SCREEN_W / 2, 375, 22, "#e6e1f7", CT, 0);
      if (best->valid) {
        char t2[32];
        fmt(t2, sizeof t2, (double)best->time);
        snprintf(buf, sizeof buf, "BEST  %06ld  \xc2\xb7  %s  \xc2\xb7  %ld robots", best->score, t2, best->kills);
        txt(c, buf, SCREEN_W / 2, 425, 20, "#b9b2cf", CT, 0);
      }
      txt(c, "PRESS R TO PLAY AGAIN", SCREEN_W / 2, 520, 30, "#fff", CT, 1);
    }
  }
}
