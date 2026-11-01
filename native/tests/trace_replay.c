/* trace_replay.c - replays a recorded input list through the C game logic and prints
 * state snapshots in the same key=value format as tools/golden/trace_js.js.
 * usage: trace_replay <inputs-file> <seed> [every=10] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"

static const char *mode_name(Mode m) {
  static const char *n[] = {"title", "intro", "play", "lost", "clear", "over", "win"};
  return n[m];
}
enum { B_UP = 1, B_DOWN = 2, B_LEFT = 4, B_RIGHT = 8, B_SPACE = 16, B_D1 = 32, B_D2 = 64, B_D3 = 128, B_R = 256, B_ENTER = 512, B_CHEAT = 1024, B_CHEAT2 = 2048 };

static Game G;

static void snap(int frame) {
  const Game *g = &G;
  printf("f=%d mode=%s mt=%.12g score=%.12g lives=%d level=%d time=%.12g lt=%.12g kills=%d w=%d a0=%d a1=%d a2=%d "
         "c0=%.12g c3=%.12g c5=%.12g tt=%.12g shake=%.12g px=%.12g py=%.12g php=%.12g pface=%.12g paim=%.12g pcd=%.12g "
         "pinv=%.12g pflash=%.12g rx=%.12g ry=%.12g rhp=%.12g rface=%.12g rslow=%.12g rcd=%.12g rpause=%.12g rdir=%g "
         "rdead=%d rsh=%.12g rwind=%.12g rck=%.12g rrev=%.12g rside=%g rstk=%.12g nbul=%d nrb=%d nmol=%d nfire=%d "
         "nparts=%d T=%.12g\n",
         frame, mode_name(g->S.mode), g->S.mt, g->S.score, g->S.lives, g->S.level, g->S.time, g->S.lt, g->S.kills,
         g->S.w, g->S.ammo[0], g->S.ammo[1], g->S.ammo[2], g->S.cool[0], g->S.cool[3], g->S.cool[5], g->S.tt,
         g->S.shake, g->P.x, g->P.y, g->P.hp, g->P.face, g->P.aim, g->P.cd, g->P.inv, g->P.flash, g->R.x, g->R.y,
         g->R.hp, g->R.face, g->R.slow, g->R.cd, g->R.pause, g->R.dir, g->R.dead, g->R.sh, g->R.wind, g->R.ck,
         g->R.rev, g->R.side, g->R.stk, g->nbul, g->nrb, g->nmols, g->nfires, g->nparts, g->T);
}

int main(int argc, char **argv) {
  if (argc < 3) { fprintf(stderr, "usage: %s inputs seed [every]\n", argv[0]); return 2; }
  int every = argc > 3 ? atoi(argv[3]) : 10;
  FILE *f = fopen(argv[1], "r");
  if (!f) { perror("inputs"); return 2; }
  int n = 0, cap = 1024, *in = malloc(sizeof(int) * cap), v;
  while (fscanf(f, "%d", &v) == 1) {
    if (n == cap) in = realloc(in, sizeof(int) * (cap *= 2));
    in[n++] = v;
  }
  fclose(f);
  game_init(&G, (uint32_t)strtoul(argv[2], NULL, 10));
  G.best.valid = 0; /* traces never depend on the saved best */
  const double dt = 1.0 / 60;
  printf("# C replay frames=%d\n", n);
  for (int fr = 0; fr < n; fr++) {
    int m = in[fr];
    G.K.ArrowUp = !!(m & B_UP); G.K.ArrowDown = !!(m & B_DOWN);
    G.K.ArrowLeft = !!(m & B_LEFT); G.K.ArrowRight = !!(m & B_RIGHT); G.K.Space = !!(m & B_SPACE);
    if (m & B_D1) game_key_down(&G, GK_DIGIT1);
    if (m & B_D2) game_key_down(&G, GK_DIGIT2);
    if (m & B_D3) game_key_down(&G, GK_DIGIT3);
    if (m & B_R) game_key_down(&G, GK_R);
    if (m & B_ENTER) game_key_down(&G, GK_ENTER);
    if (m & B_CHEAT) { for (int i = 0; i < 3; i++) G.S.ammo[i] = MAXA[i]; }
    if (m & B_CHEAT2) { if (G.R.hp > 0.5) G.R.hp = 0.5; }
    update(&G, dt);
    double sx, sy;
    game_pre_draw(&G, &sx, &sy); /* js: draw() side effects */
    if (fr % every == 0 || fr == n - 1) snap(fr);
  }
  free(in);
  return 0;
}
