#!/bin/sh
# usage: tools/fps.sh [cc65 flags]  - core ticks the platform loop manages per 60 video frames while playing (ideal: 50)
cd "$(dirname "$0")/.."
sh build.sh plat_nes "$@" 2>&1 | grep -v "never used\|nused"
GA=$(grep " \._G\$" build/plat_nes.lbl | awk '{print $2}' | sed 's/^0*//')
cat > build/fps.lua <<'EOL'
local GA=tonumber(os.getenv("GADDR"),16)
for i=1,10 do emu.frameadvance() end joypad.set(1,{start=true}) emu.frameadvance() emu.frameadvance()
for i=1,230 do emu.frameadvance() end
local total, last = 0, memory.readbyte(GA+34)
for i=1,600 do joypad.set(1,{left=math.floor(i/100)%2==0,right=math.floor(i/100)%2==1,up=math.floor(i/150)%2==1,A=(i%20)<3}); emu.frameadvance()
  if i%30==0 then local t=memory.readbyte(GA+34); total=total+(t-last)%256; last=t end end
local o=io.open(os.getenv("OUT"),"w"); o:write(string.format("ticks in 600 frames: %d (ideal 500)  mode=%d\n",total,memory.readbyte(GA))); o:close(); emu.exit()
EOL
GADDR=$GA OUT=$PWD/build/fps.out timeout 100 xvfb-run -a /usr/games/fceux --loadlua build/fps.lua build/plat_nes.nes >/dev/null 2>&1
echo "[$*] $(cat build/fps.out)"
