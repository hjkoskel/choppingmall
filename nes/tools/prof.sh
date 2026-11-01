#!/bin/sh
# usage: tools/prof.sh [cc65 flags] - where the video frames go in the platform loop while playing (frames per 600): game tick vs HUD+sprites
cd "$(dirname "$0")/.."
sh build.sh plat_nes -DNES_PROF "$@" 2>&1 | grep -v "never used\|nused"
a() { grep " \._$1\$" build/plat_nes.lbl | awk '{print $2}' | sed 's/^0*//'; }
cat > build/prof.lua <<'EOL'
local function rd(a) return memory.readbyte(a)+256*memory.readbyte(a+1) end
local TA,DA,IA=tonumber(os.getenv("TA"),16),tonumber(os.getenv("DA"),16),tonumber(os.getenv("IA"),16)
for i=1,10 do emu.frameadvance() end joypad.set(1,{start=true}) emu.frameadvance() emu.frameadvance()
for i=1,230 do emu.frameadvance() end
local t0,d0=rd(TA),rd(DA)
for i=1,600 do joypad.set(1,{left=math.floor(i/100)%2==0,right=math.floor(i/100)%2==1,up=math.floor(i/150)%2==1,A=(i%20)<3}); emu.frameadvance() end
local o=io.open(os.getenv("OUT"),"w"); o:write(string.format("per 600 frames: tick part %d frames, draw part %d frames\n",rd(TA)-t0,rd(DA)-d0)); o:close(); emu.exit()
EOL
TA=$(a prof_tick) DA=$(a prof_draw) IA=$(a prof_iter) OUT=$PWD/build/prof.out timeout 100 xvfb-run -a /usr/games/fceux --loadlua build/prof.lua build/plat_nes.nes >/dev/null 2>&1
echo "[$*] $(cat build/prof.out)"
