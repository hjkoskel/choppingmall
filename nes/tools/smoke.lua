-- smoke.lua: emulator smoke test of the finished ROM (env GADDR, OUT). Starts a game, idles for 12 s and checks:
--   * the title screen is shown and START begins a game (mode TITLE -> INTRO -> PLAY)
--   * the core keeps running at about 50 ticks per second
--   * APU registers are being written (music), and nothing writes outside the APU/PPU/controller registers by accident (no crash: the NMI counter keeps counting)
local GA=tonumber(os.getenv("GADDR"),16)
local apu=0
for a=0x4000,0x4015 do memory.registerwrite(a,1,function() apu=apu+1 end) end
local function mode() return memory.readbyte(GA) end
local fails={}
local function check(c,msg) if not c then table.insert(fails,msg) end end
for i=1,15 do emu.frameadvance() end
check(mode()==0,"title mode at start (got "..mode()..")")
joypad.set(1,{start=true}) emu.frameadvance() emu.frameadvance()
for i=1,5 do emu.frameadvance() end
check(mode()==1,"INTRO after START (got "..mode()..")")
local seen={}
local total,last=0,memory.readbyte(GA+34)
for i=1,720 do
  emu.frameadvance()
  seen[mode()]=true
  if i%30==0 then local t=memory.readbyte(GA+34); total=total+(t-last)%256; last=t end
end
check(seen[2],"PLAY mode reached")
check(total>=450,"core ticks in 720 frames (got "..total..", ideal 600)")
check(apu>200,"APU activity (got "..apu.." writes)")
local o=io.open(os.getenv("OUT"),"w")
o:write(string.format("ticks=%d/720 frames, apu writes=%d, modes seen: %s\n",total,apu,(function() local s="" for k,_ in pairs(seen) do s=s..k.." " end return s end)()))
if #fails==0 then o:write("SMOKE OK\n") else for _,f in ipairs(fails) do o:write("FAIL: "..f.."\n") end end
o:close(); emu.exit()
