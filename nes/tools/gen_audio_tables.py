#!/usr/bin/env python3
"""NTSC APU period tables (pulse: CPU/(16*f)-1, triangle: CPU/(32*f)-1) for MIDI notes 0..95 -> src/nes_audio_gen.h. usage: gen_audio_tables.py > src/nes_audio_gen.h"""
NTSC, PAL = 1789773, 1662607   # CPU clock, Hz
def per_p(n, clk=NTSC):
    f = 440 * 2 ** ((n - 69) / 12); return max(8, min(2047, round(clk / (16 * f)) - 1))
def per_t(n, clk=NTSC):
    f = 440 * 2 ** ((n - 69) / 12); return max(2, min(2047, round(clk / (32 * f)) - 1))
print('/* nes_audio_gen.h - GENERATED (tools/gen_audio_tables.py). APU timer periods (NTSC: PER_P/PER_T, PAL: PER_PP/PER_TP) for MIDI notes 0..95 */\n#ifndef NES_AUDIO_GEN_H\n#define NES_AUDIO_GEN_H\n#include "types.h"')
for nm, fn in (('PER_P', per_p), ('PER_T', per_t), ('PER_PP', lambda n: per_p(n, PAL)), ('PER_TP', lambda n: per_t(n, PAL))):
    print('static const u16 %s[96] = {' % nm)
    v = [fn(n) for n in range(96)]
    for i in range(0, 96, 12): print('  ' + ', '.join(str(x) + 'u' for x in v[i:i + 12]) + ',')
    print('};')
print('#endif')
