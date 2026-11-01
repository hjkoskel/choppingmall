/* sound.h - the game-facing audio API. The game thread only queues requests;
 * everything is synthesised on the audio thread (see wa/ and sound/). */
#ifndef SOUND_H
#define SOUND_H

typedef enum { SND_SINE, SND_SQUARE, SND_SAWTOOTH, SND_TRIANGLE } SndWave;
typedef enum { JINGLE_WIN, JINGLE_LOSE } JingleKind;

void snd_sfx(double f, double d, SndWave type, double v, double f2); /* js: sfx */
void snd_clank(double v);                                            /* js: clank */
void snd_play_jingle(JingleKind kind);                               /* js: playJingle */
void snd_new_game(void);   /* js: startMusic(); jingleStop(); if (mus.paused) mlResume(); */
void snd_engine(double engV, double engF); /* js: engineApply */
void snd_toggle_mute(void);                /* js: KeyM handler */
#endif
