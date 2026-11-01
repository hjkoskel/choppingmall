/* world.h - mall geometry, shops, obstacles, tuning. js: CANVAS, GEOMETRY AND TUNING */
#ifndef WORLD_H
#define WORLD_H
#include "config.h"

typedef struct { double x, y, a; } Normal;
typedef enum { OB_PLANT, OB_BENCH, OB_BIN, OB_KIOSK } ObType;
typedef struct { double x, y; ObType type; double r, rot; } Obstacle;
typedef struct { const char *n; const char *c; int k; } Shop;
typedef struct {
  double playerSpeed, aimDelay;
  double robotHp[3], robotSpeed[3], robotShotInterval[3];
  double shopCooldown;
  int shopRestock[3];
  int medkit;
} Cfg;

extern double D30;
extern Normal N[6];
extern Obstacle OB[CAP_OB];
extern const int OB_COUNT;
extern const Shop SH[6];
extern const int MAXA[3];
extern const char *const WN[3];
extern const Cfg CFG;

void world_init(void); /* fills N, D30 and OB */
void polar(int k, double d, double t, double *ox, double *oy); /* js: polar */
double max_dot(double x, double y);                            /* js: maxDot */
int valid(double x, double y, double r);                       /* js: valid */
#endif
