/* loader.c - shows the loading picture, then loads and starts the game from the same disk.
 * The picture (loading/loading.koa, see docs/LOADING_SCREEN.md) is copied into the very same VIC bank 3 layout the game uses
 * ($E000 bitmap, $DC00 colours, colour RAM $D800), which the game's own load ($0801-$BE73) never touches, so the picture stays up
 * while the 1541 loads. The KERNAL LOAD call runs from a 24-byte routine in the cassette buffer because it overwrites this program. */
#include <string.h>
#include <peekpoke.h>
#include "loading_gen.h"

#define BMP    ((unsigned char *)0xE000)
#define MATRIX ((unsigned char *)0xDC00) /* RAM under the I/O area: written with I/O banked out, interrupts off */
#define COLRAM ((unsigned char *)0xD800)
#define GAME_NAME_LEN 4
/* file name on the disk as PETSCII bytes (cc65 string literals are converted, so spell it out): "GAME" */
static const unsigned char GAME_NAME[GAME_NAME_LEN] = { 0x47, 0x41, 0x4D, 0x45 };

/* SETNAM(len, name) / SETLFS(1, 8, 1) / LOAD(0) then jump to the game's BASIC "SYS 2061" entry ($080D) */
static const unsigned char tramp[] = {
  0xA9, GAME_NAME_LEN,       /* lda #len */
  0xA2, 0x00, 0xA0, 0x03,    /* ldx #<name, ldy #>name  (patched below: name follows at $0334 + 28) */
  0x20, 0xBD, 0xFF,          /* jsr SETNAM */
  0xA9, 0x01, 0xA2, 0x08, 0xA0, 0x01, /* lda #1, ldx #8, ldy #1 (load to the address stored in the file) */
  0x20, 0xBA, 0xFF,          /* jsr SETLFS */
  0xA9, 0x00,                /* lda #0 (load, not verify) */
  0x20, 0xD5, 0xFF,          /* jsr LOAD */
  0x4C, 0x0D, 0x08           /* jmp $080D */
};

void main(void) {
  unsigned char *t = (unsigned char *)0x0334;
  POKE(204, 1);                       /* no blinking cursor */
  POKE(0xD020, 0); POKE(0xD021, PIC_BG);
  POKE(0xD011, 0x0B);                 /* screen off while the picture is copied */
  POKE(0xDD00, (unsigned char)(PEEK(0xDD00) & 0xFC)); /* VIC bank 3 */
  memcpy(BMP, PIC_BMP, 8000); memcpy(COLRAM, PIC_COL, 1000);
  __asm__("sei"); POKE(1, 0x34); memcpy(MATRIX, PIC_SCR, 1000); POKE(1, 0x36); __asm__("cli"); /* the KERNAL IRQ must not run with the ROMs off */
  POKE(0xD018, 0x78); POKE(0xD016, 0xD8); POKE(0xD011, 0x3B); /* multicolour bitmap on */
  memcpy(t, tramp, sizeof tramp);
  t[3] = (unsigned char)((0x0334 + sizeof tramp) & 255); t[5] = (unsigned char)((0x0334 + sizeof tramp) >> 8);
  memcpy(t + sizeof tramp, GAME_NAME, GAME_NAME_LEN);
  ((void (*)(void))0x0334)();
}
