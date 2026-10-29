/* test_core.c - plays the portable core with the bot and checks invariants. gcc on the host. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bot.h"
#include "assets.h"
#include "core.h"
extern unsigned long host_sfx_calls, host_jingles, host_new_games;

static int fails, checks;
static unsigned all_modes = 1u << M_TITLE; /* union over every run; the title is shown before the first tick */
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; if (fails < 20) { printf("  FAIL (tick %lu): ", tick); printf(__VA_ARGS__); printf("\n"); } } } while (0)
static unsigned long tick;
static const u8 AMAX[3] = {60, 24, 6};

static void invariants(void) {
  int i;
  if (G.mode == M_PLAY || G.mode == M_INTRO || G.mode == M_LOST || G.mode == M_CLEAR) {
    CHECK(valid(G.px, G.py, 3), "player left the corridor at (%d,%d)", G.px >> FP, G.py >> FP);
    CHECK(valid(G.rx, G.ry, 4), "robot left the corridor at (%d,%d)", G.rx >> FP, G.ry >> FP);
  }
  CHECK(G.php <= 1000 && G.php > -600, "player health %d", G.php);
  CHECK(G.rhp <= G.rmax && G.rhp > -400, "robot health %d/%d", G.rhp, G.rmax);
  for (i = 0; i < 3; i++) CHECK(G.ammo[i] <= AMAX[i], "ammo[%d]=%d", i, G.ammo[i]);
  CHECK(G.pface < 32 && G.rface < 32, "face out of range");
  CHECK(G.level >= 1 && G.level <= 3 && G.lives <= 3, "level/lives %d/%d", G.level, G.lives);
  CHECK(G.mode <= M_WIN, "mode %d", G.mode);
  for (i = 0; i < 6; i++) CHECK(G.cool[i] <= 1100, "door cooldown %d", G.cool[i]);
}

typedef struct { int maxlevel, wins, overs, kills_total, losses; unsigned modes; u32 maxscore; } Stats;
static void run(u16 seed, unsigned long ticks, int use_bot, Stats *st) {
  u8 ev, in;
  int last_mode = -1, last_lives = 3;
  memset(&G, 0, sizeof G);
  game_init(seed);
  bot_reset();
  for (tick = 0; tick < ticks; tick++) {
    if (use_bot) in = bot_input(&ev); else { in = 0; ev = (G.mode == M_TITLE || ((G.mode == M_OVER || G.mode == M_WIN) && G.mt == 0)) ? EV_START : 0; }
    game_tick(in, ev);
    invariants();
    st->modes |= 1u << G.mode;
    all_modes |= 1u << G.mode;
    if (G.level > st->maxlevel) st->maxlevel = G.level;
    if (G.mode == M_WIN && last_mode != M_WIN) st->wins++;
    if (G.mode == M_OVER && last_mode != M_OVER) st->overs++;
    if (G.mode == M_LOST && last_mode != M_LOST) st->losses++;
    if (G.mode == M_CLEAR && last_mode != M_CLEAR) st->kills_total++;
    if (G.score > st->maxscore) st->maxscore = G.score;
    (void)last_lives;
    last_mode = G.mode;
  }
}

static Assets AS;
static unsigned char slow[340 * 230];
/* the build-time walkability bitmap must give exactly the answer of the slow valid(r = 5) everywhere */
static void test_walkmap(void) {
  int x, y, f, bad = 0;
  u16 h1, h2;
  game_init(1);
  assets_build(&AS);
  for (y = -8; y < 214; y++) for (x = -8; x < 330; x++) slow[(y + 8) * 340 + (x + 8)] = valid((i16)(x << FP), (i16)(y << FP), 5);
  core_set_walkmap(AS.walk);
  for (y = -8; y < 214; y++)
    for (x = -8; x < 330; x++)
      for (f = 0; f < 16; f += 7) {
        u8 a = valid((i16)((x << FP) + f), (i16)((y << FP) + f), 5);
        if (a != slow[(y + 8) * 340 + (x + 8)]) { if (bad++ < 5) printf("  walkmap differs at (%d,%d)+%d: map %d slow %d\n", x, y, f, a, slow[(y + 8) * 340 + (x + 8)]); }
      }
  CHECK(bad == 0, "walkability bitmap equals valid(r=5) at every pixel (%d differences)", bad);
  { /* a long bot run must produce the same state hash with and without the bitmap */
    u8 ev, in; unsigned long t; u16 acc[2]; int pass;
    for (pass = 0; pass < 2; pass++) {
      memset(&G, 0, sizeof G); game_init(1); bot_reset(); core_set_walkmap(pass ? AS.walk : 0); acc[pass] = 0;
      for (t = 0; t < 8000; t++) { in = bot_input(&ev); game_tick(in, ev); acc[pass] = (u16)(acc[pass] * 31u + game_hash()); }
    }
    h1 = acc[0]; h2 = acc[1];
    CHECK(h1 == h2, "8000 bot ticks: state hash %u (slow valid) vs %u (bitmap)", h1, h2);
  }
  core_set_walkmap(0);
}

int main(void) {
  Stats st;
  test_walkmap();
  u16 seeds[5] = {1, 2, 3, 4, 5};
  int i;
  /* 1. idle player: the robot must win; the game must cycle through LOST/OVER and restart */
  memset(&st, 0, sizeof st);
  run(1, 12000, 0, &st);
  CHECK(st.overs >= 1, "an idle player never reaches game over (overs=%d)", st.overs);
  CHECK(st.losses >= 2, "an idle player loses lives (%d)", st.losses);
  printf("idle player : overs=%d lives lost=%d modes=0x%02x\n", st.overs, st.losses, st.modes);
  /* 2. bot: must play all the way through (shops, weapons, fire, three levels) */
  memset(&st, 0, sizeof st);
  for (i = 0; i < 5; i++) run(seeds[i], 30000, 1, &st);
  printf("bot (5 runs x 30000 ticks): max level %d, robots destroyed %d, wins %d, game overs %d, lives lost %d, best score %lu, modes 0x%02x\n",
         st.maxlevel, st.kills_total, st.wins, st.overs, st.losses, (unsigned long)st.maxscore, st.modes);
  CHECK(st.kills_total >= 5, "the bot destroys robots (%d)", st.kills_total);
  CHECK(st.maxlevel == 3, "the bot reaches level 3 (%d)", st.maxlevel);
  /* The ammo limits are tight on purpose (max 15/8/6, 6/2/2 per shop visit): the simple bot is no longer expected to win, so the win
   * path is checked directly: clearing level 3 must lead to M_WIN, and a start event from there must begin a new game at level 1. */
  memset(&G, 0, sizeof G); game_init(1);
  G.level = 3; G.mode = M_CLEAR; G.mt = 1;
  game_tick(0, 0); all_modes |= 1u << G.mode;
  CHECK(G.mode == M_WIN, "clearing level 3 leads to the win screen (mode %d)", G.mode);
  { int t; for (t = 0; t < 70; t++) game_tick(0, 0); }
  game_tick(0, EV_START);
  CHECK(G.level == 1 && G.mode != M_WIN, "after the win screen a new game starts at level 1 (level %d mode %d)", G.level, G.mode);
  CHECK(all_modes == (1u << (M_WIN + 1)) - 1, "every game mode was visited: 0x%02x", all_modes);
  CHECK(host_sfx_calls > 100 && host_jingles >= 2 && host_new_games >= 2, "sound hooks fired (%lu sfx, %lu jingles)", host_sfx_calls, host_jingles);
  printf("core: %d checks, %d failed\n", checks, fails);
  return fails != 0;
}
