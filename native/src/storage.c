/* storage.c - js: saveBest and the localStorage load. Best run kept in a text file. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "game.h"
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir(p, 0755)
#endif

static int best_path(char *out, size_t n, int make_dir) {
  const char *over = getenv("CHOPPING_MALL_BEST"); /* test override */
  if (over && *over) { snprintf(out, n, "%s", over); return 1; }
  char dir[512];
#ifdef _WIN32
  const char *base = getenv("APPDATA");
  if (!base) return 0;
  snprintf(dir, sizeof dir, "%s\\chopping-mall", base);
#elif defined(__APPLE__)
  const char *home = getenv("HOME");
  if (!home) return 0;
  snprintf(dir, sizeof dir, "%s/Library/Application Support/chopping-mall", home);
#else
  const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
  if (xdg && *xdg) snprintf(dir, sizeof dir, "%s/chopping-mall", xdg);
  else if (home) snprintf(dir, sizeof dir, "%s/.config/chopping-mall", home);
  else return 0;
#endif
  if (make_dir) {
#ifndef _WIN32
    char parent[512];
    snprintf(parent, sizeof parent, "%s", dir);
    char *s = strrchr(parent, '/');
    if (s) { *s = 0; MKDIR(parent); }
#endif
    MKDIR(dir);
  }
  snprintf(out, n, "%s/best.txt", dir);
  return 1;
}

void storage_load_best(Game *g) {
  char path[600];
  g->best.valid = 0;
  if (!best_path(path, sizeof path, 0)) return;
  FILE *f = fopen(path, "r");
  if (!f) return;
  long s, t, k;
  if (fscanf(f, "score=%ld time=%ld kills=%ld", &s, &t, &k) == 3) {
    g->best.valid = 1; g->best.score = s; g->best.time = t; g->best.kills = k;
  }
  fclose(f);
}

/* js: saveBest */
void save_best(Game *g) {
  long sc = (long)floor(g->S.score);
  if (!g->best.valid || sc > g->best.score) {
    g->best.valid = 1;
    g->best.score = sc;
    g->best.time = (long)floor(g->S.time);
    g->best.kills = g->S.kills;
    char path[600];
    if (!best_path(path, sizeof path, 1)) return;
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "score=%ld time=%ld kills=%ld\n", g->best.score, g->best.time, g->best.kills);
    fclose(f);
  }
}
