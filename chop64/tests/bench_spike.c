/* bench_spike.c - finds the most expensive game tick in a chased-idle-player scenario and shows which sections of
 * the tick cost what (needs core.c built with -DCORE_PROF). */
#include <stdint.h>
#include <string.h>
#include "core.h"
#define POKE(a, v) (*(volatile u8 *)(a) = (v))
#define PEEK(a) (*(volatile u8 *)(a))
u16 prof_timer(void) { return (u16)(PEEK(0xDC04) | (PEEK(0xDC05) << 8)); }
static u16 cur[8], best[8];
void prof_rec(u8 k, u16 t0) { cur[k] = (u16)(t0 - prof_timer()); }
static void num(u8 row, u8 col, u16 v) {
  u8 *scr = (u8 *)0x0400 + row * 40 + col;
  scr[0] = (u8)('0' + v / 10000); scr[1] = (u8)('0' + (v / 1000) % 10); scr[2] = (u8)('0' + (v / 100) % 10); scr[3] = (u8)('0' + (v / 10) % 10); scr[4] = (u8)('0' + v % 10);
}
int main(void) {
  u16 t0, w, worst = 0, n, wt = 0;
  u8 k;
  __asm__("sei");
  POKE(0xDC0D, 0x7F);
  POKE(0xDC04, 0xFF); POKE(0xDC05, 0xFF); POKE(0xDC0E, 0x11); POKE(0xDC0E, 0x01);
  memset((u8 *)0x0400, 32, 1000);
  game_init(1);
  game_tick(0, EV_START);
  for (n = 0; n < 125; n++) game_tick(0, 0);
  for (n = 0; n < 600; n++) {
    for (k = 0; k < 8; k++) cur[k] = 0;
    t0 = prof_timer(); game_tick(0, 0); w = (u16)(t0 - prof_timer());
    if (G.mode == M_PLAY && w > worst) { worst = w; wt = n; for (k = 0; k < 8; k++) best[k] = cur[k]; }
    if (G.mode != M_PLAY) game_tick(0, 0);
  }
  num(1, 0, worst); num(1, 8, wt); num(1, 16, G.mode);
  for (k = 0; k < 7; k++) num((u8)(3 + k), 0, best[k]);
  for (;;) {}
}
