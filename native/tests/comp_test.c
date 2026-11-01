/* comp_test.c - sine of a given amplitude through the default compressor; writes a mono WAV.
 * usage: comp_test amplitude out.wav [seconds] */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "wa/wa.h"
int main(int argc, char **argv) {
  double amp = atof(argv[1]), secs = argc > 3 ? atof(argv[3]) : 1.5;
  int SR = 48000, n = (int)(secs * SR);
  wa *w = wa_create(SR, 1);
  wa_id o = wa_osc(w, WA_SINE), g = wa_gain(w, amp), c = wa_compressor(w);
  wa_set_value(w, o, WA_P_FREQ, 440);
  wa_connect(w, o, g); wa_connect(w, g, c); wa_connect(w, c, wa_destination(w));
  wa_start(w, o, 0);
  float *st = malloc(sizeof(float) * 2 * (size_t)n);
  wa_render(w, st, n);
  FILE *f = fopen(argv[2], "wb");
  uint32_t data = (uint32_t)n * 2, riff = 36 + data, rate = SR, brate = SR * 2, fmt = 16; uint16_t pcm = 1, ch = 1, al = 2, bits = 16;
  fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmt, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f);
  fwrite(&rate, 4, 1, f); fwrite(&brate, 4, 1, f); fwrite(&al, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
  for (int i = 0; i < n; i++) { float v = st[2 * i]; int16_t s = (int16_t)lrintf((v > 1 ? 1 : v < -1 ? -1 : v) * 32767); fwrite(&s, 2, 1, f); }
  fclose(f);
  free(st);
  wa_destroy(w);
  return 0;
}
