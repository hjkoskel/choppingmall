-- lua_read.lua: run until par_done (address in env PAR_DONE etc. passed via file build/addrs.lua), dump acc/ticks
dofile(os.getenv("ADDRS"))
emu.speedmode("turbo")
for i=1,5 do emu.frameadvance() end
local frames=0
while memory.readbyte(A_done)==0 and frames<60000 do emu.frameadvance(); frames=frames+1 end
local acc=memory.readbyte(A_acc)+256*memory.readbyte(A_acc+1)
local tk=memory.readbyte(A_ticks)+256*memory.readbyte(A_ticks+1)
local f=io.open(os.getenv("OUT"),"w"); f:write(string.format("%d %d frames=%d\n",acc,tk,frames)); f:close()
emu.exit()
