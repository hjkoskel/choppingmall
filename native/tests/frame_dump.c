#define _POSIX_C_SOURCE 200809L
/* frame_dump.c - replays recorded inputs through the C game, renders every frame through
 * the full c2d pipeline and writes composited PNGs at the requested frames.
 * usage: frame_dump inputs seed outdir frame[,frame...] [scale]            */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <plutovg.h>
#include "game.h"
#include "render.h"

static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000.0 + t.tv_nsec / 1e6; }

int main(int argc, char **argv) {
  if (argc < 5) { fprintf(stderr, "usage\n"); return 2; }
  int k = argc > 5 ? atoi(argv[5]) : 1;
  FILE *f = fopen(argv[1], "r");
  int n = 0, cap = 1024, *in = malloc(sizeof(int) * cap), v;
  while (fscanf(f, "%d", &v) == 1) { if (n == cap) in = realloc(in, sizeof(int) * (cap *= 2)); in[n++] = v; }
  fclose(f);
  int want[256], nw = 0;
  for (char *s = strtok(argv[4], ","); s; s = strtok(NULL, ",")) want[nw++] = atoi(s);
  static Game G;
  game_init(&G, (uint32_t)strtoul(argv[2], NULL, 10));
  G.best.valid = 0;
  c2d *bg = c2d_create(SCREEN_W, SCREEN_H, k, 1, 0);
  c2d *fr = c2d_create(SCREEN_W, SCREEN_H, k, FRAME_LAYERS, 1);
  build_bg(bg);
  uint8_t *rgba = malloc((size_t)SCREEN_W * k * SCREEN_H * k * 4);
  double tsum = 0, tmax = 0;
  int cnt = 0;
  for (int i = 0; i < n; i++) {
    int m = in[i];
    G.K.ArrowUp = !!(m & 1); G.K.ArrowDown = !!(m & 2); G.K.ArrowLeft = !!(m & 4); G.K.ArrowRight = !!(m & 8); G.K.Space = !!(m & 16);
    if (m & 32) game_key_down(&G, GK_DIGIT1);
    if (m & 64) game_key_down(&G, GK_DIGIT2);
    if (m & 128) game_key_down(&G, GK_DIGIT3);
    if (m & 256) game_key_down(&G, GK_R);
    if (m & 512) game_key_down(&G, GK_ENTER);
    if (m & 1024) for (int a = 0; a < 3; a++) G.S.ammo[a] = MAXA[a];
    if ((m & 2048) && G.R.hp > 0.5) G.R.hp = 0.5;
    update(&G, 1.0 / 60);
    double sx, sy;
    game_pre_draw(&G, &sx, &sy);
    double t0 = now_ms();
    draw_frame(fr, &G);
    double dt = now_ms() - t0;
    tsum += dt; cnt++; if (dt > tmax) tmax = dt;
    for (int w = 0; w < nw; w++)
      if (want[w] == i) {
        c2d_compose_frame(bg, fr, LAYER_W, LAYER_X, LAYER_H, sx * k, sy * k, rgba);
        plutovg_surface_t *s = plutovg_surface_create_for_data(rgba, SCREEN_W * k, SCREEN_H * k, SCREEN_W * k * 4);
        /* rgba holds R,G,B,A bytes; PlutoVG writes BGRA premultiplied, so swap for the PNG writer */
        for (int p = 0; p < SCREEN_W * k * SCREEN_H * k; p++) { uint8_t t = rgba[p * 4]; rgba[p * 4] = rgba[p * 4 + 2]; rgba[p * 4 + 2] = t; }
        char path[512];
        snprintf(path, sizeof path, "%s/c%d.png", argv[3], i);
        plutovg_surface_write_to_png(s, path);
        for (int p = 0; p < SCREEN_W * k * SCREEN_H * k; p++) { uint8_t t = rgba[p * 4]; rgba[p * 4] = rgba[p * 4 + 2]; rgba[p * 4 + 2] = t; }
        plutovg_surface_destroy(s);
        printf("frame %d mode=%d shake=(%.2f,%.2f) nparts=%d\n", i, G.S.mode, sx, sy, G.nparts);
      }
  }
  free(rgba); free(in);
  c2d_destroy(bg);
  c2d_destroy(fr);
  printf("draw_frame: avg %.2f ms, max %.2f ms over %d frames (scale %d)\n", tsum / cnt, tmax, cnt, k);
  return 0;
}
