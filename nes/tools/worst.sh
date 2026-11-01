#!/bin/sh
# usage: tools/worst.sh N [flags] - histogram of game_tick durations in video frames while the bot plays N ticks
export BOT=1
cd "$(dirname "$0")/.."; n=$1; shift
sh build.sh parity_nes -DPAR_TICKS=$n -DPAR_WORST "$@" 2>&1 | grep -v "never used"
addr() { grep " \._$1\$" build/parity_nes.lbl | awk '{print $2}' | sed 's/^0*/0x/'; }
printf 'A_acc=%s\nA_ticks=%s\nA_done=%s\nA_hist=%s\n' "$(addr par_acc)" "$(addr par_ticks)" "$(addr par_done)" "$(addr par_hist)" > build/addrs.lua
cat > build/w.lua <<'EOL'
dofile(os.getenv("ADDRS")); emu.speedmode("turbo")
for i=1,5 do emu.frameadvance() end
local f=0
while memory.readbyte(A_done)==0 and f<200000 do emu.frameadvance(); f=f+1 end
local o=io.open(os.getenv("OUT"),"w"); o:write(string.format("ticks=%d  <1frame:%d  1:%d  2:%d  >=3:%d\n", memory.readbyte(A_ticks)+256*memory.readbyte(A_ticks+1), memory.readbyte(A_hist), memory.readbyte(A_hist+1), memory.readbyte(A_hist+2), memory.readbyte(A_hist+3))); o:close(); emu.exit()
EOL
ADDRS=$PWD/build/addrs.lua OUT=$PWD/build/w.out timeout 600 xvfb-run -a /usr/games/fceux --loadlua build/w.lua build/parity_nes.nes >/dev/null 2>&1
echo "[$*] $(cat build/w.out)"
