-- apulog.lua: starts the game, idles (the robot eventually kills the player -> lose jingle) and logs every APU register write: frame addr value
local o=io.open(os.getenv("OUT"),"w"); local fr=0
for a=0x4000,0x4015 do memory.registerwrite(a,1,function(addr,size,value) o:write(string.format("%d %04X %02X\n",fr,addr,value)) end) end
local frames=tonumber(os.getenv("FRAMES") or "3000")
for i=1,10 do fr=fr+1 emu.frameadvance() end joypad.set(1,{start=true}) fr=fr+1 emu.frameadvance() fr=fr+1 emu.frameadvance()
for i=1,frames do fr=fr+1 emu.frameadvance() end
o:close(); emu.exit()
