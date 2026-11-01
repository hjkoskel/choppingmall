/* parity_nes.c - host-vs-NES parity: the bot plays N ticks, the whole state is folded into a 16-bit hash every tick;
 * fceux (Lua) reads par_acc / par_ticks out of RAM. Same arithmetic as c64/tests/hash_host.c. */
#include "bot.h"
#include "core.h"
#include "plat.h"
#ifndef PAR_TICKS
#define PAR_TICKS 600
#endif
u16 par_acc; u16 par_ticks; u8 par_done;
u8 par_maxd, par_hist[4];
extern volatile u8 nmi_count;
#pragma zpsym("nmi_count")
void snd_sfx(i16 a, u8 b, u8 c, i16 d) { (void)a; (void)b; (void)c; (void)d; }
void snd_clank(void) {}
void snd_jingle(u8 k) { (void)k; }
void snd_new_game(void) {}
void main(void) {
  u8 in, ev;
  game_init(1);
#ifdef PAR_WORST /* worst-case tick: bot plays; only game_tick is timed, in video frames (NMI count): par_hist[d] = ticks that took d frames */
  *(volatile u8 *)0x2000 = 0x80;
  while (par_ticks < PAR_TICKS) { u8 c, d; in = bot_input(&ev); c = nmi_count; game_tick(in, ev); d = (u8)(nmi_count - c); if (d > 3) d = 3; par_hist[d]++; if (d > par_maxd) par_maxd = d; par_ticks++; }
#elif defined(PAR_TIMING) /* game_tick only (no bot, no hash): frames needed = cost of the core per tick; input is a fixed pattern */
  while (par_ticks < PAR_TICKS) { in = (u8)(((par_ticks >> 4) & 15) | IN_FIRE); ev = 0; if (G.mode == M_TITLE || G.mode == M_OVER) ev = EV_START; game_tick(in, ev); par_ticks++; }
#else
  while (par_ticks < PAR_TICKS) { in = bot_input(&ev); game_tick(in, ev); par_ticks++; par_acc = (u16)(par_acc * 31u + game_hash()); }
#endif
  par_done = 1;
  for (;;) {}
}
