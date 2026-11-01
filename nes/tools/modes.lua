-- modes.lua: forces each game mode and saves a screenshot (overlay check). env GADDR OUTDIR
local GA=tonumber(os.getenv("GADDR"),16); local d=os.getenv("OUTDIR")
for i=1,10 do emu.frameadvance() end joypad.set(1,{start=true}) emu.frameadvance() emu.frameadvance()
for i=1,200 do emu.frameadvance() end
memory.writebyte(GA+8,3)              -- level 3: all weapons
memory.writebyte(GA+11,12) memory.writebyte(GA+12,7) memory.writebyte(GA+13,5)   -- ammo
memory.writebyte(GA+10,1)             -- shotgun selected
memory.writebyte(GA+3,0x39) memory.writebyte(GA+4,0x30) -- score 12345
for i=1,30 do emu.frameadvance() end
gui.savescreenshotas(d.."/m_play3.png")
local names={[3]="lost",[4]="clear",[5]="over",[6]="win"}
for _,m in ipairs({3,4,5,6}) do
  memory.writebyte(GA,m); memory.writebyte(GA+1,200); memory.writebyte(GA+2,0)   -- mode, mt (16 bit)
  for i=1,20 do emu.frameadvance() end
  gui.savescreenshotas(d.."/m_"..names[m]..".png")
end
emu.exit()
