/* world.c - js: N polar maxDot SH OB MAXA WN CFG valid */
#include "world.h"
#include <math.h>

double D30;
Normal N[6];
Obstacle OB[CAP_OB];
const int OB_COUNT = 18;

const Shop SH[6] = {
    {"SHELLS", "#d9503f", 1}, {"GAS", "#3fae6a", 2},       {"FIRST AID", "#4a90d9", 3},
    {"AMMO", "#e8a33d", 0},   {"SHELLS", "#d9503f", 1},    {"AMMO", "#e8a33d", 0},
};
const int MAXA[3] = {60, 24, 6};
const char *const WN[3] = {"PISTOL", "SHOTGUN", "MOLOTOV"};
const Cfg CFG = {
    .playerSpeed = 165,
    .aimDelay = 0.3,
    .robotHp = {30, 45, 60},
    .robotSpeed = {118, 130, 142},
    .robotShotInterval = {2.6, 2.2, 1.8},
    .shopCooldown = 22,
    .shopRestock = {18, 6, 2},
    .medkit = 45,
};

void polar(int k, double d, double t, double *ox, double *oy) {
  *ox = MALL_CX + N[k].x * d - N[k].y * t;
  *oy = MALL_CY + N[k].y * d + N[k].x * t;
}

double max_dot(double x, double y) {
  double m = -1e9;
  for (int i = 0; i < 6; i++) {
    double v = (x - MALL_CX) * N[i].x + (y - MALL_CY) * N[i].y;
    if (v > m) m = v; /* js: Math.max(m, ...) */
  }
  return m;
}

int valid(double x, double y, double r) {
  double m = max_dot(x, y);
  if (m > MALL_AO - r || m < MALL_AI + r) return 0;
  for (int i = 0; i < OB_COUNT; i++) {
    double dx = x - OB[i].x, dy = y - OB[i].y, rr = OB[i].r + r;
    if (dx * dx + dy * dy < rr * rr) return 0;
  }
  return 1;
}

void world_init(void) {
  D30 = cos(PI_D / 6);
  for (int k = 0; k < 6; k++) {
    double a = ((30 + 60 * k) * PI_D) / 180; /* js: ((30 + 60 * k) * Math.PI) / 180 */
    N[k].x = cos(a);
    N[k].y = sin(a);
    N[k].a = a;
  }
  static const ObType t0[6] = {OB_BENCH, OB_PLANT, OB_BENCH, OB_KIOSK, OB_PLANT, OB_BENCH};
  static const ObType t1[2] = {OB_PLANT, OB_BIN};
  static const ObType t2[3] = {OB_KIOSK, OB_BENCH, OB_PLANT};
  static const double tt[3] = {-105, 75, 118}, dd[3] = {262, 188, 258};
  int n = 0;
  for (int k = 0; k < 6; k++) {
    ObType ty[3] = {t0[k], t1[k % 2], t2[k % 3]};
    for (int i = 0; i < 3; i++) {
      double x, y;
      polar(k, dd[i], (k % 2) ? -tt[i] : tt[i], &x, &y);
      ObType type = ty[i];
      double r = type == OB_PLANT ? 17 : type == OB_BENCH ? 20 : type == OB_BIN ? 12 : 26;
      OB[n].x = x; OB[n].y = y; OB[n].type = type; OB[n].r = r;
      OB[n].rot = N[k].a + PI_D / 2;
      n++;
    }
  }
}
