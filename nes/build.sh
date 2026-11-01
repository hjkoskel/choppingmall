# usage: build.sh target [extra cc65 flags]   - builds build/<target>.nes from src/<target>.c (+ shared core files)
set -e
cd "$(dirname "$0")"; mkdir -p build
T=$1; shift
S=../chop64/src
ca65 -t none src/crt0.s -o build/crt0.o
objs=""
SRCS="src/$T.c $S/core.c"; [ -n "$BOT" ] && SRCS="$SRCS $S/bot.c"
for f in $SRCS; do
  b=$(basename $f .c)
  cc65 -t none --cpu 6502 -Oirs -Cl -DNES -DNES_NO_WALKMAP -Isrc -I$S "$@" $f -o build/$b.s
  ca65 -t none build/$b.s -o build/$b.o
  objs="$objs build/$b.o"
done
ld65 -C nes.cfg build/crt0.o $objs none.lib -m build/$T.map -Ln build/$T.lbl -o build/$T.nes
