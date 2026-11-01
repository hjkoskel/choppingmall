/* jingles.c - js: jingleWin and jingleLose (the win/lose stingers).
 * Each function builds its whole graph into `master` at the current engine time and
 * returns the piece length in seconds. Runs on the audio thread. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jingles.h"

typedef struct { wa *w; wa_id master; double beat, t0; } jc;

static double at(const jc *j, double b) { return j->t0 + b * j->beat; }

/* js: hz(name) -- note name such as "Bb3" or "F#4" to Hz */
static double hz(const char *name) {
  static const int semi[7] = {9, 11, 0, 2, 4, 5, 7}; /* A B C D E F G */
  int l = name[0] - 'A', acc = 0, oct;
  const char *p = name + 1;
  if (*p == '#') { acc = 1; p++; } else if (*p == 'b') { acc = -1; p++; }
  oct = atoi(p);
  return 440.0 * pow(2.0, (12 * (oct + 1) + semi[l] + acc - 69) / 12.0);
}

static void brass(jc *j, const char *name, double b, double dur, double vol) {
  wa *w = j->w;
  double s = at(j, b), e = s + dur * j->beat;
  wa_id g = wa_gain(w, 1), lp = wa_biquad(w, WA_LOWPASS);
  wa_set_value(w, lp, WA_P_Q, 2);
  wa_set_at(w, lp, WA_P_FREQ, 300, s);
  wa_linear_to(w, lp, WA_P_FREQ, 1800, s + 0.08);
  wa_linear_to(w, lp, WA_P_FREQ, 1000, e);
  wa_set_at(w, g, WA_P_GAIN, 0, s);
  wa_linear_to(w, g, WA_P_GAIN, vol, s + 0.04);
  wa_set_at(w, g, WA_P_GAIN, vol, e - 0.05);
  wa_linear_to(w, g, WA_P_GAIN, 0, e + 0.12);
  wa_connect(w, lp, g);
  wa_connect(w, g, j->master);
  static const double det[2] = {-6, 6};
  for (int i = 0; i < 2; i++) {
    wa_id o = wa_osc(w, WA_SAWTOOTH);
    wa_set_value(w, o, WA_P_FREQ, hz(name));
    wa_set_value(w, o, WA_P_DETUNE, det[i]);
    wa_connect(w, o, lp);
    wa_start(w, o, s);
    wa_stop(w, o, e + 0.15);
  }
}

static void strings(jc *j, const char *name, double b, double dur, double vol) {
  wa *w = j->w;
  double s = at(j, b), e = s + dur * j->beat;
  wa_id g = wa_gain(w, 1), lp = wa_biquad(w, WA_LOWPASS);
  wa_set_value(w, lp, WA_P_FREQ, 2200);
  wa_set_at(w, g, WA_P_GAIN, 0, s);
  wa_linear_to(w, g, WA_P_GAIN, vol, s + 0.08);
  wa_set_at(w, g, WA_P_GAIN, vol, e - 0.05);
  wa_linear_to(w, g, WA_P_GAIN, 0, e + 0.2);
  wa_connect(w, lp, g);
  wa_connect(w, g, j->master);
  static const double det[2] = {-7, 7};
  for (int i = 0; i < 2; i++) {
    wa_id o = wa_osc(w, WA_SAWTOOTH);
    wa_set_value(w, o, WA_P_FREQ, hz(name));
    wa_set_value(w, o, WA_P_DETUNE, det[i]);
    wa_connect(w, o, lp);
    wa_start(w, o, s);
    wa_stop(w, o, e + 0.25);
  }
}

static void piano(jc *j, const char *name, double b, double dur, double vol) {
  wa *w = j->w;
  double s = at(j, b), e = s + dur * j->beat, f = hz(name);
  wa_id g = wa_gain(w, 1);
  wa_set_at(w, g, WA_P_GAIN, 0, s);
  wa_linear_to(w, g, WA_P_GAIN, vol, s + 0.005);
  wa_exp_to(w, g, WA_P_GAIN, vol * 0.25, s + 0.4);
  wa_exp_to(w, g, WA_P_GAIN, 0.0005, e + 0.4);
  wa_connect(w, g, j->master);
  static const wa_wave types[2] = {WA_TRIANGLE, WA_SINE};
  static const double mult[2] = {1, 2};
  for (int i = 0; i < 2; i++) {
    wa_id o = wa_osc(w, types[i]), og = wa_gain(w, mult[i] == 1 ? 1 : 0.3);
    wa_set_value(w, o, WA_P_FREQ, f * mult[i]);
    wa_connect(w, o, og);
    wa_connect(w, og, g);
    wa_start(w, o, s);
    wa_stop(w, o, e + 0.45);
  }
}

static void bass(jc *j, const char *name, double b, double dur, double vol) {
  wa *w = j->w;
  double s = at(j, b), e = s + dur * j->beat;
  wa_id o = wa_osc(w, WA_SAWTOOTH), lp = wa_biquad(w, WA_LOWPASS), g = wa_gain(w, 1);
  wa_set_value(w, o, WA_P_FREQ, hz(name));
  wa_set_at(w, lp, WA_P_FREQ, 900, s);
  wa_exp_to(w, lp, WA_P_FREQ, 300, e);
  wa_set_at(w, g, WA_P_GAIN, 0, s);
  wa_linear_to(w, g, WA_P_GAIN, vol, s + 0.01);
  wa_set_at(w, g, WA_P_GAIN, vol, e - 0.06);
  wa_linear_to(w, g, WA_P_GAIN, 0, e + 0.08);
  wa_connect(w, o, lp);
  wa_connect(w, lp, g);
  wa_connect(w, g, j->master);
  wa_start(w, o, s);
  wa_stop(w, o, e + 0.1);
}

typedef void (*voice_fn)(jc *, const char *, double, double, double);
/* js: chord(fn, names, b, dur, vol) */
static void chord(jc *j, voice_fn fn, const char *const *names, int n, double b, double dur, double vol) {
  for (int i = 0; i < n; i++) fn(j, names[i], b, dur, vol);
}

#define ARR(...) (const char *const[]){__VA_ARGS__}
#define NARR(...) (int)(sizeof((const char *const[]){__VA_ARGS__}) / sizeof(char *))
#define CHORD(j, fn, b, dur, vol, ...) chord((j), (fn), ARR(__VA_ARGS__), NARR(__VA_ARGS__), (b), (dur), (vol))

/* js: jingleWin -- 157 bpm victory piece */
double jingle_win(wa *w, wa_id master) {
  jc j = {w, master, 60.0 / 157, wa_now(w) + 0.05};
  piano(&j, "G4", 1, 1, 0.22);
  piano(&j, "A4", 2, 1, 0.22);
  CHORD(&j, piano, 3, 1, 0.16, "Bb4", "D5", "F5");
  CHORD(&j, brass, 4, 4, 0.22, "Eb3", "Bb3", "Eb4", "G4", "Bb4", "Eb5", "G5");
  piano(&j, "Bb3", 4, 2, 0.22);
  strings(&j, "Bb4", 4, 4, 0.22);
  bass(&j, "Eb2", 4, 4, 0.6);
  CHORD(&j, piano, 10, 2, 0.14, "D4", "F4", "A4", "D5", "F5");
  CHORD(&j, piano, 10, 2, 0.14, "D2", "A2", "D3", "F3", "A3");
  CHORD(&j, brass, 10, 2, 0.3, "D4", "F4", "A4");
  strings(&j, "A4", 8, 1, 0.22);
  strings(&j, "G4", 9, 3, 0.22);
  CHORD(&j, piano, 13, 3, 0.2, "G3", "B3", "D4");
  CHORD(&j, piano, 13, 3, 0.2, "G2", "D3", "G3");
  brass(&j, "D3", 13, 3, 0.5);
  bass(&j, "G2", 13, 3, 0.6);
  CHORD(&j, piano, 16, 4, 0.14, "C#4", "E4", "A4", "C#5", "E5");
  CHORD(&j, piano, 16, 4, 0.16, "A1", "A2", "E3", "A3");
  return 20 * j.beat + 0.8;
}

/* ---- jingleLose: pitches are MIDI numbers ---- */
static double mhz(int m) { return 440.0 * pow(2.0, (m - 69) / 12.0); }

static void l_brass(jc *j, int m, double sb, double db) {
  wa *w = j->w;
  double s = at(j, sb), e = s + db * j->beat;
  wa_id g = wa_gain(w, 1), lp = wa_biquad(w, WA_LOWPASS);
  wa_set_value(w, lp, WA_P_Q, 2);
  wa_set_at(w, lp, WA_P_FREQ, 300, s);
  wa_linear_to(w, lp, WA_P_FREQ, 1800, s + 0.08);
  wa_linear_to(w, lp, WA_P_FREQ, 1100, e);
  wa_set_at(w, g, WA_P_GAIN, 0, s);
  wa_linear_to(w, g, WA_P_GAIN, 0.5, s + 0.04);
  wa_set_at(w, g, WA_P_GAIN, 0.5, e - 0.05);
  wa_linear_to(w, g, WA_P_GAIN, 0, e + 0.12);
  wa_connect(w, lp, g);
  wa_connect(w, g, j->master);
  static const double det[2] = {-6, 6};
  for (int i = 0; i < 2; i++) {
    wa_id o = wa_osc(w, WA_SAWTOOTH);
    wa_set_value(w, o, WA_P_FREQ, mhz(m));
    wa_set_value(w, o, WA_P_DETUNE, det[i]);
    wa_connect(w, o, lp);
    wa_start(w, o, s);
    wa_stop(w, o, e + 0.15);
  }
}

static void l_strings(jc *j, int m, double sb, double db) {
  wa *w = j->w;
  double s = at(j, sb), e = s + db * j->beat;
  wa_id g = wa_gain(w, 1), lp = wa_biquad(w, WA_LOWPASS);
  wa_set_value(w, lp, WA_P_FREQ, 2500);
  wa_set_at(w, g, WA_P_GAIN, 0, s);
  wa_linear_to(w, g, WA_P_GAIN, 0.28, s + 0.02);
  wa_exp_to(w, g, WA_P_GAIN, 0.001, e + 0.1);
  wa_connect(w, lp, g);
  wa_connect(w, g, j->master);
  static const double det[2] = {-5, 5};
  for (int i = 0; i < 2; i++) {
    wa_id o = wa_osc(w, WA_SAWTOOTH);
    wa_set_value(w, o, WA_P_FREQ, mhz(m));
    wa_set_value(w, o, WA_P_DETUNE, det[i]);
    wa_connect(w, o, lp);
    wa_start(w, o, s);
    wa_stop(w, o, e + 0.15);
  }
}

static void l_timpani(jc *j, int m, double sb) {
  wa *w = j->w;
  double s = at(j, sb), f = mhz(m);
  wa_id o = wa_osc(w, WA_SINE), g = wa_gain(w, 1);
  wa_set_at(w, o, WA_P_FREQ, f * 1.5, s);
  wa_exp_to(w, o, WA_P_FREQ, f, s + 0.06);
  wa_set_at(w, g, WA_P_GAIN, 0.9, s);
  wa_exp_to(w, g, WA_P_GAIN, 0.001, s + 0.6);
  wa_connect(w, o, g);
  wa_connect(w, g, j->master);
  wa_start(w, o, s);
  wa_stop(w, o, s + 0.65);
  /* 40 ms noise burst with a linear fade, through a bandpass at 400 Hz (Q 1) */
  wa_id n = wa_noise(w, 0.04, 1), ng = wa_gain(w, 0.35), bp = wa_biquad(w, WA_BANDPASS);
  wa_set_value(w, bp, WA_P_FREQ, 400);
  wa_connect(w, n, bp);
  wa_connect(w, bp, ng);
  wa_connect(w, ng, j->master);
  wa_start(w, n, s);
}

/* js: jingleLose -- 143 bpm robot-wins piece */
double jingle_lose(wa *w, wa_id master) {
  jc j = {w, master, 60.0 / 143, wa_now(w) + 0.05};
  const int D2 = 38, A2 = 45, D3 = 50, Fs3 = 54, A3 = 57, D4 = 62, D5 = 74, Fs5 = 78, A5 = 81;
  l_brass(&j, A3, 0, 3.5);
  l_brass(&j, A2, 0, 3.5);
  l_brass(&j, D4, 4, 12);
  l_brass(&j, D3, 4, 12);
  l_brass(&j, D2, 4, 12);
  const double tp = 12, sx = 1.0 / 6;
  l_timpani(&j, D3, tp + sx);
  l_timpani(&j, D3, tp + 2 * sx);
  for (int k = 0; k < 3; k++) l_timpani(&j, D3, tp + 0.5 + k * sx);
  for (int k = 0; k < 3; k++) l_timpani(&j, D3, tp + 1 + k * sx);
  l_timpani(&j, D3, tp + 1.5);
  { const int ms[2] = {A3, A2}; for (int i = 0; i < 2; i++) { l_strings(&j, ms[i], 2, 0.4); l_strings(&j, ms[i], 3.5, 0.5); } }
  { const int ms[2] = {D2, A2}; for (int i = 0; i < 2; i++) l_strings(&j, ms[i], 6, 2); }
  l_strings(&j, Fs5, 8, 2);
  { const int ms[5] = {D5, Fs5, A5, D3, Fs3}; for (int i = 0; i < 5; i++) l_strings(&j, ms[i], 10, 4); }
  { const int ms[4] = {D3, Fs3, A3, D4}; for (int i = 0; i < 4; i++) l_strings(&j, ms[i], 14, 2); }
  return 16 * j.beat + 0.8;
}
