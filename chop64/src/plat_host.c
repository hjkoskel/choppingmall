/* plat_host.c - silent platform stubs for host-side tests; counts calls. */
#include "plat.h"
unsigned long host_sfx_calls, host_jingles, host_new_games, host_clanks;
void snd_sfx(i16 f1, u8 frames, u8 wave, i16 f2) { (void)f1; (void)frames; (void)wave; (void)f2; host_sfx_calls++; }
void snd_clank(void) { host_clanks++; }
void snd_jingle(u8 kind) { (void)kind; host_jingles++; }
void snd_new_game(void) { host_new_games++; }
