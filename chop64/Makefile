# CHOPPING MALL for the Commodore 64 (cc65).
#   make          chopping-mall.prg  (cc65 only)           make d64   .d64 disk image with loading screen (needs c1541 from VICE)
#   make test     host tests of the portable core (gcc)     make parity  host-vs-6502 state parity (needs VICE + C64 ROMs)
#   make assets   regenerate src/assets_gen.h + src/*_gen.h (host gcc + python3; the generated files are checked in)
CL65   ?= cl65
CC     ?= gcc
OPT    ?= -Oirs
# -Cl: static locals (much faster code on a 6502); the optimiser setting is verified by tools/parity.sh
C65FLAGS = -t c64 $(OPT) -Cl -Isrc -Wl -D,__STACKSIZE__=0x300  # 768 byte C stack ($CD00-$CFFF); the colour matrix lives under the I/O area at $DC00
SRC65  = src/plat_c64.c src/core.c src/mcfont.c

all: build/chopping-mall.prg

build/chopping-mall.prg: $(SRC65) $(wildcard src/*.h)
	@mkdir -p build
	$(CL65) $(C65FLAGS) -m build/chopping-mall.map -o $@ $(SRC65)
	@echo "memory map (the program must end below \$$CD00; the VIC uses \$$E000-\$$FFFF):"; grep -E "^(CODE|RODATA|DATA|BSS) " build/chopping-mall.map
	@python3 -c "import re,sys; m=open('build/chopping-mall.map').read(); e=max(int(x,16) for x in re.findall(r'^(?:CODE|RODATA|DATA|BSS) +[0-9A-F]+ +([0-9A-F]+)',m,re.M)); print('program ends at \$$%04X, limit \$$CCFF'%e); sys.exit(e>0xCCFF)"

# disk image: "chopping mall" = the loader (shows loading/loading.koa), "game" = the game it loads
d64: build/chopping-mall.prg build/loader.prg
	c1541 -format "chopping mall,cm" d64 build/chopping-mall.d64 -write build/loader.prg "chopping mall" -write build/chopping-mall.prg "game" >/dev/null

# loading screen: loading/loading.koa (Koala format) -> src/loading_gen.h -> build/loader.prg  (docs/LOADING_SCREEN.md)
src/loading_gen.h: loading/loading.koa tools/koa2h.py
	python3 tools/koa2h.py loading/loading.koa $@
build/loader.prg: src/loader.c src/loading_gen.h
	@mkdir -p build
	$(CL65) -t c64 -Oirs -Isrc -o $@ src/loader.c
loading/loading.koa:
	python3 tools/mkloading.py $@
loader: build/loader.prg

# autoplay demo build: the bot plays (for screenshots and soak tests)
build/demo.prg: $(SRC65) src/bot.c $(wildcard src/*.h)
	@mkdir -p build
	$(CL65) $(C65FLAGS) -DAUTOPLAY -DAUTOPLAY_TICKS=0 -o $@ $(SRC65) src/bot.c

# ---- host side
build/test_core: tests/test_core.c src/core.c src/assets.c src/picture.c src/mcfont.c src/bot.c src/plat_host.c $(wildcard src/*.h)
	@mkdir -p build
	$(CC) -std=c99 -O2 -Wall -Wextra -Wpedantic -Isrc $(filter %.c,$^) -o $@
test: build/test_core
	build/test_core

assets:
	@mkdir -p build
	python3 tools/gen_tables.py tables > src/tables_gen.h
	python3 tools/gen_tables.py music > src/music_gen.h
	python3 tools/gen_jingles.py > src/jingle_gen.h
	$(CC) -std=c99 -O2 -Isrc tools/gen_assets.c src/assets.c src/picture.c src/mcfont.c src/core.c src/plat_host.c -o build/gen_assets
	build/gen_assets loading/win.koa > src/assets_gen.h

parity:
	tools/parity.sh 600 && tools/parity.sh 3000

clean:
	rm -rf build src/*.o
.PHONY: all d64 test assets parity clean
