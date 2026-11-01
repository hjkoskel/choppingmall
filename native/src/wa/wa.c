/* wa.c - see wa.h. Pull-based graph renderer, block size WA_BLOCK. */
#include "wa.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NODES 1536
#define MAX_EDGES 4096
#define MAX_EV 24
#define MAX_COMP 4
#define NOISE_TAB (1 << 18)
#define TWO_PI 6.283185307179586

typedef enum { N_FREE, N_OSC, N_GAIN, N_BIQUAD, N_NOISE, N_COMP, N_DEST } ntype;
typedef enum { EV_SET, EV_LIN, EV_EXP, EV_TARGET } evtype;
typedef struct { double t, v, t0, v0, tau; uint8_t type; } ev_t;
typedef struct { double value; int n; ev_t ev[MAX_EV]; } param_t;
typedef struct { int src, dst, param, next; } edge_t;

/* ---- dynamics compressor state (port of Blink's DynamicsCompressorKernel) ---- */
#define PRE_DELAY_FRAMES 1024
#define DIVISION 32
typedef struct {
  float predelay[PRE_DELAY_FRAMES];
  int rd, wr, last_pre_delay_frames;
  float detector_avg, comp_gain, metering_gain;
  float max_attack_diff_db;
  float db_threshold, db_knee, ratio, linear_threshold, knee_threshold, knee_threshold_db, ypos, slope, k;
  int curve_valid;
  int used;
} comp_t;

typedef struct {
  uint8_t type, wave, ftype;
  int pinned, first_in, comp_idx, out_ready_dummy;
  double die_at, idle_since, start_t, stop_t;
  uint32_t last_block;
  param_t p[2];
  double phase;
  /* biquad */
  double b0, b1, b2, a1, a2, x1, x2, y1, y2;
  /* noise */
  int noise_off, noise_pos, noise_len, noise_fade;
  float out[WA_BLOCK];
} node_t;

struct wa {
  int rate;
  uint64_t frames;           /* frames rendered at the start of the current block */
  uint32_t block_counter;
  node_t nodes[MAX_NODES];
  int free_nodes[MAX_NODES], nfree;
  edge_t edges[MAX_EDGES];
  int free_edges[MAX_EDGES], nfree_edges;
  comp_t comps[MAX_COMP];
  float noise[NOISE_TAB];
  float stage[WA_BLOCK * 2];
  int stage_pos;
  int dest, live, peak, alloc_fail;
  wa_block_hook hook;
  void *hook_user;
};

/* ===================================================================== helpers */
static inline node_t *N(wa *w, wa_id id) { return (id >= 0 && id < MAX_NODES && w->nodes[id].type != N_FREE) ? &w->nodes[id] : NULL; }
double wa_now(const wa *w) { return (double)w->frames / w->rate; }
int wa_sample_rate(const wa *w) { return w->rate; }
int wa_node_count(const wa *w) { return w->live; }
int wa_node_peak(const wa *w) { return w->peak; }
int wa_alloc_failures(const wa *w) { return w->alloc_fail; }
void wa_set_block_hook(wa *w, wa_block_hook fn, void *user) { w->hook = fn; w->hook_user = user; }

/* ===================================================================== AudioParam timeline */
static double ev_end_value(const ev_t *e) { return e->v; }

static double param_at(const param_t *p, double t) {
  int n = p->n, i = -1;
  while (i + 1 < n && p->ev[i + 1].t <= t) i++;
  if (i + 1 < n) {
    const ev_t *nx = &p->ev[i + 1];
    if (nx->type == EV_LIN || nx->type == EV_EXP) { /* a ramp is heading for nx */
      if (t < nx->t0) return nx->v0;
      double span = nx->t - nx->t0;
      if (span <= 0) return nx->v;
      double f = (t - nx->t0) / span;
      if (nx->type == EV_LIN) return nx->v0 + (nx->v - nx->v0) * f;
      if (nx->v0 == 0 || nx->v0 * nx->v < 0) return nx->v0; /* spec: hold when the ramp is undefined */
      return nx->v0 * pow(nx->v / nx->v0, f);
    }
  }
  if (i < 0) return p->value;
  const ev_t *e = &p->ev[i];
  if (e->type == EV_TARGET) return e->v + (e->v0 - e->v) * exp(-(t - e->t) / e->tau);
  return e->v;
}

static void param_insert(const wa *w, param_t *p, ev_t ev) {
  int pos = p->n;
  while (pos > 0 && p->ev[pos - 1].t > ev.t) pos--;
  if (ev.type == EV_LIN || ev.type == EV_EXP) {
    if (pos > 0) { ev.t0 = p->ev[pos - 1].t; ev.v0 = ev_end_value(&p->ev[pos - 1]); }
    else { ev.t0 = wa_now(w); ev.v0 = p->value; }
  } else if (ev.type == EV_TARGET) ev.v0 = param_at(p, ev.t);
  if (p->n == MAX_EV) { memmove(&p->ev[0], &p->ev[1], sizeof(ev_t) * (MAX_EV - 1)); p->n--; if (pos > 0) pos--; }
  memmove(&p->ev[pos + 1], &p->ev[pos], sizeof(ev_t) * (size_t)(p->n - pos));
  p->ev[pos] = ev;
  p->n++;
}

static void param_block(const param_t *p, double t0, double dt, float *out) {
  if (p->n == 0) { for (int i = 0; i < WA_BLOCK; i++) out[i] = (float)p->value; return; }
  const ev_t *last = &p->ev[p->n - 1];
  if (last->t <= t0 && last->type != EV_TARGET) { /* everything is in the past: constant */
    float v = (float)param_at(p, t0);
    for (int i = 0; i < WA_BLOCK; i++) out[i] = v;
    return;
  }
  for (int i = 0; i < WA_BLOCK; i++) out[i] = (float)param_at(p, t0 + i * dt);
}

/* drop events that can no longer influence the future; fold finished timelines into value */
static void param_prune(param_t *p, double t_end) {
  if (p->n == 0) return;
  int i = -1;
  while (i + 1 < p->n && p->ev[i + 1].t <= t_end) i++;
  if (i > 0) { memmove(&p->ev[0], &p->ev[i], sizeof(ev_t) * (size_t)(p->n - i)); p->n -= i; i = 0; }
  if (p->n == 1 && p->ev[0].t <= t_end) {
    const ev_t *e = &p->ev[0];
    if (e->type != EV_TARGET) { p->value = e->v; p->n = 0; }
    else if (t_end - e->t > 14 * e->tau) { p->value = e->v; p->n = 0; }
  }
}

/* ===================================================================== pools */
static int alloc_node(wa *w, ntype t) {
  if (w->nfree == 0) { w->alloc_fail++; return WA_NONE; }
  int id = w->free_nodes[--w->nfree];
  node_t *n = &w->nodes[id];
  memset(n, 0, sizeof *n);
  n->type = (uint8_t)t;
  n->first_in = -1; n->comp_idx = -1;
  n->die_at = INFINITY; n->idle_since = -1;
  n->start_t = INFINITY; n->stop_t = INFINITY;
  w->live++;
  if (w->live > w->peak) w->peak = w->live;
  return id;
}

static void free_node(wa *w, int id) {
  node_t *n = &w->nodes[id];
  if (n->comp_idx >= 0) w->comps[n->comp_idx].used = 0;
  n->type = N_FREE;
  w->free_nodes[w->nfree++] = id;
  w->live--;
}

wa *wa_create(int rate, uint32_t seed) {
  wa *w = calloc(1, sizeof *w);
  if (!w) return NULL;
  w->rate = rate;
  for (int i = MAX_NODES - 1; i >= 0; i--) w->free_nodes[w->nfree++] = i;
  for (int i = MAX_EDGES - 1; i >= 0; i--) w->free_edges[w->nfree_edges++] = i;
  uint32_t s = seed ? seed : 0x1234567u; /* xorshift32 white noise table */
  for (int i = 0; i < NOISE_TAB; i++) {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    w->noise[i] = (float)((double)s / 2147483648.0 - 1.0);
  }
  w->dest = alloc_node(w, N_DEST);
  w->nodes[w->dest].pinned = 1;
  w->stage_pos = WA_BLOCK;
  return w;
}
void wa_destroy(wa *w) { free(w); }

/* ===================================================================== creation */
wa_id wa_osc(wa *w, wa_wave type) {
  int id = alloc_node(w, N_OSC);
  if (id < 0) return id;
  node_t *n = &w->nodes[id];
  n->wave = (uint8_t)type;
  n->p[0].value = 440; n->p[1].value = 0;
  return id;
}
wa_id wa_gain(wa *w, double v) {
  int id = alloc_node(w, N_GAIN);
  if (id >= 0) w->nodes[id].p[0].value = v;
  return id;
}
wa_id wa_biquad(wa *w, wa_filter type) {
  int id = alloc_node(w, N_BIQUAD);
  if (id < 0) return id;
  node_t *n = &w->nodes[id];
  n->ftype = (uint8_t)type;
  n->p[0].value = 350; n->p[1].value = 1;
  return id;
}
wa_id wa_noise(wa *w, double dur, int fade) {
  int id = alloc_node(w, N_NOISE);
  if (id < 0) return id;
  node_t *n = &w->nodes[id];
  n->noise_len = (int)floor(w->rate * dur);
  n->noise_fade = fade;
  w->nodes[id].noise_off = (int)((w->frames * 2654435761u + (uint64_t)id * 40503u + (uint64_t)w->block_counter * 977u) & (NOISE_TAB - 1));
  return id;
}
wa_id wa_compressor(wa *w) {
  int ci = -1;
  for (int i = 0; i < MAX_COMP; i++) if (!w->comps[i].used) { ci = i; break; }
  if (ci < 0) { w->alloc_fail++; return WA_NONE; }
  int id = alloc_node(w, N_COMP);
  if (id < 0) return id;
  comp_t *c = &w->comps[ci];
  memset(c, 0, sizeof *c);
  c->used = 1; c->comp_gain = 1; c->max_attack_diff_db = -1; c->last_pre_delay_frames = -1;
  w->nodes[id].comp_idx = ci;
  return id;
}
wa_id wa_destination(wa *w) { return w->dest; }

static void add_edge(wa *w, wa_id from, wa_id to, int param) {
  if (!N(w, from) || !N(w, to)) return;
  if (w->nfree_edges == 0) { w->alloc_fail++; return; }
  int e = w->free_edges[--w->nfree_edges];
  w->edges[e] = (edge_t){from, to, param, w->nodes[to].first_in};
  w->nodes[to].first_in = e;
}
void wa_connect(wa *w, wa_id from, wa_id to) { add_edge(w, from, to, -1); }
void wa_connect_param(wa *w, wa_id from, wa_id to, int param) { add_edge(w, from, to, param); }
void wa_start(wa *w, wa_id id, double t) { node_t *n = N(w, id); if (n) n->start_t = t; }
void wa_stop(wa *w, wa_id id, double t) { node_t *n = N(w, id); if (n) n->stop_t = t; }
void wa_pin(wa *w, wa_id id) { node_t *n = N(w, id); if (n) n->pinned = 1; }
void wa_die_at(wa *w, wa_id id, double t) { node_t *n = N(w, id); if (n) { n->pinned = 1; n->die_at = t; } }

/* ===================================================================== AudioParam API */
static param_t *P(wa *w, wa_id id, int p) { node_t *n = N(w, id); return n && p >= 0 && p < 2 ? &n->p[p] : NULL; }
void wa_set_value(wa *w, wa_id id, int p, double v) {
  param_t *q = P(w, id, p);
  if (!q) return;
  q->value = v;
  if (q->n == 0) return; /* no timeline yet: the intrinsic value is all there is */
  param_insert(w, q, (ev_t){.t = wa_now(w), .v = v, .type = EV_SET}); /* .value = v is setValueAtTime(v, currentTime) */
}
void wa_set_at(wa *w, wa_id id, int p, double v, double t) { param_t *q = P(w, id, p); if (q) param_insert(w, q, (ev_t){.t = t, .v = v, .type = EV_SET}); }
void wa_linear_to(wa *w, wa_id id, int p, double v, double t) { param_t *q = P(w, id, p); if (q) param_insert(w, q, (ev_t){.t = t, .v = v, .type = EV_LIN}); }
void wa_exp_to(wa *w, wa_id id, int p, double v, double t) { param_t *q = P(w, id, p); if (q) param_insert(w, q, (ev_t){.t = t, .v = v, .type = EV_EXP}); }
void wa_target(wa *w, wa_id id, int p, double v, double t, double tau) { param_t *q = P(w, id, p); if (q) param_insert(w, q, (ev_t){.t = t, .v = v, .tau = tau, .type = EV_TARGET}); }
void wa_cancel(wa *w, wa_id id, int p, double t) {
  param_t *q = P(w, id, p);
  if (!q) return;
  int k = 0;
  for (int i = 0; i < q->n; i++) if (q->ev[i].t < t) q->ev[k++] = q->ev[i];
  q->n = k;
}
double wa_param_at(const wa *w, wa_id id, int p, double t) {
  if (id < 0 || id >= MAX_NODES || w->nodes[id].type == N_FREE) return 0;
  return param_at(&w->nodes[id].p[p], t);
}

/* ===================================================================== oscillator */
static inline double blep(double t, double dt) { /* PolyBLEP residual for a unit-height (2.0) step at t = 0 */
  if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
  if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
  return 0.0;
}
static inline double blamp(double u) { /* 2-point polyBLAMP residual for a unit slope change; u in samples from the corner */
  if (u <= -1.0 || u >= 1.0) return 0.0;
  if (u < 0) { double a = u + 1.0; return a * a * a / 6.0; }
  double a = 1.0 - u;
  return a * a * a / 6.0;
}
static inline double wrap_half(double d) { d -= floor(d + 0.5); return d; } /* to [-0.5, 0.5) */

static inline double osc_sample(int wave, double p, double dt) {
  switch (wave) {
  default:
  case WA_SINE: return sin(TWO_PI * p);
  case WA_SQUARE: { /* +1 for the first half period */
    double v = p < 0.5 ? 1.0 : -1.0;
    double q = p + 0.5; q -= floor(q);
    return v + blep(p, dt) - blep(q, dt);
  }
  case WA_SAWTOOTH: { /* starts at 0, rises to +1 at half period, wraps to -1 */
    double u = p + 0.5; u -= floor(u);
    return 2.0 * u - 1.0 - blep(u, dt);
  }
  case WA_TRIANGLE: {
    double v = p < 0.25 ? 4 * p : p < 0.75 ? 2 - 4 * p : 4 * p - 4;
    v += -8.0 * dt * blamp(wrap_half(p - 0.25) / dt);
    v += 8.0 * dt * blamp(wrap_half(p - 0.75) / dt);
    return v;
  }
  }
}

static void render_osc(wa *w, node_t *n, const float *freq, const float *detune, const float *fmod, double t0, double dtm, float *out) {
  double nyq = w->rate * 0.5;
  for (int i = 0; i < WA_BLOCK; i++) {
    double t = t0 + i * dtm;
    if (t < n->start_t || t >= n->stop_t) { out[i] = 0; continue; }
    double f = (double)freq[i] + (fmod ? fmod[i] : 0.0);
    if (detune[i] != 0) f *= pow(2.0, detune[i] / 1200.0);
    if (f > nyq) f = nyq; else if (f < -nyq) f = -nyq;
    double dt = fabs(f) / w->rate;
    out[i] = (float)osc_sample(n->wave, n->phase, dt > 1e-9 ? dt : 1e-9);
    n->phase += f / w->rate;
    n->phase -= floor(n->phase);
  }
}

/* ===================================================================== biquad (Web Audio spec cookbook) */
static void biquad_coeffs(const wa *w, node_t *n, double freq, double q) {
  double nyq = w->rate * 0.5, f = freq / nyq; /* normalised 0..1 */
  if (f >= 1.0) { /* at/above Nyquist */
    n->b0 = n->ftype == WA_LOWPASS ? 1 : 0; n->b1 = n->b2 = n->a1 = n->a2 = 0;
    return;
  }
  if (f <= 0.0) {
    n->b0 = n->ftype == WA_HIGHPASS ? 1 : 0; n->b1 = n->b2 = n->a1 = n->a2 = 0;
    if (n->ftype == WA_BANDPASS) n->b0 = 0;
    return;
  }
  double w0 = TWO_PI * 0.5 * f, cs = cos(w0), sn = sin(w0), a0, b0, b1, b2, a1, a2, alpha;
  switch (n->ftype) {
  default:
  case WA_LOWPASS: {
    alpha = sn / (2.0 * pow(10.0, q / 20.0)); /* Q is in dB for lowpass/highpass */
    b0 = (1 - cs) / 2; b1 = 1 - cs; b2 = b0;
    break;
  }
  case WA_HIGHPASS: {
    alpha = sn / (2.0 * pow(10.0, q / 20.0));
    b0 = (1 + cs) / 2; b1 = -(1 + cs); b2 = b0;
    break;
  }
  case WA_BANDPASS: {
    if (q <= 0) { n->b0 = n->b1 = n->b2 = n->a1 = n->a2 = 0; return; }
    alpha = sn / (2.0 * q);
    b0 = alpha; b1 = 0; b2 = -alpha;
    break;
  }
  }
  a0 = 1 + alpha; a1 = -2 * cs; a2 = 1 - alpha;
  n->b0 = b0 / a0; n->b1 = b1 / a0; n->b2 = b2 / a0; n->a1 = a1 / a0; n->a2 = a2 / a0;
}

/* ===================================================================== compressor (Blink DynamicsCompressorKernel) */
static inline float lin2db(float x) { return x == 0 ? -1000.0f : 20.0f * log10f(x); }
static inline float db2lin(float x) { return powf(10.0f, 0.05f * x); }
static float knee_curve(const comp_t *c, float x, float k) {
  if (x < c->linear_threshold) return x;
  return c->linear_threshold + (1 - expf(-k * (x - c->linear_threshold))) / k;
}
static float saturate(const comp_t *c, float x, float k) {
  if (x < c->knee_threshold) return knee_curve(c, x, k);
  float x_db = lin2db(x);
  float y_db = c->ypos + c->slope * (x_db - c->knee_threshold_db);
  return db2lin(y_db);
}
static float slope_at(const comp_t *c, float x, float k) {
  if (x < c->linear_threshold) return 1;
  float x2 = x * 1.001f;
  float x_db = lin2db(x), x2_db = lin2db(x2);
  float y_db = lin2db(knee_curve(c, x, k)), y2_db = lin2db(knee_curve(c, x2, k));
  return (y2_db - y_db) / (x2_db - x_db);
}
static float k_at_slope(const comp_t *c, float desired) {
  float x_db = c->db_threshold + c->db_knee, x = db2lin(x_db);
  float min_k = 0.1f, max_k = 10000, k = 5;
  for (int i = 0; i < 15; i++) {
    float slope = slope_at(c, x, k);
    if (slope < desired) max_k = k; else min_k = k;
    k = sqrtf(min_k * max_k);
  }
  return k;
}
static float update_curve(comp_t *c, float db_threshold, float db_knee, float ratio) {
  if (!c->curve_valid || db_threshold != c->db_threshold || db_knee != c->db_knee || ratio != c->ratio) {
    c->db_threshold = db_threshold;
    c->linear_threshold = db2lin(db_threshold);
    c->db_knee = db_knee;
    c->ratio = ratio;
    c->slope = 1 / ratio;
    float k = k_at_slope(c, 1 / ratio);
    c->knee_threshold_db = db_threshold + db_knee;
    c->knee_threshold = db2lin(c->knee_threshold_db);
    c->ypos = lin2db(knee_curve(c, c->knee_threshold, k));
    c->k = k;
    c->curve_valid = 1;
  }
  return c->k;
}

static void comp_process(const wa *w, comp_t *c, const float *src, float *dst) {
  const float sr = (float)w->rate;
  const float db_threshold = -24, db_knee = 30, ratio = 12, attack_time = 0.003f, release_time = 0.25f, pre_delay_time = 0.006f;
  const float z1 = 0.09f, z2 = 0.16f, z3 = 0.42f, z4 = 0.98f;
  const float PI_F = 3.14159265358979f;
  float k = update_curve(c, db_threshold, db_knee, ratio);
  float full_range_gain = saturate(c, 1, k);
  float full_range_makeup = powf(1 / full_range_gain, 0.6f);
  float master_linear_gain = db2lin(0) * full_range_makeup;
  float attack_frames = fmaxf(0.001f, attack_time) * sr;
  float release_frames = sr * release_time;
  float sat_release_frames = 0.0025f * sr;
  float y1 = release_frames * z1, y2 = release_frames * z2, y3 = release_frames * z3, y4 = release_frames * z4;
  float kA = 0.9999999999999998f * y1 + 1.8432219684323923e-16f * y2 - 1.9373394351676423e-16f * y3 + 8.824516011816245e-18f * y4;
  float kB = -1.5788320352845888f * y1 + 2.3305837032074286f * y2 - 0.9141194204840429f * y3 + 0.1623677525612032f * y4;
  float kC = 0.5334142869106424f * y1 - 1.272736789213631f * y2 + 0.9258856042207512f * y3 - 0.18656310191776226f * y4;
  float kD = 0.08783463138207234f * y1 - 0.1694162967925622f * y2 + 0.08588057951595272f * y3 - 0.00429891410546283f * y4;
  float kE = -0.042416883008123074f * y1 + 0.1115693827987602f * y2 - 0.09764676325265872f * y3 + 0.028494263462021576f * y4;
  int pdf = (int)(pre_delay_time * sr);
  if (pdf > PRE_DELAY_FRAMES - 1) pdf = PRE_DELAY_FRAMES - 1;
  if (pdf != c->last_pre_delay_frames) {
    c->last_pre_delay_frames = pdf;
    memset(c->predelay, 0, sizeof c->predelay);
    c->rd = (c->wr + PRE_DELAY_FRAMES - pdf) & (PRE_DELAY_FRAMES - 1);
  }
  int frame = 0;
  for (int div = 0; div < WA_BLOCK / DIVISION; div++) {
    float desired_gain = c->detector_avg;
    float scaled_desired_gain = asinf(desired_gain) / (0.5f * PI_F);
    float envelope_rate;
    int is_releasing = scaled_desired_gain > c->comp_gain;
    float diff_db = lin2db(c->comp_gain / scaled_desired_gain);
    if (is_releasing) {
      c->max_attack_diff_db = -1;
      if (isnan(diff_db) || isinf(diff_db)) diff_db = -1;
      float x = fminf(0.0f, fmaxf(-12.0f, diff_db));
      x = 0.25f * (x + 12);
      float x2 = x * x, x3 = x2 * x, x4 = x2 * x2;
      float rel = kA + kB * x + kC * x2 + kD * x3 + kE * x4;
      envelope_rate = db2lin(5.0f / rel); /* kSpacingDb = 5 */
    } else {
      if (isnan(diff_db) || isinf(diff_db)) diff_db = 1;
      if (c->max_attack_diff_db == -1 || c->max_attack_diff_db < diff_db) c->max_attack_diff_db = diff_db;
      float eff = fmaxf(0.5f, c->max_attack_diff_db);
      envelope_rate = 1 - powf(0.25f / eff, 1 / attack_frames);
    }
    int rd = c->rd, wr = c->wr;
    float det = c->detector_avg, cg = c->comp_gain;
    for (int i = 0; i < DIVISION; i++) {
      float in = src[frame];
      c->predelay[wr] = in;
      float abs_in = in > 0 ? in : -in;
      float shaped = saturate(c, abs_in, k);
      float attenuation = abs_in <= 0.0001f ? 1 : shaped / abs_in;
      float attenuation_db = fmaxf(2.0f, -lin2db(attenuation));
      float sat_release_rate = db2lin(attenuation_db / sat_release_frames) - 1;
      float rate = attenuation > det ? sat_release_rate : 1;
      det += (attenuation - det) * rate;
      det = fminf(1.0f, det);
      if (isnan(det) || isinf(det)) det = 1;
      if (envelope_rate < 1) cg += (scaled_desired_gain - cg) * envelope_rate;
      else { cg *= envelope_rate; cg = fminf(1.0f, cg); }
      float post = sinf(0.5f * PI_F * cg);
      dst[frame] = c->predelay[rd] * (master_linear_gain * post); /* dry_mix 0, wet_mix 1 */
      frame++;
      rd = (rd + 1) & (PRE_DELAY_FRAMES - 1);
      wr = (wr + 1) & (PRE_DELAY_FRAMES - 1);
    }
    c->rd = rd; c->wr = wr; c->detector_avg = det; c->comp_gain = cg;
  }
}

/* ===================================================================== graph rendering */
static void render_node(wa *w, int id);

/* sums audio inputs into `in` and parameter inputs into mod[0..1] (if any) */
static void gather(wa *w, node_t *n, float *in, float *mod0, float *mod1, int *has_mod0, int *has_mod1) {
  memset(in, 0, sizeof(float) * WA_BLOCK);
  *has_mod0 = *has_mod1 = 0;
  for (int e = n->first_in; e >= 0; e = w->edges[e].next) {
    const edge_t *ed = &w->edges[e];
    if (!N(w, ed->src)) continue;
    render_node(w, ed->src);
    const float *s = w->nodes[ed->src].out;
    float *d = in;
    if (ed->param == 0) { if (!*has_mod0) { memset(mod0, 0, sizeof(float) * WA_BLOCK); *has_mod0 = 1; } d = mod0; }
    else if (ed->param == 1) { if (!*has_mod1) { memset(mod1, 0, sizeof(float) * WA_BLOCK); *has_mod1 = 1; } d = mod1; }
    for (int i = 0; i < WA_BLOCK; i++) d[i] += s[i];
  }
}

static void render_node(wa *w, int id) {
  node_t *n = &w->nodes[id];
  if (n->last_block == w->block_counter) return;
  n->last_block = w->block_counter;
  const double t0 = wa_now(w), dtm = 1.0 / w->rate;
  float in[WA_BLOCK], mod0[WA_BLOCK], mod1[WA_BLOCK], p0[WA_BLOCK], p1[WA_BLOCK];
  int hm0, hm1;
  switch (n->type) {
  case N_OSC: {
    gather(w, n, in, mod0, mod1, &hm0, &hm1);
    param_block(&n->p[0], t0, dtm, p0);
    param_block(&n->p[1], t0, dtm, p1);
    render_osc(w, n, p0, p1, hm0 ? mod0 : NULL, t0, dtm, n->out);
    break;
  }
  case N_GAIN: {
    gather(w, n, in, mod0, mod1, &hm0, &hm1);
    param_block(&n->p[0], t0, dtm, p0);
    for (int i = 0; i < WA_BLOCK; i++) n->out[i] = in[i] * (p0[i] + (hm0 ? mod0[i] : 0.0f));
    break;
  }
  case N_BIQUAD: {
    gather(w, n, in, mod0, mod1, &hm0, &hm1);
    int dyn = (n->p[0].n > 0 && !(n->p[0].ev[n->p[0].n - 1].t <= t0 && n->p[0].ev[n->p[0].n - 1].type != EV_TARGET)) ||
              (n->p[1].n > 0 && !(n->p[1].ev[n->p[1].n - 1].t <= t0 && n->p[1].ev[n->p[1].n - 1].type != EV_TARGET)) || hm0 || hm1;
    param_block(&n->p[0], t0, dtm, p0);
    param_block(&n->p[1], t0, dtm, p1);
    if (!dyn) biquad_coeffs(w, n, p0[0], p1[0]);
    for (int i = 0; i < WA_BLOCK; i++) {
      if (dyn) biquad_coeffs(w, n, p0[i] + (hm0 ? mod0[i] : 0), p1[i] + (hm1 ? mod1[i] : 0));
      double x = in[i];
      double y = n->b0 * x + n->b1 * n->x1 + n->b2 * n->x2 - n->a1 * n->y1 - n->a2 * n->y2;
      n->x2 = n->x1; n->x1 = x; n->y2 = n->y1; n->y1 = y;
      n->out[i] = (float)y;
    }
    break;
  }
  case N_NOISE: {
    for (int i = 0; i < WA_BLOCK; i++) {
      double t = t0 + i * dtm;
      if (t < n->start_t || n->noise_pos >= n->noise_len) { n->out[i] = 0; continue; }
      float v = w->noise[(n->noise_off + n->noise_pos) & (NOISE_TAB - 1)];
      if (n->noise_fade) v *= 1.0f - (float)n->noise_pos / (float)n->noise_len;
      n->out[i] = v;
      n->noise_pos++;
    }
    break;
  }
  case N_COMP: {
    gather(w, n, in, mod0, mod1, &hm0, &hm1);
    comp_process(w, &w->comps[n->comp_idx], in, n->out);
    break;
  }
  case N_DEST: {
    gather(w, n, in, mod0, mod1, &hm0, &hm1);
    memcpy(n->out, in, sizeof n->out);
    break;
  }
  default: memset(n->out, 0, sizeof n->out); break;
  }
}

/* ===================================================================== garbage collection */
static int has_inputs(wa *w, const node_t *n) {
  for (int e = n->first_in; e >= 0; e = w->edges[e].next)
    if (N(w, w->edges[e].src)) return 1;
  return 0;
}

static void collect(wa *w, double t_end) {
  char dead[MAX_NODES];
  int any = 0;
  for (int i = 0; i < MAX_NODES; i++) {
    dead[i] = 0;
    node_t *n = &w->nodes[i];
    if (n->type == N_FREE || n->type == N_DEST) continue;
    int d = 0;
    if (n->die_at <= t_end) d = 1;
    else if (!n->pinned) {
      switch (n->type) {
      case N_OSC: d = t_end >= n->stop_t; break;
      case N_NOISE: d = n->start_t != INFINITY && t_end >= n->start_t + (double)n->noise_len / w->rate; break;
      case N_GAIN: d = !has_inputs(w, n); break;
      case N_BIQUAD: case N_COMP:
        if (has_inputs(w, n)) n->idle_since = -1;
        else {
          if (n->idle_since < 0) n->idle_since = t_end;
          d = t_end - n->idle_since > (n->type == N_COMP ? 0.2 : 0.1);
        }
        break;
      default: break;
      }
    }
    if (d) { dead[i] = 1; any = 1; }
  }
  if (!any) return;
  for (int i = 0; i < MAX_NODES; i++) { /* rebuild each live node's incoming list without dead sources */
    node_t *n = &w->nodes[i];
    if (n->type == N_FREE) continue;
    int *link = &n->first_in;
    while (*link >= 0) {
      edge_t *ed = &w->edges[*link];
      if (dead[ed->src] || dead[i]) {
        int next = ed->next, idx = *link;
        w->free_edges[w->nfree_edges++] = idx;
        *link = next;
      } else link = &ed->next;
    }
  }
  for (int i = 0; i < MAX_NODES; i++) if (dead[i]) free_node(w, i);
}

/* ===================================================================== block loop */
static void render_block(wa *w) {
  if (w->hook) w->hook(w, w->hook_user);
  w->block_counter++;
  render_node(w, w->dest);
  const float *m = w->nodes[w->dest].out;
  for (int i = 0; i < WA_BLOCK; i++) {
    float v = m[i];
    v = v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
    w->stage[i * 2] = w->stage[i * 2 + 1] = v;
  }
  double t_end = (double)(w->frames + WA_BLOCK) / w->rate;
  for (int i = 0; i < MAX_NODES; i++) {
    node_t *n = &w->nodes[i];
    if (n->type == N_FREE) continue;
    param_prune(&n->p[0], t_end);
    param_prune(&n->p[1], t_end);
  }
  collect(w, t_end);
  w->frames += WA_BLOCK;
  w->stage_pos = 0;
}

void wa_render(wa *w, float *out, int frames) {
  while (frames > 0) {
    if (w->stage_pos >= WA_BLOCK) render_block(w);
    int n = WA_BLOCK - w->stage_pos;
    if (n > frames) n = frames;
    memcpy(out, &w->stage[w->stage_pos * 2], sizeof(float) * 2 * (size_t)n);
    out += 2 * n;
    w->stage_pos += n;
    frames -= n;
  }
}
