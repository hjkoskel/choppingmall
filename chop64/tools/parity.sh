#!/bin/sh
# Host-vs-6502 parity: the bot plays N ticks on both and each folds the whole game state into a 16-bit hash every tick.
# usage: tools/parity.sh N [cc65 optimiser flags, default -Oirs]
# (the C64 build is slow in this mode, so it needs ~100 K emulated cycles per tick; the hash is read off the screen)
set -e
n=${1:-600}; opt=${2:--Oirs}; cd "$(dirname "$0")/.."
mkdir -p build
gcc -std=c99 -O2 -Isrc tests/hash_host.c src/core.c src/bot.c src/plat_host.c -o build/hash_host
host=$(build/hash_host "$n" | cut -d' ' -f1)
cl65 -t c64 $opt -Cl -Isrc -Wl -D,__STACKSIZE__=0x300 -DAUTOPLAY -DAUTOPLAY_TICKS="$n" -DDEBUG_HASH -o build/parity.prg src/plat_c64.c src/core.c src/mcfont.c src/bot.c
tools/vice_run.sh build/parity.prg $((n * 220000 + 60000000)) build/parity.png
c64=$(python3 tools/readscreen.py build/parity.png 0 10 5)
echo "N=$n  host=$host  c64=$c64"
[ "$host" = "$c64" ] && echo PARITY OK || { echo PARITY MISMATCH; exit 1; }
