/* bench_c64.c - times core functions on the C64 (CIA1 timer A = cycle counter); shows results on the text screen. */
#include <stdint.h>
#include <string.h>
#include "assets_gen.h"
#include "core.h"
#define POKE(a, v) (*(volatile u8 *)(a) = (v))
#define PEEK(a) (*(volatile u8 *)(a))
static u16 timer(void) { return (u16)(PEEK(0xDC04) | (PEEK(0xDC05) << 8)); }
static void show(u8 row, const char *label, u16 v) {
  u8 *scr = (u8 *)0x0400 + row * 40;
  u8 i = 0;
  char buf[6];
  while (label[i]) { scr[i] = (u8)((u8)label[i] >= 193 ? (u8)label[i] - 192 : label[i]); i++; }
  buf[5] = 0;
  for (i = 5; i > 0; i--) { buf[i - 1] = (char)('0' + v % 10); v /= 10; }
  for (i = 0; i < 5; i++) scr[20 + i] = (u8)buf[i];
}
volatile u8 sink;
int main(void) {
  u16 t0, t1, k, n;
  u8 ev = 0;
  i16 wx, wy; i8 dir = 1;
  __asm__("sei");
  POKE(0xDC0D, 0x7F);
  POKE(0xDC04, 0xFF); POKE(0xDC05, 0xFF); POKE(0xDC0E, 0x11); POKE(0xDC0E, 0x01);
  memset((u8 *)0x0400, 32, 1000);
  game_init(1);
  /* valid(): typical corridor point, 20 calls */
  t0 = timer(); for (k = 0; k < 20; k++) sink = valid((222 + (k & 3)) << 4, 64 << 4, 4); t1 = timer(); show(1, "VALID", (t0 - t1) / 20);
  t0 = timer(); for (k = 0; k < 20; k++) sink = ang32((i16)k - 7, 33); t1 = timer(); show(2, "ANG32", (t0 - t1) / 20);
  t0 = timer(); for (k = 0; k < 20; k++) sink = ang32(100, (i16)k - 7); t1 = timer(); show(7, "ANG32 BIG", (t0 - t1) / 20);
  t0 = timer(); for (k = 0; k < 20; k++) sink = mdot((i16)k, 33); t1 = timer(); show(8, "MDOT", (t0 - t1) / 20);
  t0 = timer(); for (k = 0; k < 20; k++) sink = (u8)k; t1 = timer(); show(9, "EMPTY LOOP", (t0 - t1) / 20);
  t0 = timer(); for (k = 0; k < 4; k++) sink = clear_shot(222, 64, 98, 136); t1 = timer(); show(3, "CLEARSHOT", (t0 - t1) / 4);
  t0 = timer(); for (k = 0; k < 4; k++) sink = nav_target(98, 136, 222, 64, &dir, &wx, &wy); t1 = timer(); show(4, "NAVTARGET", (t0 - t1) / 4);
  /* whole game ticks, no profiling hooks: (A) idle player chased by the robot, (B) player running along the corridor */
  { u16 w, mx = 0, over = 0, ph[16]; u32 sum = 0; u8 pass, in, q, nsp = 0;
    for (pass = 0; pass < 2; pass++) {
      memset(&G, 0, sizeof G); game_init(1); core_set_walkmap(ASSET_WALK); mx = 0; over = 0; sum = 0; for (q = 0; q < 16; q++) ph[q] = 0;
      game_tick(0, EV_START);
      for (n = 0; n < 125; n++) game_tick(0, 0); /* through the intro */
      for (n = 0; n < 600; n++) {
        in = pass ? (u8)((n / 90) & 1 ? IN_LEFT : IN_RIGHT) | ((n / 60) & 1 ? IN_DOWN : 0) : 0;
        t0 = timer(); game_tick(in, 0); t1 = timer();
        w = (u16)(t0 - t1);
        sum += w; if (w > mx) mx = w; if (w > 17000) over++;
        if (G.mode == M_PLAY && w > ph[G.tick & 15]) ph[G.tick & 15] = w;
        if (!pass && w > 17000 && (G.tick & 3) != 0 && nsp < 6) { /* spike outside the periodic phases: record state flags */
          u8 *scr = (u8 *)0x0400 + (19 + nsp * 1) * 40 + 0;
          u16 st[8]; u8 z;
          st[0] = n; st[1] = w; st[2] = G.rwind; st[3] = (u16)(G.rrev ? 1 : 0); st[4] = G.rpause; st[5] = G.bolt.life; st[6] = G.pinv; st[7] = G.rstk;
          for (z = 0; z < 8; z++) { u16 v = st[z]; scr[z * 5] = (u8)('0' + (v / 10000) % 10); scr[z*5+1] = (u8)('0' + (v / 1000) % 10); scr[z*5+2] = (u8)('0' + (v / 100) % 10); scr[z*5+3] = (u8)('0' + (v / 10) % 10); scr[z*5+4] = (u8)('0' + v % 10); }
          nsp++;
        }
        if (G.mode != M_PLAY) { game_tick(0, 0); }
      }
      show((u8)(10 + pass * 4), pass ? "RUN AVG" : "IDLE AVG", (u16)(sum / 600));
      show((u8)(11 + pass * 4), pass ? "RUN MAX" : "IDLE MAX", mx);
      show((u8)(12 + pass * 4), pass ? "RUN >17000" : "IDLE >17000", over);
      show((u8)(13 + pass * 4), "MODE", G.mode);
      if (!pass) for (q = 0; q < 16; q++) { u8 *scr = (u8 *)0x0400 + (20 + q / 4) * 40 + (q % 4) * 9; u16 v = ph[q]; scr[0] = (u8)((q < 10 ? '0' + q : 1 + q - 10)); scr[1] = ':'; scr[2] = (u8)('0' + v / 10000); scr[3] = (u8)('0' + (v / 1000) % 10); scr[4] = (u8)('0' + (v / 100) % 10); scr[5] = (u8)('0' + (v / 10) % 10); scr[6] = (u8)('0' + v % 10); }
    }
  }
  for (;;) {}
}
