-- rot.lua: cycles player and robot through all 32 directions, saving a screenshot each (contact sheet input). env GADDR OUTDIR
local GA=tonumber(os.getenv("GADDR"),16); local d=os.getenv("OUTDIR")
local function rd16(a) local v=memory.readbyte(a)+256*memory.readbyte(a+1); if v>=32768 then v=v-65536 end return v end
for i=1,10 do emu.frameadvance() end joypad.set(1,{start=true}) emu.frameadvance() emu.frameadvance()
for i=1,200 do emu.frameadvance() end
local o=io.open(d.."/rot.txt","w")
for k=0,31 do
  memory.writebyte(GA+41, k); memory.writebyte(GA+54, k)   -- pface, rface
  emu.frameadvance()
  o:write(string.format("%d %d %d %d %d\n",k,math.floor(rd16(GA+35)/16),math.floor(rd16(GA+37)/16),math.floor(rd16(GA+46)/16),math.floor(rd16(GA+48)/16)))
  gui.savescreenshotas(d.."/rot"..string.format("%02d",k)..".png")
end
o:close(); emu.exit()
