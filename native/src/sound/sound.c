/* sound.c - game-facing audio API implemented as a lock-free SPSC command ring.
 * The game thread only enqueues small POD commands; the audio thread drains them at
 * the start of every render block and builds the synthesis graph (js: sfx, clank,
 * engineApply, playJingle, jingleStop, startMusic, mute handling). */
#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "jingles.h"
#include "music.h"
#include "sound.h"
#include "sound_engine.h"

typedef enum { C_SFX, C_CLANK, C_JINGLE, C_NEWGAME, C_ENGINE, C_MUTE } cmd_kind;
typedef struct { uint8_t kind, wave; double a, b, c, d, e; } cmd_t;

#define RING_SIZE 1024 /* power of two */
static cmd_t ring[RING_SIZE];
static _Atomic uint32_t r_head, r_tail;
static _Atomic int dropped;

static wa *W;
static Music mus;
static int mute;
static wa_id jg = WA_NONE; /* js: mus.jg */
static struct { int on; wa_id o, o2, g, lg, l; } eng;

static void push(cmd_t c) {
  uint32_t h = atomic_load_explicit(&r_head, memory_order_relaxed);
  uint32_t t = atomic_load_explicit(&r_tail, memory_order_acquire);
  if (h - t >= RING_SIZE) { atomic_fetch_add(&dropped, 1); return; }
  ring[h & (RING_SIZE - 1)] = c;
  atomic_store_explicit(&r_head, h + 1, memory_order_release);
}

/* ============================================================ game-thread API */
void snd_sfx(double f, double d, SndWave type, double v, double f2) { push((cmd_t){C_SFX, (uint8_t)type, f, d, v, f2, 0}); }
void snd_clank(double v) { push((cmd_t){C_CLANK, 0, v, 0, 0, 0, 0}); }
void snd_play_jingle(JingleKind k) { push((cmd_t){C_JINGLE, (uint8_t)k, 0, 0, 0, 0, 0}); }
void snd_new_game(void) { push((cmd_t){C_NEWGAME, 0, 0, 0, 0, 0, 0}); }
void snd_engine(double engV, double engF) { push((cmd_t){C_ENGINE, 0, engV, engF, 0, 0, 0}); }
void snd_toggle_mute(void) { push((cmd_t){C_MUTE, 0, 0, 0, 0, 0, 0}); }

/* ============================================================ audio-thread recipes */
static wa_wave to_wave(int t) {
  switch (t) {
  case SND_SINE: return WA_SINE;
  case SND_SQUARE: return WA_SQUARE;
  case SND_SAWTOOTH: return WA_SAWTOOTH;
  default: return WA_TRIANGLE;
  }
}

/* js: sfx(f, d, ty, v, f2) */
static void do_sfx(double f, double d, int ty, double v, double f2) {
  if (mute) return;
  double t = wa_now(W);
  wa_id o = wa_osc(W, to_wave(ty)), gn = wa_gain(W, 1);
  wa_set_at(W, o, WA_P_FREQ, f, t);
  wa_exp_to(W, o, WA_P_FREQ, fmax(20, f2), t + d);
  wa_set_at(W, gn, WA_P_GAIN, v, t);
  wa_exp_to(W, gn, WA_P_GAIN, 0.0001, t + d);
  wa_connect(W, o, gn);
  wa_connect(W, gn, wa_destination(W));
  wa_start(W, o, t);
  wa_stop(W, o, t + d);
}

/* js: clank(v) -- three metallic claw clanks */
static void do_clank(double v) {
  if (mute) return;
  static const double delay[3] = {0, 0.09, 0.17}, freq[3] = {1250, 1720, 1480};
  for (int i = 0; i < 3; i++) {
    double t = wa_now(W) + delay[i], f = freq[i];
    wa_id o = wa_osc(W, WA_SQUARE), o2 = wa_osc(W, WA_SQUARE), gn = wa_gain(W, 1), fl = wa_biquad(W, WA_BANDPASS);
    wa_set_value(W, o, WA_P_FREQ, f);
    wa_set_value(W, o2, WA_P_FREQ, f * 1.51);
    wa_set_value(W, fl, WA_P_FREQ, 2200);
    wa_set_value(W, fl, WA_P_Q, 3);
    wa_set_at(W, gn, WA_P_GAIN, 0.1 * v, t);
    wa_exp_to(W, gn, WA_P_GAIN, 0.0001, t + 0.09);
    wa_connect(W, o, fl);
    wa_connect(W, o2, fl);
    wa_connect(W, fl, gn);
    wa_connect(W, gn, wa_destination(W));
    wa_start(W, o, t);
    wa_start(W, o2, t);
    wa_stop(W, o, t + 0.1);
    wa_stop(W, o2, t + 0.1);
  }
}

/* js: engineApply -- creates the hum on first use, then retargets it every frame */
static void do_engine(double engV, double engF) {
  if (!eng.on) {
    eng.on = 1;
    eng.o = wa_osc(W, WA_SAWTOOTH);
    eng.o2 = wa_osc(W, WA_SAWTOOTH);
    wa_id fl = wa_biquad(W, WA_LOWPASS);
    eng.g = wa_gain(W, 0);
    eng.l = wa_osc(W, WA_SINE);
    eng.lg = wa_gain(W, 1);
    wa_set_value(W, fl, WA_P_FREQ, 340);
    wa_set_value(W, eng.l, WA_P_FREQ, 8);
    wa_connect(W, eng.o, fl);
    wa_connect(W, eng.o2, fl);
    wa_connect(W, fl, eng.g);
    wa_connect(W, eng.g, wa_destination(W));
    wa_connect(W, eng.l, eng.lg);
    wa_connect_param(W, eng.lg, eng.g, WA_P_GAIN);
    double now = wa_now(W);
    wa_start(W, eng.o, now);
    wa_start(W, eng.o2, now);
    wa_start(W, eng.l, now);
    wa_pin(W, eng.o); wa_pin(W, eng.o2); wa_pin(W, eng.l); wa_pin(W, fl);
    wa_pin(W, eng.g); wa_pin(W, eng.lg);
  }
  double t = wa_now(W), v = mute ? 0 : engV;
  wa_target(W, eng.g, WA_P_GAIN, v, t, 0.08);
  wa_target(W, eng.lg, WA_P_GAIN, v * 0.5, t, 0.08);
  wa_target(W, eng.o, WA_P_FREQ, engF, t, 0.1);
  wa_target(W, eng.o2, WA_P_FREQ, engF * 1.5 + 1.5, t, 0.1);
  wa_target(W, eng.l, WA_P_FREQ, 5 + engF * 0.1, t, 0.1);
}

/* js: jingleStop */
static void do_jingle_stop(void) {
  if (jg != WA_NONE) {
    double now = wa_now(W);
    wa_cancel(W, jg, WA_P_GAIN, now);
    wa_set_at(W, jg, WA_P_GAIN, 0, now);
  }
  jg = WA_NONE;
}

/* js: playJingle(kind) */
static void do_jingle(int kind) {
  if (mute) return;
  do_jingle_stop();
  wa_id j = wa_gain(W, 1), comp = wa_compressor(W), master = wa_gain(W, kind == JINGLE_WIN ? 0.4 : 0.45);
  wa_connect(W, master, comp);
  wa_connect(W, comp, j);
  wa_connect(W, j, wa_destination(W));
  jg = j;
  double dur = kind == JINGLE_WIN ? jingle_win(W, master) : jingle_lose(W, master);
  double end = wa_now(W) + dur + 1.0; /* the piece plus its release tails */
  wa_die_at(W, master, end);
  wa_die_at(W, comp, end);
  wa_die_at(W, j, end);
  music_pause(&mus, W, dur);
}

/* js: newGame()'s audio part: startMusic(); jingleStop(); if (mus.paused) mlResume(); */
static void do_new_game(void) {
  music_start(&mus, W, mute);
  do_jingle_stop();
  if (mus.paused) music_resume(&mus, W, mute);
}

/* js: KeyM handler */
static void do_mute(void) {
  mute = !mute;
  if (mus.on) wa_set_value(W, mus.bus, WA_P_GAIN, mute ? 0 : 0.45);
}

static void block_hook(wa *w, void *user) {
  (void)w; (void)user;
  uint32_t t = atomic_load_explicit(&r_tail, memory_order_relaxed);
  uint32_t h = atomic_load_explicit(&r_head, memory_order_acquire);
  while (t != h) {
    cmd_t c = ring[t & (RING_SIZE - 1)];
    t++;
    switch (c.kind) {
    case C_SFX: do_sfx(c.a, c.b, c.wave, c.c, c.d); break;
    case C_CLANK: do_clank(c.a); break;
    case C_JINGLE: do_jingle(c.wave); break;
    case C_NEWGAME: do_new_game(); break;
    case C_ENGINE: do_engine(c.a, c.b); break;
    case C_MUTE: do_mute(); break;
    }
  }
  atomic_store_explicit(&r_tail, t, memory_order_release);
  music_pump(&mus, W, mute);
}

/* ============================================================ lifecycle */
int snd_engine_init(int sample_rate) {
  W = wa_create(sample_rate, 0xC0FFEEu);
  if (!W) return 0;
  memset(&mus, 0, sizeof mus);
  memset(&eng, 0, sizeof eng);
  mute = 0;
  jg = WA_NONE;
  atomic_store(&r_head, 0);
  atomic_store(&r_tail, 0);
  atomic_store(&dropped, 0);
  wa_set_block_hook(W, block_hook, NULL);
  return 1;
}
void snd_engine_shutdown(void) { wa_destroy(W); W = NULL; }
void snd_engine_render(float *stereo, int frames) {
  if (W) wa_render(W, stereo, frames);
  else memset(stereo, 0, sizeof(float) * 2 * (size_t)frames);
}
const wa *snd_engine_wa(void) { return W; }
int snd_dropped_commands(void) { return atomic_load(&dropped); }
