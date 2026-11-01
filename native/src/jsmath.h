/* jsmath.h - helpers that reproduce JavaScript Math semantics exactly. */
#ifndef JSMATH_H
#define JSMATH_H
#include <math.h>
#include <stdint.h>

/* js: Math.hypot(a, b) as implemented by V8 (scale by max, Kahan-sum squares).
 * glibc's hypot is correctly rounded and can differ in the last bit, which
 * could flip a distance threshold, so mirror V8 instead. */
static inline double js_hypot(double a, double b) {
  double x = fabs(a), y = fabs(b);
  if (isinf(x) || isinf(y)) return INFINITY;
  if (isnan(x) || isnan(y)) return NAN;
  double max = x > y ? x : y;
  if (max == 0.0) return 0.0;
  double sum = 0.0, comp = 0.0;
  double v[2] = {x, y};
  for (int i = 0; i < 2; i++) {
    double n = v[i] / max;
    double summand = n * n - comp;
    double prelim = sum + summand;
    comp = (prelim - sum) - summand;
    sum = prelim;
  }
  return sqrt(sum) * max;
}

/* js: Math.sign */
static inline double js_sign(double x) {
  if (x > 0) return 1;
  if (x < 0) return -1;
  return x; /* 0, -0 or NaN */
}

static inline double js_max(double a, double b) { return a > b ? a : b; }
static inline double js_min(double a, double b) { return a < b ? a : b; }

#endif
