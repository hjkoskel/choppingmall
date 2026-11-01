-- shots.lua: title -> start -> gameplay screenshots. env OUTDIR
local d=os.getenv("OUTDIR")
local function shot(n) gui.savescreenshotas(d.."/"..n..".png") end
for i=1,20 do emu.frameadvance() end
shot("s1_title")
joypad.set(1,{start=true}); emu.frameadvance(); emu.frameadvance()
for i=1,40 do emu.frameadvance() end
shot("s2_intro")
for i=1,200 do emu.frameadvance() end
shot("s3_play")
for i=1,60 do joypad.set(1,{left=true,A=true}); emu.frameadvance() end
shot("s4_move")
emu.exit()
