/* music.h - js: the "Dm / Bb loop" chiptune sequencer (mlTone .. mlPump). Audio thread only. */
#ifndef MUSIC_H
#define MUSIC_H
#include "wa/wa.h"

#define ML_BARS 12
extern const char *const ml_lead1[ML_BARS];
extern const char *const ml_lead2[ML_BARS];
extern const char *const ml_bass[ML_BARS];
extern const char *const ml_drums[ML_BARS];

typedef struct {
  int on, paused;
  wa_id bus;       /* js: mus.gain */
  double resume_at, next;
  int step;
} Music;

void music_start(Music *m, wa *w, int mute);        /* js: startMusic */
void music_pump(Music *m, wa *w, int mute);         /* js: mlPump (lookahead = this block) */
void music_pause(Music *m, wa *w, double dur);      /* js: mlPause */
void music_resume(Music *m, wa *w, int mute);       /* js: mlResume */
#endif
