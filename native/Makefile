# CHOPPING MALL - C port. Plain Makefile; vendored raylib 5.5 and PlutoVG v1.3.3.
#   make          build the game  (build/chopping-mall)
#   make test     host-side tests: logic traces, c2d, audio engine (no window needed)
#   make asan     the same tests with AddressSanitizer + UBSan
#   make lint     extra warnings (-Wconversion) on the game-logic files
#   make devtools extra comparison utilities (frame_dump, audio_render, bg_dump, comp_test)
#   make clean
CC      ?= cc
UNAME_S := $(shell uname -s 2>/dev/null)
WARN    := -Wall -Wextra -Wpedantic -Wshadow
CFLAGS  ?= -O2
CFLAGS  += -std=c11 $(WARN) -fno-fast-math
INC     := -Isrc -Isrc/c2d -Ithird_party/plutovg/include -Ithird_party/raylib/src
PLUTO_C := $(wildcard third_party/plutovg/source/*.c)
PLUTO_O := $(patsubst third_party/plutovg/source/%.c,build/plutovg/%.o,$(PLUTO_C))
PLUTO_F := -DPLUTOVG_BUILD_STATIC -Ithird_party/plutovg/source -Ithird_party/plutovg/include -w

LOGIC   := src/game.c src/weapons.c src/robot.c src/world.c src/storage.c
GFX     := src/render.c src/render_bg.c src/hud.c src/c2d/c2d.c src/c2d/glow.c src/c2d/layers.c src/c2d/font_data.c
AUDIO   := src/sound/sound.c src/sound/music.c src/sound/music_data.c src/sound/jingles.c src/wa/wa.c
RAYLIB  := third_party/raylib/src/libraylib.a

ifeq ($(UNAME_S),Linux)
  PLATLIBS := -lGL -lm -lpthread -ldl -lrt -lX11
else ifeq ($(UNAME_S),Darwin)
  PLATLIBS := -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -lm
else
  PLATLIBS := -lopengl32 -lgdi32 -lwinmm -lm
endif

all: build/chopping-mall

build/plutovg/%.o: third_party/plutovg/source/%.c
	@mkdir -p build/plutovg
	$(CC) -std=c11 -O2 $(PLUTO_F) -c $< -o $@
build/libplutovg.a: $(PLUTO_O)
	$(AR) rcs $@ $^

$(RAYLIB):
	$(MAKE) -C third_party/raylib/src PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC

build/chopping-mall: src/main.c $(LOGIC) $(GFX) $(AUDIO) build/libplutovg.a $(RAYLIB) $(wildcard src/*.h src/*/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) $(INC) -DPLUTOVG_BUILD_STATIC src/main.c $(LOGIC) $(GFX) $(AUDIO) build/libplutovg.a $(RAYLIB) $(PLATLIBS) -o $@

# ---------------------------------------------------------------- tests (host side)
TESTCF := $(CFLAGS) $(INC) -DPLUTOVG_BUILD_STATIC
TESTS  := build/trace_replay build/test_c2d build/test_wa build/frame_dump build/audio_render

build/trace_replay: tests/trace_replay.c $(LOGIC) src/sound/sound_null.c
	@mkdir -p build
	$(CC) $(TESTCF) $^ -lm -o $@
build/test_c2d: tests/test_c2d.c $(GFX) src/world.c build/libplutovg.a
	@mkdir -p build
	$(CC) $(TESTCF) $^ -lm -o $@
build/test_wa: tests/test_wa.c src/wa/wa.c
	@mkdir -p build
	$(CC) $(TESTCF) $^ -lm -o $@
build/frame_dump: tests/frame_dump.c $(LOGIC) $(GFX) src/sound/sound_null.c build/libplutovg.a
	@mkdir -p build
	$(CC) $(TESTCF) $^ -lm -o $@
build/audio_render: tests/audio_render.c $(AUDIO)
	@mkdir -p build
	$(CC) $(TESTCF) $^ -lm -o $@

build/bg_dump: tests/bg_dump.c src/render_bg.c src/world.c src/c2d/c2d.c src/c2d/glow.c src/c2d/layers.c src/c2d/font_data.c build/libplutovg.a
	@mkdir -p build
	$(CC) $(TESTCF) $^ -lm -o $@
build/comp_test: tests/comp_test.c src/wa/wa.c
	@mkdir -p build
	$(CC) $(TESTCF) $^ -lm -o $@

# utilities used by tools/golden/ to compare against the original HTML (need node, see README)
devtools: $(TESTS) build/bg_dump build/comp_test

test: $(TESTS)
	@echo "== game logic: replay recorded traces (real JS vs C)"
	@fail=0; for t in tools/golden/traces/*.inputs; do b=$${t%.inputs}; seed=$$(grep -o 'seed=[0-9]*' $$b.trace | head -1 | cut -d= -f2); \
	  CHOPPING_MALL_BEST=/dev/null build/trace_replay $$t $$seed > /tmp/cm_replay.trace; printf '%-26s ' $$(basename $$b); \
	  python3 tools/golden/compare_trace.py $$b.trace /tmp/cm_replay.trace || fail=1; done; exit $$fail
	@echo "== c2d drawing layer"; build/test_c2d
	@echo "== audio engine"; build/test_wa

asan: CFLAGS := -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
asan:
	$(MAKE) clean-tests CFLAGS="$(CFLAGS)" && $(MAKE) test CFLAGS="$(CFLAGS)"

lint:
	$(CC) -std=c11 -fsyntax-only -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Isrc $(LOGIC) src/sound/sound_null.c

clean-tests:
	rm -f $(TESTS)
clean:
	rm -rf build
	-$(MAKE) -C third_party/raylib/src clean

.PHONY: all test asan lint clean clean-tests devtools
