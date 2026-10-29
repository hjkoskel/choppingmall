#!/bin/sh
# Runs a PRG headless in VICE (x64sc under xvfb) for a number of emulated cycles and saves a screenshot.
# usage: vice_run.sh game.prg cycles out.png [extra x64sc options]
# Needs VICE with the C64 ROMs installed (kernal/basic/chargen). The 1541 drive ROMs are NOT needed: the PRG is injected into RAM.
prg=$1; cyc=$2; out=$3; shift 3
timeout 900 xvfb-run -a -s "-screen 0 1024x768x24" x64sc -warp +sound -autostartprgmode 1 "$@" -limitcycles "$cyc" -exitscreenshot "$out" -autostart "$prg" >/dev/null 2>&1 || true # VICE exits with status 1 when -limitcycles is reached
