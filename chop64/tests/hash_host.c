/* hash_host.c - host reference for the C64 parity test: plays the bot for N ticks and prints the same running hash
 * the DEBUG_HASH C64 build computes. usage: hash_host N */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bot.h"
#include "core.h"
int main(int argc, char **argv) {
  unsigned long n = argc > 1 ? strtoul(argv[1], 0, 10) : 1000, t;
  u16 acc = 0;
  u8 in, ev;
  memset(&G, 0, sizeof G);
  game_init(1);
  for (t = 0; t < n; t++) { in = bot_input(&ev); game_tick(in, ev); acc = (u16)(acc * 31u + game_hash()); }
  printf("%u %lu score=%lu level=%d kills=%d mode=%d\n", acc, n, (unsigned long)G.score, G.level, G.kills, G.mode);
  return 0;
}
