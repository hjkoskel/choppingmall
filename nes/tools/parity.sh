#!/bin/sh
# Host-vs-NES (6502 in fceux) parity: usage tools/parity.sh [N]
set -e
n=${1:-600}; export BOT=1
cd "$(dirname "$0")/.."
mkdir -p build
gcc -std=c99 -O2 -I../chop64/src ../chop64/tests/hash_host.c ../chop64/src/core.c ../chop64/src/bot.c ../chop64/src/plat_host.c -o build/hash_host
host=$(build/hash_host "$n" | cut -d' ' -f1)
sh build.sh parity_nes -DPAR_TICKS="$n" 2>&1 | grep -v "never used" || true
addr() { grep " \._$1\$" build/parity_nes.lbl | awk '{print $2}' | sed 's/^0*/0x/'; }
printf 'A_acc=%s\nA_ticks=%s\nA_done=%s\n' "$(addr par_acc)" "$(addr par_ticks)" "$(addr par_done)" > build/addrs.lua
rm -f build/parity.out
ADDRS=$PWD/build/addrs.lua OUT=$PWD/build/parity.out timeout 600 xvfb-run -a /usr/games/fceux --loadlua tools/lua_read.lua build/parity_nes.nes >/dev/null 2>&1 || true
nes=$(cut -d' ' -f1 build/parity.out)
echo "N=$n  host=$host  nes=$nes  ($(cat build/parity.out))"
[ "$host" = "$nes" ] && echo PARITY OK || { echo PARITY MISMATCH; exit 1; }
