/* rng.h - seedable PRNG replacing Math.random. mulberry32, identical to the
 * generator the HTML test mode uses, so recorded traces can be replayed. */
#ifndef RNG_H
#define RNG_H
#include <stdint.h>

static inline double rng_next(uint32_t *state) {
  *state += 0x6D2B79F5u;
  uint32_t t = *state;
  t = (t ^ (t >> 15)) * (1u | t);
  t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
  return (double)(t ^ (t >> 14)) / 4294967296.0;
}
#endif
