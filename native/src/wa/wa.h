/* wa.h - a tiny Web-Audio-style synthesis engine (mono graph, stereo out).
 *
 * Nodes: oscillator, gain, biquad (lowpass/highpass/bandpass), noise burst,
 * dynamics compressor, destination. Parameters carry the Web Audio automation
 * timeline (setValueAtTime, linear/exponential ramps, setTargetAtTime,
 * cancelScheduledValues) and may take audio-rate modulation (LFO -> gain.gain).
 *
 * Threading: the graph and all wa_* calls belong to ONE thread (the audio
 * callback, or the test thread when rendering offline). Nothing here allocates
 * or locks after wa_create(). The game thread talks to the engine through the
 * command queue in sound/, never through this API.
 *
 * Lifetime: nodes are garbage collected. A source (osc/noise) dies when it has
 * stopped; gain/biquad/compressor die when all their inputs are gone (biquads
 * after a short tail). wa_pin()/wa_die_at() override this for long-lived nodes.
 */
#ifndef WA_H
#define WA_H
#include <stdint.h>

#define WA_BLOCK 128 /* render quantum, like Web Audio */

typedef int wa_id;
#define WA_NONE (-1)

typedef enum { WA_SINE, WA_SQUARE, WA_SAWTOOTH, WA_TRIANGLE } wa_wave;
typedef enum { WA_LOWPASS, WA_HIGHPASS, WA_BANDPASS } wa_filter;
enum { WA_P_FREQ = 0, WA_P_GAIN = 0, WA_P_DETUNE = 1, WA_P_Q = 1 };

typedef struct wa wa;
typedef void (*wa_block_hook)(wa *w, void *user); /* called at the start of every block */

wa *wa_create(int sample_rate, uint32_t noise_seed);
void wa_destroy(wa *w);
void wa_set_block_hook(wa *w, wa_block_hook fn, void *user);
double wa_now(const wa *w);       /* currentTime: time at the start of the current block */
int wa_sample_rate(const wa *w);
void wa_render(wa *w, float *stereo_out, int frames); /* interleaved L,R */
int wa_node_count(const wa *w);   /* live nodes (diagnostics) */
int wa_node_peak(const wa *w);    /* highest live node count seen */
int wa_alloc_failures(const wa *w); /* creations that failed because a pool was full */

/* node creation. Returns WA_NONE if the pool is full (callers may ignore: all other
 * calls accept WA_NONE as a no-op). */
wa_id wa_osc(wa *w, wa_wave type);
wa_id wa_gain(wa *w, double value);
wa_id wa_biquad(wa *w, wa_filter type);
wa_id wa_noise(wa *w, double duration_s, int linear_fade);
wa_id wa_compressor(wa *w);       /* default threshold -24 dB, knee 30, ratio 12, attack .003, release .25 */
wa_id wa_destination(wa *w);

void wa_connect(wa *w, wa_id from, wa_id to);                 /* audio connection */
void wa_connect_param(wa *w, wa_id from, wa_id to, int param); /* modulate a parameter */
void wa_start(wa *w, wa_id n, double when);
void wa_stop(wa *w, wa_id n, double when);
void wa_pin(wa *w, wa_id n);                    /* never garbage collect */
void wa_die_at(wa *w, wa_id n, double when);    /* force-free at a time (pins until then) */

/* AudioParam */
void wa_set_value(wa *w, wa_id n, int param, double v);               /* param.value = v */
void wa_set_at(wa *w, wa_id n, int param, double v, double t);        /* setValueAtTime */
void wa_linear_to(wa *w, wa_id n, int param, double v, double t);     /* linearRampToValueAtTime */
void wa_exp_to(wa *w, wa_id n, int param, double v, double t);        /* exponentialRampToValueAtTime */
void wa_target(wa *w, wa_id n, int param, double v, double t, double tau); /* setTargetAtTime */
void wa_cancel(wa *w, wa_id n, int param, double t);                  /* cancelScheduledValues */
double wa_param_at(const wa *w, wa_id n, int param, double t);        /* value of the timeline at t (tests) */
#endif
