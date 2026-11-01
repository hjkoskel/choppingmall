/* bg_dump.c - renders the static background to a PNG (and prints timing). usage: bg_dump out.png [scale] */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "render.h"
#include "world.h"
int main(int argc, char **argv) {
  int k = argc > 2 ? atoi(argv[2]) : 1;
  world_init();
  c2d *c = c2d_create(SCREEN_W, SCREEN_H, k, 1, 0);
  clock_t t0 = clock();
  build_bg(c);
  printf("build_bg scale=%d: %.1f ms\n", k, (double)(clock() - t0) * 1000 / CLOCKS_PER_SEC);
  c2d_layer_write_png(c, 0, argv[1]);
  c2d_destroy(c);
  return 0;
}
