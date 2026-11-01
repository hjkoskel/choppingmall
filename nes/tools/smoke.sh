#!/bin/sh
# usage: tools/smoke.sh [rom]  - emulator smoke test (fceux + Lua, headless)
cd "$(dirname "$0")/.."
rom=${1:-build/chopping-mall.nes}
lbl=${rom%.nes}.lbl; [ -f "$lbl" ] || lbl=build/plat_nes.lbl
GA=$(grep " \._G\$" "$lbl" | awk '{print $2}' | sed 's/^0*//')
rm -f build/smoke.out
GADDR=$GA OUT=$PWD/build/smoke.out timeout 120 xvfb-run -a /usr/games/fceux --loadlua tools/smoke.lua "$rom" >/dev/null 2>&1
cat build/smoke.out; grep -q "SMOKE OK" build/smoke.out
