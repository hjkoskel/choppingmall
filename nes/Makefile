# NES port of CHOPPING MALL (cc65: ca65/cc65/ld65; host gcc + python3 for the generators; fceux + xvfb for the tests)
#   make            build/chopping-mall.nes (NROM-256: 32 KB PRG + 8 KB CHR)
#   make test       host-vs-6502 parity (bot plays, state hash compared) + emulator smoke test
#   make tables     regenerate src/nes_tables.h (direction tables, obstacle grid)         [generated files are checked in]
#   make bg         redo the background tile conversion, ~2 minutes (gfx/*.bin)           [checked in as well]
#   make shots      screenshots of title / play / overlays into build/shots
C64SRC := ../chop64/src
all: build/chopping-mall.nes
build/chr.bin build/nt.bin build/attr.bin: tools/mknes_assets.py gfx/bg_tiles.bin gfx/bg_nt.bin gfx/bg_attr.bin
	python3 tools/mknes_assets.py
# always rebuilt: the experiment scripts (prof.sh, worst.sh, ...) write other flag variants to the same file
build/plat_nes.nes: FORCE build/chr.bin build/nt.bin build/attr.bin
	sh build.sh plat_nes
build/chopping-mall.nes: build/plat_nes.nes
	cp build/plat_nes.nes $@; cp build/plat_nes.lbl build/chopping-mall.lbl
	@ls -l $@
tables: build/gen_nes_tables
	build/gen_nes_tables > src/nes_tables.h
build/gen_nes_tables: tools/gen_nes_tables.c $(C64SRC)/core.c $(C64SRC)/assets.c $(C64SRC)/picture.c
	mkdir -p build; gcc -std=c99 -O2 -I$(C64SRC) tools/gen_nes_tables.c $(C64SRC)/assets.c $(C64SRC)/picture.c $(C64SRC)/mcfont.c $(C64SRC)/core.c $(C64SRC)/plat_host.c -o $@
gfx/pic_notext.png: $(C64SRC)/picture.c
	mkdir -p build; gcc -std=c99 -O2 -DNO_TEXT -I$(C64SRC) ../chop64/tools/gen_assets.c $(C64SRC)/assets.c $(C64SRC)/picture.c $(C64SRC)/mcfont.c $(C64SRC)/core.c $(C64SRC)/plat_host.c -o build/gen_notext
	build/gen_notext --ppm > build/pic_nt.ppm; python3 -c "from PIL import Image; Image.open('build/pic_nt.ppm').convert('RGB').save('gfx/pic_notext.png')"
bg: gfx/pic_notext.png
	python3 tools/mknes_bg.py gfx/pic_notext.png 199 build/bg
	cp build/bg_tiles.bin build/bg_nt.bin build/bg_attr.bin build/bg_pal.bin build/bg_preview.png gfx/
test: build/chopping-mall.nes
	tools/parity.sh 6000
	tools/smoke.sh build/chopping-mall.nes
shots: build/chopping-mall.nes
	mkdir -p build/shots; GADDR=$$(grep " \._G$$" build/chopping-mall.lbl | awk '{print $$2}' | sed 's/^0*//') OUTDIR=$$PWD/build/shots xvfb-run -a /usr/games/fceux --loadlua tools/shots.lua build/chopping-mall.nes >/dev/null 2>&1; \
	GADDR=$$(grep " \._G$$" build/chopping-mall.lbl | awk '{print $$2}' | sed 's/^0*//') OUTDIR=$$PWD/build/shots xvfb-run -a /usr/games/fceux --loadlua tools/modes.lua build/chopping-mall.nes >/dev/null 2>&1; ls build/shots
clean:
	rm -rf build
FORCE:
.PHONY: FORCE all tables bg test shots clean
