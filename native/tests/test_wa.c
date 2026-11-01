/* test_wa.c - audio engine checks against the Web Audio specification and closed-form results. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wa/wa.h"

static int fails, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define SR 48000
static const double PI_ = 3.14159265358979323846;

/* renders n frames; returns mono left channel (caller frees) */
static float *render(wa *w, int n) {
  float *st = malloc(sizeof(float) * 2 * (size_t)n), *m = malloc(sizeof(float) * (size_t)n);
  wa_render(w, st, n);
  for (int i = 0; i < n; i++) m[i] = st[2 * i];
  free(st);
  return m;
}
static double rms(const float *x, int n) { double s = 0; for (int i = 0; i < n; i++) s += (double)x[i] * x[i]; return sqrt(s / n); }
/* amplitude of frequency f via correlation over whole periods (Hann-free, exact for integer cycles) */
static double tone_amp(const float *x, int n, double f) {
  double c = 0, s = 0;
  for (int i = 0; i < n; i++) { c += x[i] * cos(2 * PI_ * f * i / SR); s += x[i] * sin(2 * PI_ * f * i / SR); }
  return 2 * sqrt(c * c + s * s) / n;
}

static void test_params(void) {
  wa *w = wa_create(SR, 1);
  wa_id g = wa_gain(w, 0);
  wa_set_at(w, g, WA_P_GAIN, 0, 0.1);
  wa_linear_to(w, g, WA_P_GAIN, 1, 0.2);
  CHECK(fabs(wa_param_at(w, g, 0, 0.15) - 0.5) < 1e-9, "linear ramp midpoint");
  CHECK(wa_param_at(w, g, 0, 0.05) == 0 && fabs(wa_param_at(w, g, 0, 0.3) - 1) < 1e-9, "before/after a linear ramp");
  wa_set_at(w, g, WA_P_GAIN, 1, 1.0);
  wa_exp_to(w, g, WA_P_GAIN, 0.01, 2.0);
  CHECK(fabs(wa_param_at(w, g, 0, 1.5) - 0.1) < 1e-9, "exponential ramp midpoint (geometric mean) %g", wa_param_at(w, g, 0, 1.5));
  wa_set_at(w, g, WA_P_GAIN, 0.8, 3.0);
  wa_target(w, g, WA_P_GAIN, 0.2, 3.0, 0.5);
  CHECK(fabs(wa_param_at(w, g, 0, 3.5) - (0.2 + 0.6 * exp(-1))) < 1e-9, "setTarget after one time constant");
  wa_cancel(w, g, WA_P_GAIN, 2.5);
  CHECK(fabs(wa_param_at(w, g, 0, 3.5) - 0.01) < 1e-9, "cancelScheduledValues removed later events: %g", wa_param_at(w, g, 0, 3.5));
  for (int i = 0; i < 200; i++) wa_set_at(w, g, WA_P_GAIN, i, 10 + i * 0.01); /* overflow of the event list must not crash */
  float *x = render(w, 1000); free(x);
  wa_destroy(w);
}

static void check_wave(wa_wave type, const char *name, double e25, double e50, double e75, double tol) {
  wa *w = wa_create(SR, 1);
  wa_id o = wa_osc(w, type);
  wa_set_value(w, o, WA_P_FREQ, 100); /* 480 samples per period */
  wa_connect(w, o, wa_destination(w));
  wa_start(w, o, 0);
  float *x = render(w, 1920);
  /* second period: phase 0.25, 0.5, 0.75 */
  CHECK(fabs(x[480 + 120] - e25) < tol, "%s at phase .25 = %.3f want %.3f", name, x[600], e25);
  CHECK(fabs(x[480 + 360] - e75) < tol, "%s at phase .75 = %.3f want %.3f", name, x[840], e75);
  if (e50 > -9) CHECK(fabs(x[480 + 240] - e50) < tol, "%s at phase .5 = %.3f want %.3f", name, x[720], e50);
  free(x);
  wa_destroy(w);
}

static void test_oscillators(void) {
  /* Web Audio conventions: sine starts at 0; square +1 first; saw rises from 0; triangle rises from 0 */
  check_wave(WA_SINE, "sine", 1, 0, -1, 0.01);
  check_wave(WA_SQUARE, "square", 1, -9, -1, 0.02); /* phase .5 is the exact edge (band-limited value 0) */
  check_wave(WA_SAWTOOTH, "sawtooth", 0.5, -9, -0.5, 0.02);
  check_wave(WA_TRIANGLE, "triangle", 1, 0, -1, 0.02);
  /* frequency, detune and band-limiting */
  wa *w = wa_create(SR, 1);
  wa_id o = wa_osc(w, WA_SINE);
  wa_set_value(w, o, WA_P_FREQ, 440);
  wa_set_value(w, o, WA_P_DETUNE, 1200); /* +1 octave */
  wa_connect(w, o, wa_destination(w)); wa_start(w, o, 0);
  float *x = render(w, SR);
  CHECK(fabs(tone_amp(x, SR, 880) - 1) < 0.001, "detune +1200 cents doubles the frequency (amp %.4f)", tone_amp(x, SR, 880));
  free(x); wa_destroy(w);
  /* 2300 Hz sawtooth: harmonic 11 (25.3 kHz) would alias to 22.7 kHz, which is not a harmonic of 2300.
   * The game's highest fundamental is ~2.6 kHz. An ideal band-limited oscillator has nothing there; 2-point PolyBLEP/BLAMP must be >= 6 dB below naive. */
  static const struct { wa_wave t; const char *n; double h11; double max_ratio; } cases[2] = {
      {WA_SAWTOOTH, "sawtooth", 1.0 / 11, 0.5}, {WA_TRIANGLE, "triangle", 1.0 / 121, 0.5}};
  for (int i = 0; i < 2; i++) {
    w = wa_create(SR, 1);
    o = wa_osc(w, cases[i].t);
    wa_set_value(w, o, WA_P_FREQ, 2300);
    wa_connect(w, o, wa_destination(w)); wa_start(w, o, 0);
    x = render(w, SR);
    double fund = tone_amp(x, SR, 2300), alias = tone_amp(x, SR, 22700), naive = fund * cases[i].h11;
    CHECK(alias < naive * cases[i].max_ratio, "%s alias at 22.7 kHz: %.5f vs naive %.5f (%.0f%%)", cases[i].n, alias, naive, 100 * alias / naive);
    free(x); wa_destroy(w);
  }
}

static double filter_gain(wa_filter t, double fc, double q, double f) {
  wa *w = wa_create(SR, 1);
  wa_id o = wa_osc(w, WA_SINE), pre = wa_gain(w, 0.25), b = wa_biquad(w, t); /* 0.25: stay clear of the output limiter */
  wa_set_value(w, o, WA_P_FREQ, f);
  wa_set_value(w, b, WA_P_FREQ, fc);
  wa_set_value(w, b, WA_P_Q, q);
  wa_connect(w, o, pre); wa_connect(w, pre, b); wa_connect(w, b, wa_destination(w)); wa_start(w, o, 0);
  float *x = render(w, SR);
  double a = tone_amp(x + SR / 2, SR / 2, f) / 0.25;
  free(x); wa_destroy(w);
  return a;
}

static void test_biquads(void) {
  /* lowpass Q is in dB: |H| at fc equals the linear Q */
  CHECK(fabs(filter_gain(WA_LOWPASS, 1000, 1, 1000) - pow(10, 1.0 / 20)) < 0.03, "lowpass default Q (1 dB): gain at fc = %.3f", filter_gain(WA_LOWPASS, 1000, 1, 1000));
  CHECK(fabs(filter_gain(WA_LOWPASS, 1000, 2, 1000) - pow(10, 2.0 / 20)) < 0.03, "lowpass Q=2 dB: gain at fc");
  double ql = pow(10, 1.0 / 20);
  for (double r = 0.25; r <= 4; r *= 2) {
    double want = 1 / sqrt((1 - r * r) * (1 - r * r) + r * r / (ql * ql)), got = filter_gain(WA_LOWPASS, 1000, 1, 1000 * r);
    CHECK(fabs(got - want) / want < 0.06, "lowpass |H| at %.2f fc: %.3f want %.3f", r, got, want);
  }
  CHECK(fabs(filter_gain(WA_HIGHPASS, 2000, 1, 20000)) > 0.97, "highpass passes the high band");
  CHECK(filter_gain(WA_HIGHPASS, 2000, 1, 200) < 0.02, "highpass blocks the low band");
  CHECK(fabs(filter_gain(WA_BANDPASS, 2200, 3, 2200) - 1) < 0.02, "bandpass (Q linear = 3) unity at fc");
  CHECK(filter_gain(WA_BANDPASS, 2200, 3, 300) < 0.2 && filter_gain(WA_BANDPASS, 2200, 3, 15000) < 0.35, "bandpass attenuates away from fc");
}

static void test_compressor(void) {
  /* settled sine: gain = saturate(A)/A * makeup (closed form of Blink's curve); makeup = (1/saturate(1))^0.6 = 3.66 dB */
  static const double amp[5] = {0.05, 0.1, 0.3, 0.6, 1.0}, want_db[5] = {3.66, 3.55, 2.18, 0.07, -2.44};
  for (int i = 0; i < 5; i++) {
    wa *w = wa_create(SR, 1);
    wa_id o = wa_osc(w, WA_SINE), g = wa_gain(w, amp[i]), c = wa_compressor(w);
    wa_set_value(w, o, WA_P_FREQ, 440);
    wa_connect(w, o, g); wa_connect(w, g, c); wa_connect(w, c, wa_destination(w)); wa_start(w, o, 0);
    float *x = render(w, 2 * SR);
    double got = 20 * log10(rms(x + SR, SR) / (amp[i] / sqrt(2.0)));
    CHECK(fabs(got - want_db[i]) < 0.3, "compressor gain at A=%.2f: %.2f dB want %.2f", amp[i], got, want_db[i]);
    for (int k = 0; k < 2 * SR; k++) if (!isfinite(x[k])) { CHECK(0, "non-finite compressor output"); break; }
    free(x); wa_destroy(w);
  }
  /* latency: a click through the compressor appears 6 ms (288 frames) later */
  wa *w = wa_create(SR, 1);
  wa_id o = wa_osc(w, WA_SQUARE), g = wa_gain(w, 0), c = wa_compressor(w);
  wa_set_value(w, o, WA_P_FREQ, 100);
  wa_set_at(w, g, WA_P_GAIN, 0, 0); wa_set_at(w, g, WA_P_GAIN, 0.01, 0.5);
  wa_connect(w, o, g); wa_connect(w, g, c); wa_connect(w, c, wa_destination(w)); wa_start(w, o, 0);
  float *x = render(w, SR);
  int first = -1;
  for (int i = 0; i < SR; i++) if (fabsf(x[i]) > 1e-4f) { first = i; break; }
  CHECK(first >= 24000 + 280 && first <= 24000 + 300, "compressor pre-delay: first output at %d (want ~24288)", first);
  free(x); wa_destroy(w);
}

static void test_lifecycle(void) {
  wa *w = wa_create(SR, 1);
  int base = wa_node_count(w);
  for (int i = 0; i < 100; i++) { /* 100 short voices: osc -> lowpass -> gain -> destination */
    wa_id o = wa_osc(w, WA_SAWTOOTH), lp = wa_biquad(w, WA_LOWPASS), g = wa_gain(w, 0.1);
    double t = i * 0.01;
    wa_connect(w, o, lp); wa_connect(w, lp, g); wa_connect(w, g, wa_destination(w));
    wa_start(w, o, t); wa_stop(w, o, t + 0.05);
  }
  float *x = render(w, SR / 2); free(x);
  CHECK(wa_node_peak(w) > base + 100, "voices were alive at once (peak %d)", wa_node_peak(w));
  x = render(w, SR); free(x);
  CHECK(wa_node_count(w) == base, "finished voices are garbage collected: %d live, expected %d", wa_node_count(w), base);
  /* pool exhaustion degrades gracefully */
  int made = 0;
  for (int i = 0; i < 5000; i++) { wa_id o = wa_osc(w, WA_SINE); if (o != WA_NONE) { made++; wa_connect(w, o, wa_destination(w)); wa_start(w, o, 0); wa_stop(w, o, 0.01); } }
  CHECK(made < 5000 && wa_alloc_failures(w) > 0, "pool full is reported (%d created)", made);
  wa_set_at(w, WA_NONE, 0, 1, 1); wa_connect(w, WA_NONE, WA_NONE); /* calls on WA_NONE are no-ops */
  x = render(w, SR); free(x);
  CHECK(wa_node_count(w) == base, "pool recovers after exhaustion: %d live", wa_node_count(w));
  wa_destroy(w);
}

static void test_modulation(void) {
  /* LFO into gain.gain: output = in * (value + lfo); the engine tremolo */
  wa *w = wa_create(SR, 1);
  wa_id o = wa_osc(w, WA_SINE), g = wa_gain(w, 0.5), l = wa_osc(w, WA_SINE), lg = wa_gain(w, 0.25);
  wa_set_value(w, o, WA_P_FREQ, 1000); wa_set_value(w, l, WA_P_FREQ, 4);
  wa_connect(w, o, g); wa_connect(w, g, wa_destination(w)); wa_connect(w, l, lg); wa_connect_param(w, lg, g, WA_P_GAIN);
  wa_start(w, o, 0); wa_start(w, l, 0); wa_pin(w, g); wa_pin(w, lg);
  float *x = render(w, SR);
  double peak = 0; for (int i = SR / 4 - 200; i < SR / 4 + 200; i++) if (fabs(x[i]) > peak) peak = fabs(x[i]); /* lfo peak at t = 1/16 s .. use 1/16 */
  double hi = 0; for (int i = 3000 - 24; i < 3000 + 24; i++) hi = fmax(hi, fabs(x[i])); /* 4 Hz LFO peak at 62.5 ms = sample 3000 */
  CHECK(hi > 0.7 && hi < 0.76, "tremolo peak gain 0.5 + 0.25 = 0.75: %.3f (%.3f)", hi, peak);
  free(x); wa_destroy(w);
}

int main(void) {
  test_params(); test_oscillators(); test_biquads(); test_compressor(); test_lifecycle(); test_modulation();
  printf("wa: %d checks, %d failed\n", checks, fails);
  return fails != 0;
}
