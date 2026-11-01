/* music.c - js: mlFreq mlTone mlNoise mlKick mlDrum mlSlot mlPump mlPause mlResume startMusic. */
#include "music.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define ML_BPM 224.0
#define ML_E (60.0 / ML_BPM / 2) /* js: ML.e, one eighth note */
#define BUS_GAIN 0.45

/* js: mlFreq */
static double ml_freq(const char *n) {
  static const int semi[7] = {9, 11, 0, 2, 4, 5, 7}; /* A..G */
  int acc = 0;
  const char *p = n + 1;
  if (*p == '#') { acc = 1; p++; } else if (*p == 'b') { acc = -1; p++; }
  int oct = atoi(p);
  return 440.0 * pow(2.0, (semi[n[0] - 'A'] + acc + 12 * (oct + 1) - 69) / 12.0);
}

/* js: mlTone */
static void ml_tone(Music *m, wa *w, double t, double dur, double hz, wa_wave type, double vol, double cut) {
  wa_id o = wa_osc(w, type), amp = wa_gain(w, 1), lp = wa_biquad(w, WA_LOWPASS);
  wa_set_value(w, o, WA_P_FREQ, hz);
  wa_set_value(w, lp, WA_P_FREQ, cut);
  wa_connect(w, o, lp);
  wa_set_at(w, amp, WA_P_GAIN, 0, t);
  wa_linear_to(w, amp, WA_P_GAIN, vol, t + 0.005);
  wa_exp_to(w, amp, WA_P_GAIN, 0.001, t + dur);
  wa_connect(w, lp, amp);
  wa_connect(w, amp, m->bus);
  wa_start(w, o, t);
  wa_stop(w, o, t + dur + 0.02);
}

/* js: mlNoise */
static void ml_noise(Music *m, wa *w, double t, double dur, double hp, double vol) {
  wa_id src = wa_noise(w, dur, 0), f = wa_biquad(w, WA_HIGHPASS), amp = wa_gain(w, 1);
  wa_set_value(w, f, WA_P_FREQ, hp);
  wa_set_at(w, amp, WA_P_GAIN, vol, t);
  wa_exp_to(w, amp, WA_P_GAIN, 0.001, t + dur);
  wa_connect(w, src, f);
  wa_connect(w, f, amp);
  wa_connect(w, amp, m->bus);
  wa_start(w, src, t);
}

/* js: mlKick */
static void ml_kick(Music *m, wa *w, double t) {
  wa_id o = wa_osc(w, WA_SINE), amp = wa_gain(w, 1);
  wa_set_at(w, o, WA_P_FREQ, 150, t);
  wa_exp_to(w, o, WA_P_FREQ, 40, t + 0.12);
  wa_set_at(w, amp, WA_P_GAIN, 0.9, t);
  wa_exp_to(w, amp, WA_P_GAIN, 0.001, t + 0.2);
  wa_connect(w, o, amp);
  wa_connect(w, amp, m->bus);
  wa_start(w, o, t);
  wa_stop(w, o, t + 0.25);
}

/* js: mlDrum -- h = hat, s = snare, k = kick, c = crash */
static void ml_drum(Music *m, wa *w, double t, const char *tok) {
  if (strchr(tok, 'h')) ml_noise(m, w, t, 0.05, 7000, 0.18);
  if (strchr(tok, 's')) ml_noise(m, w, t, 0.14, 1500, 0.4);
  if (strchr(tok, 'k')) ml_kick(m, w, t);
  if (strchr(tok, 'c')) ml_noise(m, w, t, 0.9, 5000, 0.25);
}

/* the i-th space separated token of a bar */
static int token(const char *bar, int i, char *out, size_t cap) {
  int k = 0;
  const char *p = bar;
  while (*p) {
    const char *q = p;
    while (*q && *q != ' ') q++;
    if (k == i) {
      size_t n = (size_t)(q - p);
      if (n >= cap) n = cap - 1;
      memcpy(out, p, n);
      out[n] = 0;
      return 1;
    }
    k++;
    p = *q ? q + 1 : q;
  }
  return 0;
}

/* js: mlSlot -- schedules every track of one eighth-note slot (0..95) at time t */
static void ml_slot(Music *m, wa *w, int step, double t) {
  int b = step >> 3, i = step & 7;
  char tok[32], note[8];
  struct { const char *const *bars; int kind; } tr[4] = {{ml_lead1, 0}, {ml_lead2, 1}, {ml_bass, 2}, {ml_drums, 3}};
  for (int k = 0; k < 4; k++) {
    if (!token(tr[k].bars[b], i, tok, sizeof tok)) continue;
    if (strcmp(tok, "-") == 0) continue;
    if (tr[k].kind == 3) { ml_drum(m, w, t, tok); continue; }
    for (char *p = tok; *p;) {
      char *q = p;
      while (*q && *q != '+') q++;
      size_t n = (size_t)(q - p);
      if (n >= sizeof note) n = sizeof note - 1;
      memcpy(note, p, n);
      note[n] = 0;
      double hz = ml_freq(note);
      if (tr[k].kind == 0) ml_tone(m, w, t, ML_E * 0.9, hz, WA_SQUARE, 0.07, 3000);
      else if (tr[k].kind == 1) ml_tone(m, w, t, ML_E * 0.9, hz, WA_TRIANGLE, 0.07, 3000);
      else ml_tone(m, w, t, ML_E * 0.95, hz, WA_SAWTOOTH, 0.3, 500);
      p = *q ? q + 1 : q;
    }
  }
}

/* js: mlPump. The original schedules 1 s ahead from a 150 ms timer; here the sequencer
 * runs inside the audio block hook, so it only needs to schedule the slots that start
 * before the end of the current block, at their exact times. */
void music_pump(Music *m, wa *w, int mute) {
  if (!m->on) return;
  double now = wa_now(w), block_end = now + (double)WA_BLOCK / wa_sample_rate(w);
  if (m->paused) {
    if (now >= m->resume_at) music_resume(m, w, mute);
    else return;
  }
  if (m->next < now) m->next = now + 0.05;
  while (m->next < block_end) {
    ml_slot(m, w, m->step, m->next);
    m->next += ML_E;
    m->step = (m->step + 1) % 96;
  }
}

/* js: mlPause -- silences the loop and rewinds to the notes scheduled but not yet heard */
void music_pause(Music *m, wa *w, double dur) {
  if (!m->on) return;
  double now = wa_now(w);
  if (!m->paused) {
    int back = (int)fmax(0, ceil((m->next - now) / ML_E) - 1);
    m->step = (((m->step - back) % 96) + 96) % 96;
    wa_cancel(w, m->bus, WA_P_GAIN, now);
    wa_set_at(w, m->bus, WA_P_GAIN, 0, now);
    m->paused = 1;
  }
  m->resume_at = now + dur;
}

/* js: mlResume */
void music_resume(Music *m, wa *w, int mute) {
  m->paused = 0;
  double now = wa_now(w);
  wa_cancel(w, m->bus, WA_P_GAIN, now);
  wa_set_at(w, m->bus, WA_P_GAIN, mute ? 0 : BUS_GAIN, now);
  m->next = now + 0.1;
}

/* js: startMusic */
void music_start(Music *m, wa *w, int mute) {
  if (m->on) return;
  m->on = 1;
  m->bus = wa_gain(w, mute ? 0 : BUS_GAIN);
  wa_pin(w, m->bus);
  wa_connect(w, m->bus, wa_destination(w));
  m->next = wa_now(w) + 0.1;
  m->step = 0;
  music_pump(m, w, mute);
}
