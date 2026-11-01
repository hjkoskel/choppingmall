#!/bin/sh
# usage: tools/timing.sh [extra cc65 flags]  - frames the emulated NES needs for 3000 core ticks (game_tick only, fixed input pattern)
export BOT=1
cd "$(dirname "$0")/.."
sh build.sh parity_nes -DPAR_TICKS=3000 -DPAR_TIMING "$@" 2>&1 | grep -v "never used"
addr() { grep " \._$1\$" build/parity_nes.lbl | awk '{print $2}' | sed 's/^0*/0x/'; }
printf 'A_acc=%s\nA_ticks=%s\nA_done=%s\n' "$(addr par_acc)" "$(addr par_ticks)" "$(addr par_done)" > build/addrs.lua
ADDRS=$PWD/build/addrs.lua OUT=$PWD/build/t.out timeout 100 xvfb-run -a /usr/games/fceux --loadlua tools/lua_read.lua build/parity_nes.nes >/dev/null 2>&1
echo "[$*] $(cat build/t.out)"
