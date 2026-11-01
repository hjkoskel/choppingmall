/* audio_render.c - renders a sound scenario offline through the real engine to a mono 16-bit WAV.
 * usage: audio_render <scenario> <seconds> out.wav
 * scenarios: sfx:f,d,type,v,f2 | clank:v | engine:v,f | music | win | lose | mix (music, then win at 3 s) */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sound/sound.h"
#include "sound/sound_engine.h"

#define SR 48000
static void write_wav(const char *path, const float *x, int n) {
  FILE *f = fopen(path, "wb");
  uint32_t data = (uint32_t)n * 2, riff = 36 + data, rate = SR, brate = SR * 2, fmt = 16;
  uint16_t pcm = 1, ch = 1, align = 2, bits = 16;
  fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmt, 4, 1, f);
  fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&brate, 4, 1, f);
  fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
  for (int i = 0; i < n; i++) { float v = x[i] > 1 ? 1 : x[i] < -1 ? -1 : x[i]; int16_t s = (int16_t)lrintf(v * 32767); fwrite(&s, 2, 1, f); }
  fclose(f);
}
int main(int argc, char **argv) {
  if (argc < 4) return 2;
  double secs = atof(argv[2]);
  int n = (int)(secs * SR);
  float *mono = calloc((size_t)n, sizeof(float)), *buf = malloc(sizeof(float) * 2 * 512);
  snd_engine_init(SR);
  const char *sc = argv[1];
  int engine = 0; double ev = 0, ef = 0;
  if (!strncmp(sc, "sfx:", 4)) {
    char ty[32]; double f, d, v, f2;
    sscanf(sc + 4, "%lf,%lf,%31[^,],%lf,%lf", &f, &d, ty, &v, &f2);
    SndWave w = !strcmp(ty, "sine") ? SND_SINE : !strcmp(ty, "square") ? SND_SQUARE : !strcmp(ty, "sawtooth") ? SND_SAWTOOTH : SND_TRIANGLE;
    snd_sfx(f, d, w, v, f2);
  } else if (!strncmp(sc, "clank:", 6)) snd_clank(atof(sc + 6));
  else if (!strncmp(sc, "engine:", 7)) { sscanf(sc + 7, "%lf,%lf", &ev, &ef); engine = 1; }
  else if (!strcmp(sc, "music")) snd_new_game();
  else if (!strcmp(sc, "win")) snd_play_jingle(JINGLE_WIN);
  else if (!strcmp(sc, "lose")) snd_play_jingle(JINGLE_LOSE);
  else if (!strcmp(sc, "mix")) snd_new_game();
  int done = 0, next_frame = 0, mixed = 0;
  while (done < n) {
    int chunk = n - done < 128 ? n - done : 128;
    if (engine && done >= next_frame) { snd_engine(ev, ef); next_frame += SR / 60; }
    if (!strcmp(sc, "mix") && !mixed && done >= 3 * SR) { snd_play_jingle(JINGLE_WIN); mixed = 1; }
    snd_engine_render(buf, chunk);
    for (int i = 0; i < chunk; i++) mono[done + i] = buf[2 * i];
    done += chunk;
  }
  write_wav(argv[3], mono, n);
  const wa *w = snd_engine_wa();
  printf("%s: %.1fs peak nodes %d, alloc failures %d, dropped cmds %d\n", sc, secs, wa_node_peak(w), wa_alloc_failures(w), snd_dropped_commands());
  snd_engine_shutdown();
  free(mono); free(buf);
  return 0;
}
