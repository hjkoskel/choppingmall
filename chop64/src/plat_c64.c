/* plat_c64.c - Commodore 64 platform layer (cc65): VIC-II graphics, CIA input, SID sound, main loop.
 *
 * Memory map: the program lives in $0801-$CCFF (BASIC ROM banked out). The VIC uses bank 3 ($C000-$FFFF),
 * placing its data in the RAM underneath the KERNAL ROM: the VIC always sees RAM, and CPU writes
 * land in RAM even while the ROM is mapped (the program never reads these areas back):
 *   $E000 character set   $E800 screen matrix   $EBF8 sprite pointers   $F000 sprite shapes
 * Graphics: multicolour character mode. Floor/carpet/text are hires cells (colour RAM 0..7);
 * obstacles are multicolour cells (colour RAM 8..15). The hexagonal mall is rendered once into the
 * character set from the same geometry the collision code uses; the 8 hardware sprites are
 * player, robot, 3 projectiles, robot bolt and 2 fires. Runs at 50 Hz (PAL), one core tick per frame. */
#include <string.h>
#include "assets.h"
#include "assets_gen.h"
#include "bot.h"
#include "core.h"
#include "mcfont.h"
#include "music_gen.h"
#include "jingle_gen.h"
#include "plat.h"

#define POKE(a, v) (*(volatile u8 *)(a) = (v))
#define PEEK(a) (*(volatile u8 *)(a))
#define BMP ((u8 *)0xE000)     /* the multicolour bitmap, cell after cell (8 bytes per cell) */
#define MATRIX ((u8 *)0xDC00)  /* colour matrix: pixel codes 01 (hi nibble) and 10 (lo nibble); in the RAM under the I/O area, written with I/O banked out */
/* robot sprite colours (C64 palette: 11 dark grey, 12 mid grey, 15 light grey) */
/* player: multicolour sprite - body = sprite colour, head = shared colour 1, gun barrel = shared colour 2 */
#define PLAYER_BODY 14
#define MC_SHARED1 10 /* shared multicolour 1: the player's head AND the robot's eye (both sprites use code 01) */
#define PLAYER_HEAD MC_SHARED1
#define PLAYER_GUN 0
#define ROBOT_COLOR 11
#define ROBOT_WIN_COLOR 15
#define SPRPTR ((u8 *)0xDFF8)  /* also under the I/O area */
#define SPRMEM ((u8 *)0xD000)  /* in the RAM under the I/O area: written while I/O is banked out */
#define SPR_BASE 64            /* sprite pointer value of $D000 inside VIC bank 3: ($D000 - $C000) / 64 */
#define COLRAM ((u8 *)0xD800)  /* colour RAM: pixel code 11 */
#define SID(r) (*(volatile u8 *)(0xD400 + (r)))

/* shop door markers: the '$' cells are part of the picture; only their colour changes (ready / cooling down) */
static const u8 DOOR_COLOR[4] = {7, 10, 13, 14}; /* ammo yellow, shells light red, gas light green, first aid light blue */
static u8 door_ready[6];
static u16 door_idx[6]; /* colour RAM offsets, computed once (a multiply per door per frame is wasteful on a 6502) */
static void draw_doors(u8 force) {
  u8 k;
  for (k = 0; k < 6; k++) {
    u8 ready = G.cool[k] == 0 && shop_open(shop_kind[k]); /* doors of weapons the level lacks stay dark */
    if (!force && ready == door_ready[k]) continue;
    door_ready[k] = ready;
    COLRAM[door_idx[k]] = ready ? DOOR_COLOR[shop_kind[k]] : 11;
  }
}

/* ================================================================== text (glyph cells drawn into the bitmap) */
static void cell_glyph(u8 x, u8 y, u8 gi, u8 color) { /* a 3x5 glyph: 8 bytes into the bitmap, its colour into colour RAM */
  u16 i = (u16)y * 40 + x;
  memcpy(BMP + i * 8, ASSET_MCFONT + (u16)gi * 8, 8);
  COLRAM[i] = color;
}
static void put_str(u8 x, u8 y, const char *s, u8 color) {
  while (*s) cell_glyph(x++, y, glyph_index(*s++), color);
}
static void put_num(u8 x, u8 y, u32 v, u8 width, u8 color) { /* right aligned, blank padded */
  char buf[8];
  u8 i = width;
  u16 w;
  buf[width] = 0;
  while (v > 65535UL && i) { buf[--i] = (char)('0' + (u8)(v % 10)); v /= 10; } /* rare: 32-bit digits are slow on a 6502 */
  w = (u16)v;
  if (i) do { buf[--i] = (char)('0' + (u8)(w % 10)); w /= 10; } while (w && i);
  while (i) buf[--i] = ' ';
  put_str(x, y, buf, color);
}
/* ---- packed pictures: the mall background and the victory picture are stored RLE-packed (see tools/gen_assets.c) and unpacked straight into the bitmap */
static const u8 *unpack(u8 *dst, const u8 *src, u16 n) { /* returns the source position after the stream */
  while (n) {
    u8 c = *src++;
    u16 len;
    if (c & 0x80) { len = (u16)((c & 0x7F) + 3); memset(dst, *src++, len); }
    else { len = (u16)c + 1; memcpy(dst, src, len); src += len; }
    dst += len; n -= len;
  }
  return src;
}
static void show_picture(const u8 *z, u8 bg) { /* bitmap + colour matrix + colour RAM; the display is blanked while it is written */
  POKE(0xD011, 0x2B);
  z = unpack(BMP, z, 8000);
  POKE(1, 0x34); z = unpack(MATRIX, z, 1000); POKE(1, 0x36); /* I/O off: $DC00 is RAM for the VIC */
  unpack(COLRAM, z, 1000);
  POKE(0xD021, bg);
  POKE(0xD011, 0x3B);
}
static u32 h_score = 0xFFFFFFFFUL;
static u8 h_wl = 255, h_level = 255, h_lives = 255, h_kills = 255, h_bar = 255, h_barcol = 255, h_w = 255, h_a0 = 255, h_a1 = 255, h_a2 = 255, h_tt = 255;
static u16 h_sec = 0xFFFF;
static const char *h_toast;
static void restore_bg(void) { /* erase an overlay or the victory picture: unpack the mall again, then the HUD redraws itself */
  show_picture(ASSET_BG, 0);
  h_score = 0xFFFFFFFFUL; h_sec = 0xFFFF; h_toast = 0;
  h_wl = h_level = h_lives = h_kills = h_bar = h_barcol = h_w = h_a0 = h_a1 = h_a2 = h_tt = 255;
  draw_doors(1);
}
static void box(u8 x0, u8 y0, u8 w, u8 h) { /* a black panel */
  u8 y;
  for (y = y0; y < y0 + h; y++) memset(BMP + ((u16)y * 40 + x0) * 8, 0, (u16)w * 8);
}
static void center(u8 y, const char *s, u8 color) { put_str((u8)(20 - strlen(s) / 2), y, s, color); }

static u8 shown_mode = 255;
static void overlay(void) { /* title screen and banners over the arena */
  char b[12];
  if (shown_mode != 255 && shown_mode != M_PLAY && G.mode != M_WIN) restore_bg(); /* an overlay or the victory picture is on screen */
  shown_mode = G.mode;
  switch (G.mode) {
  case M_TITLE:
    box(7, 4, 26, 17);
    center(5, "CHOPPING MALL", 7);
    center(7, "C64 EDITION", 3);
    center(9, "JOY 2 OR WASD: MOVE", 1);
    center(10, "FIRE OR SPACE: AIM", 1);
    center(11, "1 2 3: WEAPONS", 1);
    center(12, "M MUTE  HOLD P: RESTART", 1);
    center(14, "NO AMMO AT START:", 13);
    center(15, "VISIT THE $ DOORS", 13);
    center(18, "PRESS FIRE OR RETURN", 7);
    break;
  case M_INTRO:
    box(7, 10, 26, 5);
    strcpy(b, "LEVEL "); b[6] = (char)('0' + G.level); b[7] = 0;
    center(11, b, 7);
    center(13, G.level == 1 ? "PISTOL ONLY - FIND AMMO" : G.level == 2 ? "SHOTGUN SHELLS UNLOCKED" : "MOLOTOV GAS UNLOCKED", 1);
    break;
  case M_LOST: box(7, 10, 26, 5); center(11, "LIFE LOST", 10); strcpy(b, "x LEFT"); b[0] = (char)('0' + G.lives); center(13, b, 1); break;
  case M_CLEAR: box(7, 10, 26, 5); center(11, "ROBOT DESTROYED", 13); center(13, "BONUS FOR SPEED", 1); break;
  case M_OVER: box(7, 9, 26, 7); center(10, "GAME OVER", 10); center(12, "SCORE", 1); center(14, "PRESS FIRE", 7); break;
  case M_WIN: /* the victory picture (loading/win.koa) with a small text panel at the bottom */
    show_picture(ASSET_WIN, WIN_BG);
    box(7, 21, 26, 4); center(22, "MALL SECURED", 13); put_str(14, 23, "SCORE", 1); center(24, "PRESS FIRE", 7);
    break;
  default: break;
  }
}
static u32 shown_final = 0xFFFFFFFFUL;

/* ================================================================== HUD (side panels; the labels are in the picture) */
/* The HUD is updated in four quarters, one per tick phase, so no single frame carries all of it. */
static void hud(u8 part) {
  u8 i, bar, col;
  char tb[8];
  if (part == 0) {
    if (G.score != h_score) { h_score = G.score; put_num(0, 2, G.score, 6, 7); }
    if (G.level != h_level) { h_level = G.level; cell_glyph(0, 5, G.level, 1); cell_glyph(1, 5, GL_SLASH, 1); cell_glyph(2, 5, 3, 1); }
  } else if (part == 1) {
    if (G.lives != h_lives) {
      h_lives = G.lives;
      for (i = 0; i < 3; i++) cell_glyph(i, 8, GL_HEART, i < G.lives ? 10 : 11);
    }
    /* health bar: 8 cells x 4 pixels = 32 steps (about 3 % each), partial cells drawn with the multicolour pixel value %11 (the cell's colour RAM colour) */
    bar = (u8)(G.php > 0 ? (u8)(((u16)G.php * 32u + 999u) / 1000u) : 0);
    col = G.php > 600 ? 5 : G.php > 300 ? 7 : 2;
    if (bar != h_bar || col != h_barcol) {
      static const u8 BARMASK[5] = {0x00, 0xC0, 0xF0, 0xFC, 0xFF};
      h_bar = bar; h_barcol = col;
      for (i = 0; i < 8; i++) {
        u8 n = bar > (u8)(i * 4) ? (u8)(bar - i * 4) : 0, *p = BMP + ((u16)11 * 40 + i) * 8;
        if (n > 4) n = 4;
        p[0] = 0; p[1] = p[2] = p[3] = p[4] = p[5] = BARMASK[n]; p[6] = 0; p[7] = 0xFF; /* bar rows + a track line under it */
        COLRAM[(u16)11 * 40 + i] = col;
      }
    }
  } else if (part == 2) {
    if (G.tsec != h_sec) {
      h_sec = G.tsec;
      tb[0] = (char)('0' + (G.tsec / 600) % 10); tb[1] = (char)('0' + (G.tsec / 60) % 10); tb[2] = ':';
      tb[3] = (char)('0' + (G.tsec % 60) / 10); tb[4] = (char)('0' + G.tsec % 10); tb[5] = 0;
      put_str(35, 2, tb, 1);
    }
    if (G.kills != h_kills) { h_kills = G.kills; put_num(35, 5, G.kills, 1, 1); }
  } else {
    if (G.level != h_wl || G.w != h_w || G.ammo[0] != h_a0 || G.ammo[1] != h_a1 || G.ammo[2] != h_a2) {
      static const char name[3] = {'P', 'S', 'M'};
      h_wl = G.level; h_w = G.w; h_a0 = G.ammo[0]; h_a1 = G.ammo[1]; h_a2 = G.ammo[2];
      for (i = 0; i < 3; i++) {
        u8 sel = G.w == i, col = sel ? 7 : 1;
        if (!W_AVAIL(i)) { put_str(35, (u8)(9 + i), "     ", 1); continue; } /* not offered on this level */
        cell_glyph(35, (u8)(9 + i), (u8)(i + 1), col);
        cell_glyph(36, (u8)(9 + i), glyph_index(name[i]), col);
        put_num(37, (u8)(9 + i), G.ammo[i], 3, G.ammo[i] ? col : 10);
      }
    }
    if (G.toast != h_toast || (G.tt == 0) != (h_tt == 0)) {
      h_toast = G.toast; h_tt = G.tt;
      put_str(35, 14, "     ", 1);
      if (G.tt && G.toast) put_str(35, 14, G.toast, 3);
    }
  }
}

/* ================================================================== sprites */
static void sprite_set(u8 n, i16 px, i16 py, u8 shape, u8 color, u8 on, u8 *enable, u8 *msb) {
  i16 sx = px + 12, sy = py + 40; /* sprite coordinates: visible area starts at x = 24, y = 50 */
  if (!on) return;
  POKE(0xD000 + n * 2, (u8)sx);
  POKE(0xD001 + n * 2, (u8)sy);
  if (sx > 255) *msb |= (u8)(1 << n);
  POKE(1, 0x34); SPRPTR[n] = (u8)(SPR_BASE + shape); POKE(1, 0x36);
  POKE(0xD027 + n, color);
  *enable |= (u8)(1 << n);
}
static void sprites(void) {
  u8 en = 0, msb = 0, i, show = G.mode != M_TITLE && G.mode != M_OVER && G.mode != M_WIN;
  if (show) {
    if (!(G.pinv && G.mode == M_PLAY && (G.tick & 2))) sprite_set(0, G.px >> FP, G.py >> FP, SH_PLAYER + (G.pface >> 1), PLAYER_BODY, 1, &en, &msb);
    if (!G.rdead) sprite_set(1, G.rx >> FP, G.ry >> FP, SH_ROBOT + (G.rface >> 1), G.rwind ? ROBOT_WIN_COLOR : ROBOT_COLOR, 1, &en, &msb);
    for (i = 0; i < 3; i++)
      if (G.pr[i].kind) sprite_set((u8)(2 + i), G.pr[i].x >> FP, G.pr[i].y >> FP, G.pr[i].kind == PK_MOLOTOV ? SH_MOLOTOV : SH_BULLET, G.pr[i].kind == PK_MOLOTOV ? 5 : 1, 1, &en, &msb);
    if (G.bolt.life) sprite_set(5, G.bolt.x >> FP, G.bolt.y >> FP, SH_BOLT, 7, 1, &en, &msb);
    for (i = 0; i < 2; i++)
      if (G.fire[i].life) sprite_set((u8)(6 + i), G.fire[i].x >> FP, G.fire[i].y >> FP, (G.tick >> 2) & 1 ? SH_FIRE1 : SH_FIRE0, (u8)((G.tick >> 1) & 1 ? 8 : 7), 1, &en, &msb);
  }
  POKE(0xD010, msb);
  POKE(0xD015, en);
}

/* ================================================================== sound (SID) */
static u8 mute, mus_on;
static u8 mus_pos, mus_f;
static i16 sfx_f, sfx_step;
static u8 sfx_n, sfx_t, sfx_wave, sfx_prio;
static u8 jingle_on; /* the win/lose stingers are 3-voice scores from jingle_gen.h (see tools/gen_jingles.py) */
static const u8 *jp[3];
static u8 jleft[3], jwave[3];
static void sid_voices(void) { /* the music / effects instruments (the stingers change them) */
  SID(2) = 0x00; SID(3) = 0x08; SID(5) = 0x08; SID(6) = 0x60;   /* voice 1: pulse lead, pluck */
  SID(9) = 0x00; SID(10) = 0x04; SID(12) = 0x09; SID(13) = 0x80; /* voice 2: bass */
  SID(16) = 0x00; SID(17) = 0x08; SID(19) = 0x00; SID(20) = 0xC4; /* voice 3: effects */
}
static void sid_init(void) {
  u8 r;
  for (r = 0; r < 25; r++) SID(r) = 0;
  SID(24) = 15;
  sid_voices();
}
void snd_toggle_mute_c64(void) { mute ^= 1; SID(24) = mute ? 0 : 15; }
static void sfx_start(i16 f1, u8 frames, u8 wave, i16 f2, u8 prio) {
  if (jingle_on) return; /* the stinger uses all three voices */
  if (sfx_n && prio < sfx_prio) return;
  sfx_prio = prio;
  sfx_f = f1; sfx_n = frames ? frames : 1; sfx_t = 0; sfx_wave = wave;
  sfx_step = (i16)((f2 - f1) / sfx_n);
  SID(18) = 0;
  SID(14) = (u8)sfx_f; SID(15) = (u8)(sfx_f >> 8);
  SID(18) = (u8)(wave | 1);
}
void snd_sfx(i16 f1, u8 frames, u8 wave, i16 f2) {
  static const u8 W[4] = {0x10, 0x40, 0x20, 0x10};
  sfx_start((i16)(f1 * 17), frames, W[wave], (i16)(f2 * 17), 2);
}
void snd_clank(void) { sfx_start(21250, 6, 0x40, 26000, 1); }
static void note_on(u8 v, u8 midi, u8 wave) {
  u8 base = (u8)(v * 7);
  u16 f = SID_FREQ[midi];
  SID(base + 4) = wave; /* gate off then on retriggers the envelope */
  SID(base) = (u8)f; SID(base + 1) = (u8)(f >> 8);
  SID(base + 4) = (u8)(wave | 1);
}
void snd_jingle(u8 kind) {
  const u8 *const *sc = kind == JINGLE_WIN ? JW_S : JL_S;
  u8 v;
  sfx_n = 0; sfx_prio = 0;
  for (v = 0; v < 3; v++) { jp[v] = sc[v]; jleft[v] = 0; jwave[v] = 0x40; SID((u8)(v * 7 + 4)) = 0; }
  jingle_on = 1;
}
static void jingle_end(void) {
  u8 v;
  jingle_on = 0;
  for (v = 0; v < 3; v++) SID((u8)(v * 7 + 4)) = 0;
  sid_voices();
  SID(4) = 0x40; SID(11) = 0x20; mus_f = 0;
}
void snd_new_game(void) { mus_on = 1; if (jingle_on) jingle_end(); mus_pos = 0; mus_f = 0; }
static void jingle_frame(void) { /* one 50 Hz step of the three event streams: wait, note, instrument */
  u8 v, active = 0;
  for (v = 0; v < 3; v++) {
    u8 base = (u8)(v * 7);
    while (jp[v] && jleft[v] == 0) {
      u8 d = jp[v][0], n = jp[v][1], in = jp[v][2], i = (u8)(in & 0x7F);
      if (n == 255) { SID((u8)(base + 4)) = jwave[v]; jp[v] = 0; break; }
      jp[v] += 3;
      if (n == 0) SID((u8)(base + 4)) = jwave[v];                       /* gate off: the release phase rings out */
      else if (n != 254) {
        u16 f = SID_FREQ[n];
        SID(base) = (u8)f; SID((u8)(base + 1) ) = (u8)(f >> 8);
        if (!(in & 0x80)) {                                              /* new note: set the instrument and retrigger */
          jwave[v] = J_WAVE[i];
          SID((u8)(base + 4)) = jwave[v];
          SID((u8)(base + 2)) = 0; SID((u8)(base + 3)) = J_PW[i];
          SID((u8)(base + 5)) = J_AD[i]; SID((u8)(base + 6)) = J_SR[i];
          SID((u8)(base + 4)) = (u8)(jwave[v] | 1);
        }
      }
      jleft[v] = d;
    }
    if (jp[v]) { active = 1; jleft[v]--; }
  }
  if (!active) jingle_end();
}
static void snd_frame(void) {
  /* effects voice */
  if (sfx_n) {
    sfx_f += sfx_step;
    SID(14) = (u8)sfx_f; SID(15) = (u8)(sfx_f >> 8);
    if (++sfx_t >= sfx_n) { SID(18) = sfx_wave; sfx_n = 0; sfx_prio = 0; }
  }
  if (jingle_on) { jingle_frame(); return; }
  if (!mus_on) return;
  if (mus_f == 0) {
    u8 n = MUS_LEAD[mus_pos], b = MUS_BASS[mus_pos], d = MUS_DRUM[mus_pos];
    if (n) note_on(0, n, 0x40);
    if (b) note_on(1, b, 0x20);
    if (d & 1) sfx_start(2550, 4, 0x10, 680, 0);   /* kick */
    else if (d & 2) sfx_start(12000, 3, 0x80, 12000, 0); /* snare */
    else if (d & 4) sfx_start(30000, 1, 0x80, 30000, 0); /* hat */
  } else if (mus_f == 5) { SID(4) = 0x40; SID(11) = 0x20; }
  if (++mus_f >= 7) { mus_f = 0; if (++mus_pos >= 96) mus_pos = 0; }
}

/* ================================================================== input */
/* Keyboard matrix: write a column select to $DC00 (active low), read the rows from $DC01 (active low).
 * Each needed column is scanned once, and the bits are tested with constants (variable shifts are slow in cc65). */
static u8 scan(u8 colmask) {
  POKE(0xDC00, colmask);
  (void)PEEK(0xDC01); /* settle: the first read after switching columns can be stale on real hardware */
  return (u8)~PEEK(0xDC01);
}
static u8 read_input(u8 *ev) {
  static u8 prev, rhold;
  u8 in, k = 0, edge, c0, c1, c2, c4, c5, c7;
  POKE(0xDC00, 0xFF);
  in = (u8)(~PEEK(0xDC00) & 0x1F); /* joystick port 2: up down left right fire (IN_* use the same bits) */
  c0 = scan(0xFE); c1 = scan(0xFD); c2 = scan(0xFB); c4 = scan(0xEF); c5 = scan(0xDF); c7 = scan(0x7F);
  POKE(0xDC00, 0xFF);
  /* (column, row): W(1,1) S(1,5) A(1,2) D(2,2) SPACE(7,4) RETURN(0,1) P(5,1) M(4,4) 1(7,0) 2(7,3) 3(1,0) */
  if (c1 & 0x02) in |= IN_UP;
  if (c1 & 0x20) in |= IN_DOWN;
  if (c1 & 0x04) in |= IN_LEFT;
  if (c2 & 0x04) in |= IN_RIGHT;
  if (c7 & 0x10) { in |= IN_FIRE; k |= 1; }
  if (c0 & 0x02) k |= 1;
  if (c4 & 0x10) k |= 4;
  if (c7 & 0x01) k |= 8;
  if (c7 & 0x08) k |= 16;
  if (c1 & 0x01) k |= 32;
  if (in & IN_FIRE) k |= 1;
  /* RESTART (P) must be HELD for one second. P sits in keyboard column 5, where none of the game's other keys are and which the joystick port
   * cannot short, so it cannot ghost. (The old R key did: W + A + D held together read as R and restarted the game at level 1.) */
  if (c5 & 0x02) { if (rhold < 255) rhold++; } else rhold = 0;
  if (rhold == 50) k |= 2;
  edge = (u8)(k & ~prev);
  prev = k;
  *ev = 0;
  if (edge & 1) *ev |= EV_START;
  if (edge & 2) *ev |= EV_RESTART;
  if (edge & 4) snd_toggle_mute_c64();
  if (edge & 8) *ev |= EV_W1;
  if (edge & 16) *ev |= EV_W2;
  if (edge & 32) *ev |= EV_W3;
  return in;
}

/* ================================================================== main */
static void wait_frame(void) { /* PAL: raster line 251 marks the start of the bottom border */
  while (PEEK(0xD012) != 251) {}
  while (PEEK(0xD012) == 251) {}
}

#ifndef AUTOPLAY_TICKS
#define AUTOPLAY_TICKS 0
#endif

#ifdef DEBUG_PERF
/* cycle probe: CIA1 timer A counts down (free running); segments shorter than 65536 cycles are measured exactly */
static u16 cia_timer(void) { return (u16)(PEEK(0xDC04) | (PEEK(0xDC05) << 8)); }
static u16 p_loops, p_iter0;
static u16 p_max[4];
static u32 p_sum[4];
static u16 p_n[4];
static void p_rec(u8 k, u16 t0) {
  u16 d = (u16)(t0 - cia_timer());
  if (G.mode != M_PLAY) return;
  if (d > p_max[k]) p_max[k] = d;
  p_sum[k] += d; p_n[k]++;
}
static u16 q_max[8], q_n[8];
static u32 q_sum[8];
u16 prof_timer(void) { return cia_timer(); }
void prof_rec(u8 k, u16 t0) { u16 d = (u16)(t0 - cia_timer()); if (d > q_max[k]) q_max[k] = d; q_sum[k] += d; q_n[k]++; }
static void p_show(void) {
  u8 k;
  for (k = 0; k < 7; k++) { put_num(10, (u8)(4 + k), q_max[k], 6, 1); put_num(18, (u8)(4 + k), q_n[k] ? (u16)(q_sum[k] / q_n[k]) : 0, 6, 1); put_num(26, (u8)(4 + k), q_n[k], 6, 1); }
  put_num(10, 3, p_loops, 6, 1);
  put_num(10, 12, p_max[3], 6, 1); put_num(18, 12, p_n[3] ? (u16)(p_sum[3] / p_n[3]) : 0, 6, 1);
  for (k = 0; k < 3; k++) { put_num(10, k, p_max[k], 6, 1); put_num(18, k, p_n[k] ? (u16)(p_sum[k] / p_n[k]) : 0, 6, 1); put_num(26, k, p_n[k], 6, 1); }
}
#endif

int main(void) {
  u8 ev, in;
  u16 ticks = 0;
  __asm__("sei");
  POKE(0xDC0D, 0x7F); POKE(0xDD0D, 0x7F); (void)PEEK(0xDC0D); (void)PEEK(0xDD0D); POKE(0xD01A, 0); POKE(0xD019, 0xFF);
  POKE(0xD020, 0); POKE(0xD021, 0); /* black border and background: multicolour code 00 */
  POKE(0xD015, 0);
  POKE(0xD01C, 0x03); POKE(0xD025, MC_SHARED1); POKE(0xD026, PLAYER_GUN); /* sprites 0 (player) and 1 (robot) are multicolour */
  POKE(0xDD00, (u8)(PEEK(0xDD00) & 0xFC)); /* VIC bank 3: $C000-$FFFF */
  POKE(0xD018, 0x78);                      /* colour matrix at $DC00, bitmap at $E000 */
  POKE(0xD011, 0x3B);                      /* bitmap mode, display on, 25 rows */
  POKE(0xD016, 0xD8);                      /* multicolour on, 40 columns */
  POKE(1, 0x34);                           /* all RAM: the sprite shapes live under the I/O area */
  memcpy(SPRMEM, ASSET_SPR, sizeof ASSET_SPR);
  POKE(1, 0x36);
  show_picture(ASSET_BG, 0);               /* the static background, drawn at build time */
  game_init(1);
#ifdef DEBUG_SHOWWIN /* debug: start on the victory screen */
  G.mode = M_WIN; G.score = 12345; G.mt = 0;
#endif
#ifndef NO_WALKMAP
  core_set_walkmap(ASSET_WALK);
#endif
  for (in = 0; in < 6; in++) door_idx[in] = (u16)((doory[in] >> 3) * 40 + (doorx[in] >> 3));
  draw_doors(1);
  sid_init();
#ifdef DEBUG_JINGLE /* debug: play a stinger at start (0 = win, 1 = lose) */
  snd_jingle(DEBUG_JINGLE);
#endif
  POKE(1, 0x34); SPRPTR[0] = SPR_BASE; POKE(1, 0x36);
#ifdef DEBUG_PERF
  POKE(0xDC04, 0xFF); POKE(0xDC05, 0xFF); POKE(0xDC0E, 0x11); POKE(0xDC0E, 0x01); /* CIA1 timer A: free running cycle counter */
#endif
#ifdef DEBUG_HASH
  { u16 acc = 0; u8 frozen = 0;
    for (;;) {
      wait_frame();
      if (AUTOPLAY_TICKS && ticks >= AUTOPLAY_TICKS) frozen = 1;
      if (!frozen) { in = bot_input(&ev); game_tick(in, ev); ticks++; acc = (u16)(acc * 31u + game_hash()); put_num(10, 0, acc, 5, 1); put_num(20, 0, ticks, 5, 1);
        put_num(0, 3, (u16)G.rx, 5, 1); put_num(6, 3, (u16)G.ry, 5, 1); put_num(12, 3, G.rface, 3, 1); put_num(16, 3, (u16)G.rwx, 4, 1); put_num(21, 3, (u16)G.rwy, 4, 1); put_num(26, 3, G.rbase, 3, 1); put_num(30, 3, G.rstk, 3, 1); put_num(34, 3, G.rang, 3, 1);
      }
    }
  }
#endif
  for (;;) {
    wait_frame();
#ifdef DEBUG_PERF
    p_iter0 = cia_timer();
#endif
    in = read_input(&ev);
#ifdef AUTOPLAY
    if (!(AUTOPLAY_TICKS && ticks >= AUTOPLAY_TICKS)) {
#ifdef DEBUG_PERF
      u16 t0 = cia_timer();
#ifdef AUTOPLAY_IDLE
      in = 0; ev = (G.mode == M_TITLE || ((G.mode == M_OVER || G.mode == M_WIN) && G.mt == 0)) ? EV_START : 0; p_rec(0, t0);
#else
      in = bot_input(&ev); p_rec(0, t0);
#endif
      t0 = cia_timer();
      ticks++; game_tick(in, ev); p_rec(1, t0);
      t0 = cia_timer();
#else
#ifdef AUTOPLAY_IDLE
      in = 0; ev = (G.mode == M_TITLE || ((G.mode == M_OVER || G.mode == M_WIN) && G.mt == 0)) ? EV_START : 0;
#else
      in = bot_input(&ev);
#endif
      ticks++; game_tick(in, ev);
#endif
    }
#else
    (void)ticks;
    game_tick(in, ev);
#endif
#ifdef DEBUG_PERF
    { u16 t0 = cia_timer();
#endif
    snd_frame();
    if (G.mode != shown_mode) overlay();
    if ((G.mode == M_OVER || G.mode == M_WIN) && shown_final != G.score) { shown_final = G.score; if (G.mode == M_WIN) put_num(20, 23, G.score, 6, 7); else put_num(17, 12, G.score, 6, 7); }
    if (G.mode != M_WIN) { /* the victory picture covers the whole screen: no HUD or door colours on top */
      if ((G.tick & 7) == 0) draw_doors(0);
      if (G.mode == M_PLAY) hud((u8)(G.tick & 3)); /* one quarter of the side panels per tick */
      else { hud(0); hud(1); hud(2); hud(3); }
    }
    sprites();
#ifdef DEBUG_LOOPS
    { static u16 loops; if ((++loops & 63) == 0) put_num(10, 3, loops, 6, 1); }
#endif
#ifdef DEBUG_PERF
    p_rec(2, t0); }
    p_rec(3, p_iter0);
    p_loops++;
    if ((G.tick & 63) == 0) p_show(); /* the debug display itself is expensive: keep it off the measured path */
#endif
  }
  return 0;
}
