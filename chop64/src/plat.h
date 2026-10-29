/* plat.h - what the portable core needs from the platform (implemented by plat_c64.c and plat_host.c). */
#ifndef PLAT_H
#define PLAT_H
#include "types.h"
enum { W_SINE, W_SQUARE, W_SAW, W_TRI };
enum { JINGLE_WIN, JINGLE_LOSE };
/* js: sfx(f, d, type, v, f2), with the duration in frames (50 per second) */
void snd_sfx(i16 f1_hz, u8 frames, u8 wave, i16 f2_hz);
void snd_clank(void);
void snd_jingle(u8 kind);
void snd_new_game(void); /* starts the music loop, stops a jingle */
#endif
