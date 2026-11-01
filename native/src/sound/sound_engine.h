/* sound_engine.h - lifecycle of the real audio backend (see sound.h for the game-facing API). */
#ifndef SOUND_ENGINE_H
#define SOUND_ENGINE_H
#include "wa/wa.h"

int snd_engine_init(int sample_rate);          /* returns 0 on failure */
void snd_engine_shutdown(void);
/* Audio-thread entry point (also used for offline rendering): fills interleaved stereo float. */
void snd_engine_render(float *stereo, int frames);
const wa *snd_engine_wa(void);                 /* diagnostics */
int snd_dropped_commands(void);                /* commands lost because the ring was full */
#endif
