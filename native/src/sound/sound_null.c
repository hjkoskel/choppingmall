/* sound_null.c - silent backend used by logic tests (no audio engine linked). */
#include "sound.h"
void snd_sfx(double f, double d, SndWave type, double v, double f2) { (void)f; (void)d; (void)type; (void)v; (void)f2; }
void snd_clank(double v) { (void)v; }
void snd_play_jingle(JingleKind kind) { (void)kind; }
void snd_new_game(void) {}
void snd_engine(double engV, double engF) { (void)engV; (void)engF; }
void snd_toggle_mute(void) {}
