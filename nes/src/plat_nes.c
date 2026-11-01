/* plat_nes.c - NES platform layer for the portable game core (NROM: 32 KB PRG, 8 KB CHR-ROM, 2 KB RAM).
 *  - NMI (crt0.s): OAM DMA + a small VRAM update list. The main loop builds one video frame (60 Hz); the core ticks 5 frames out of 6 (= 50 Hz).
 *  - Background: the mall picture, pre-converted to 8x8 tiles + 4 palettes by tools/mknes_bg.py (see gfx/). Labels, HUD and overlays are font tiles.
 *  - Sprites: player 2x2 tiles, robot 3x3 tiles, nine rotation frames each (0..90 degrees; the other quadrants use the H/V flip bits).
 *  - HUD: text rows at the top and bottom of the screen (the side panels of the C64 version do not fit a 256 px wide screen). */
#include <string.h>
#define NES_NO_WALKMAP 1
#include "types.h"
#include "core.h"
#include "plat.h"
#ifdef AUTOPLAY
#include "bot.h"
#endif
#include "music_gen.h"
#include "jingle_gen.h"
#include "nes_audio_gen.h"

#define POKE(a, v) (*(volatile u8 *)(a) = (v))
#define PEEK(a) (*(volatile u8 *)(a))
#define PPUSTATUS 0x2002
#define PPUADDR 0x2006
#define PPUDATA 0x2007

extern volatile u8 nmi_ready, nmi_count, ppumask, ppuctrl, vlen;
#pragma zpsym("nmi_ready")
#pragma zpsym("nmi_count")
#pragma zpsym("ppumask")
#pragma zpsym("ppuctrl")
#pragma zpsym("vlen")
extern u8 vbuf[56];
extern u8 oam[256];
extern const u8 nt_rom[800]; /* tile rows 2..26 of the nametable */
extern const u8 attr_rom[64];

#ifndef REGION_LIMIT
#define REGION_LIMIT 950   /* measured in fceux: NTSC 901, PAL 1006 loop passes per frame */
#endif
#define VBMAX 56
#define PAL_BG_U 0x38

/* ------------------------------------------------------------------ palettes: universal yellow, then 4 BG and 4 sprite palettes */
static const u8 PALETTE[32] = {
  0x38, 0x0F, 0x10, 0x30,   0x38, 0x16, 0x10, 0x25,   0x38, 0x2D, 0x00, 0x30,   0x38, 0x2A, 0x16, 0x30,  /* background (matches tools/mknes_bg.py) */
  0x38, 0x21, 0x36, 0x30,   0x38, 0x00, 0x0F, 0x16,   0x38, 0x27, 0x16, 0x38,   0x38, 0x10, 0x0F, 0x16,  /* sprites: player, robot, fire/bullets/doors, robot winding up */
};
static u8 pal;                               /* 1 on a PAL console (50 Hz video, 1.66 MHz CPU) */
static const u16 *per_p = PER_P, *per_t = PER_T; /* timer periods for the detected CPU clock */
/* ------------------------------------------------------------------ video helpers */
static u8 direct;      /* 1 while the screen is blanked: write the PPU straight away instead of queueing */
static u8 attr[64];
static void ppu_addr(u16 a) { (void)PEEK(PPUSTATUS); POKE(PPUADDR, (u8)(a >> 8)); POKE(PPUADDR, (u8)a); }
static void frame_end(void) { nmi_ready = 1; while (nmi_ready) {} }
static u8 tile_of(char c) {
  if (c >= '0' && c <= '9') return (u8)(c - '0' + 1);
  if (c >= 'a' && c <= 'z') c = (char)(c - 32);
  if (c >= 'A' && c <= 'Z') return (u8)(c - 'A' + 11);
  switch (c) { case ':': return 37; case '.': return 38; case '-': return 39; case '!': return 40; case '/': return 41; case '+': return 42; case '?': return 43; case '>': return 44; case '#': return 45; case '$': return 46; default: return 0; }
}
static void put_tiles(u16 addr, const u8 *t, u8 n) {
  u8 i;
  if (direct) { ppu_addr(addr); for (i = 0; i < n; i++) POKE(PPUDATA, t[i]); return; }
  if (vlen + 3 + n > VBMAX) return;
  vbuf[vlen] = (u8)(addr >> 8); vbuf[vlen + 1] = (u8)addr; vbuf[vlen + 2] = n; vlen += 3;
  for (i = 0; i < n; i++) vbuf[vlen++] = t[i];
}
static void put_str(u8 col, u8 row, const char *s) {
  u8 b[32], n = 0;
  while (*s && n < 32) b[n++] = tile_of(*s++);
  put_tiles((u16)(0x2000 + (u16)row * 32 + col), b, n);
}
static void center(u8 row, const char *s) { put_str((u8)(16 - strlen(s) / 2), row, s); }
static u8 numbuf[8];
static void fmt_num(u32 v, u8 width) { /* right aligned, blank padded, into numbuf */
  u8 i;
  for (i = 0; i < width; i++) numbuf[i] = 0;
  i = width;
  do { numbuf[--i] = (u8)(v % 10 + 1); v /= 10; } while (v && i);
}
static void put_num(u8 col, u8 row, u32 v, u8 width) { fmt_num(v, width); put_tiles((u16)(0x2000 + (u16)row * 32 + col), numbuf, width); }
static void box(u8 x0, u8 y0, u8 w, u8 h) { /* x0, y0, w, h even: a black panel (palette 0); direct mode only */
  u8 y, x, i;
  for (y = y0; y < y0 + h; y++) { ppu_addr((u16)(0x2000 + (u16)y * 32 + x0)); for (x = 0; x < w; x++) POKE(PPUDATA, 0); }
  for (y = y0; y < y0 + h; y += 2)
    for (x = x0; x < x0 + w; x += 2) { i = (u8)((y >> 2) * 8 + (x >> 2)); attr[i] = (u8)(attr[i] & ~(3 << (((y >> 1) & 1) * 4 + ((x >> 1) & 1) * 2))); }
}

/* ------------------------------------------------------------------ HUD */
static u32 h_score; static u8 h_level, h_lives, h_kills, h_bar, h_w, h_a0, h_a1, h_a2, h_wl, h_tt; static u16 h_sec; static const char *h_toast;
static void hud_reset(void) { h_score = 0xFFFFFFFFUL; h_level = h_lives = h_kills = h_bar = h_w = h_a0 = h_a1 = h_a2 = h_wl = h_tt = 255; h_sec = 0xFFFF; h_toast = (const char *)1; }
static void hud_labels(void) {
  put_str(1, 1, "SCORE"); put_str(22, 1, "TIME"); put_str(0, 2, "LV"); put_str(24, 2, "BOTS");
}
static void hud(u8 part) {
  u8 i, b[8], bar;
  if (part == 0) {
    if (G.score != h_score) { h_score = G.score; put_num(7, 1, G.score, 6); }
    if (G.level != h_level) { h_level = G.level; b[0] = (u8)(G.level + 1); b[1] = 41; b[2] = 4; put_tiles(0x2000 + 2 * 32 + 2, b, 3); }
  } else if (part == 1) {
    if (G.lives != h_lives) { h_lives = G.lives; for (i = 0; i < 3; i++) b[i] = i < G.lives ? 45 : 0; put_tiles(0x2000 + 2 * 32 + 6, b, 3); }
    bar = (u8)(G.php > 0 ? (u8)(((u16)G.php * 32u + 999u) / 1000u) : 0); /* 32 steps: 2 pixels each, 8 tiles of 8 pixels */
    if (bar != h_bar) {
      h_bar = bar;
      for (i = 0; i < 8; i++) { u8 n = bar > (u8)(i * 4) ? (u8)((bar - i * 4) * 2) : 0; if (n > 8) n = 8; b[i] = (u8)(48 + n); }
      put_tiles(0x2000 + 27 * 32 + 1, b, 8);
    }
  } else if (part == 2) {
    if (G.tsec != h_sec) {
      h_sec = G.tsec;
      b[0] = (u8)((G.tsec / 600) % 10 + 1); b[1] = (u8)((G.tsec / 60) % 10 + 1); b[2] = 37; b[3] = (u8)((G.tsec % 60) / 10 + 1); b[4] = (u8)(G.tsec % 10 + 1);
      put_tiles(0x2000 + 1 * 32 + 27, b, 5);
    }
    if (G.kills != h_kills) { h_kills = G.kills; b[0] = (u8)(G.kills % 10 + 1); put_tiles(0x2000 + 2 * 32 + 29, b, 1); }
  } else {
    if (G.level != h_wl || G.w != h_w || G.ammo[0] != h_a0 || G.ammo[1] != h_a1 || G.ammo[2] != h_a2) {
      static const u8 NAME[3] = {26, 29, 23}; /* P S M */
      u8 w[6];
      h_wl = G.level; h_w = G.w; h_a0 = G.ammo[0]; h_a1 = G.ammo[1]; h_a2 = G.ammo[2];
      for (i = 0; i < 3; i++) {
        w[0] = G.w == i ? 44 : 0; w[1] = (u8)(i + 2); w[2] = NAME[i];
        if (!W_AVAIL(i)) memset(w, 0, 6);
        else { w[3] = 0; w[4] = G.ammo[i] >= 10 ? (u8)(G.ammo[i] / 10 + 1) : 0; w[5] = (u8)(G.ammo[i] % 10 + 1); }
        put_tiles((u16)(0x2000 + 28 * 32 + 1 + i * 8), w, 6);
      }
    }
    if (G.toast != h_toast || (G.tt == 0) != (h_tt == 0)) {
      u8 t[5];
      h_toast = G.toast; h_tt = G.tt;
      memset(t, 0, 5);
      if (G.tt && G.toast) for (i = 0; i < 5 && G.toast[i]; i++) t[i] = tile_of(G.toast[i]);
      put_tiles(0x2000 + 28 * 32 + 26, t, 5);
    }
  }
}

/* ------------------------------------------------------------------ screens (the display is blanked while one is drawn) */
static u8 shown_mode = 255;
static void screen_draw(void) {
  u16 i; u8 mall = G.mode == M_PLAY || G.mode == M_INTRO || G.mode == M_LOST || G.mode == M_CLEAR;
  u8 b[8];
  ppumask = 0; frame_end(); /* rendering off (the NMI applies the mask) */
  direct = 1;
  ppu_addr(0x3F00); for (i = 0; i < 32; i++) POKE(PPUDATA, PALETTE[i]);
  ppu_addr(0x2000);
  if (G.mode == M_TITLE || G.mode == M_OVER || G.mode == M_WIN) { for (i = 0; i < 1024; i++) POKE(PPUDATA, 0); memset(attr, 0, 64); }
  else { for (i = 0; i < 64; i++) POKE(PPUDATA, 0); for (i = 0; i < 800; i++) POKE(PPUDATA, nt_rom[i]); for (i = 0; i < 160; i++) POKE(PPUDATA, 0); memcpy(attr, attr_rom, 64); }
  switch (G.mode) {
  case M_TITLE:
    center(5, "CHOPPING MALL"); center(7, "NES EDITION");
    center(10, "D-PAD: MOVE"); center(12, "A: FIRE AND AIM"); center(14, "B: NEXT WEAPON");
    center(16, "HOLD SELECT: RESTART"); center(19, "NO AMMO AT START!"); center(21, "VISIT THE $ DOORS");
    center(24, "PRESS START");
    break;
  case M_INTRO:
    box(6, 10, 20, 8); center(11, "LEVEL"); b[0] = (u8)(G.level + 1); put_tiles(0x2000 + 11 * 32 + 22, b, 1);
    center(14, G.level == 1 ? "PISTOL ONLY" : G.level == 2 ? "SHOTGUN UNLOCKED" : "MOLOTOV UNLOCKED"); center(16, "FIND AMMO");
    break;
  case M_LOST: box(6, 10, 20, 8); center(12, "LIFE LOST"); b[0] = (u8)(G.lives + 1); b[1] = 0; b[2] = tile_of('L'); b[3] = tile_of('E'); b[4] = tile_of('F'); b[5] = tile_of('T');
    put_tiles(0x2000 + 14 * 32 + 13, b, 6); break;
  case M_CLEAR: box(6, 10, 20, 8); center(12, "ROBOT DESTROYED"); center(14, "BONUS FOR SPEED"); break;
  case M_OVER: center(9, "GAME OVER"); center(12, "SCORE"); center(17, "PRESS START"); break;
  case M_WIN: center(7, "MALL SECURED!"); center(11, "YOU BEAT"); center(13, "THE KILLER ROBOT"); center(16, "SCORE"); center(22, "PRESS START"); break;
  default: break;
  }
  ppu_addr(0x23C0); for (i = 0; i < 64; i++) POKE(PPUDATA, attr[i]);
  hud_reset();
  if (mall) { hud_labels(); for (i = 0; i < 4; i++) hud((u8)i); }
  if (G.mode == M_OVER) put_num(13, 14, G.score, 6);
  if (G.mode == M_WIN) put_num(13, 18, G.score, 6);
  direct = 0;
  shown_mode = G.mode;
  ppumask = 0x1E; /* background + sprites, left column shown */
}

/* ------------------------------------------------------------------ sprites */
static u8 *op, nspr, lastspr; /* op: next free OAM entry */
static void spr(i16 x, i16 y, u8 tile, u8 at) {
  if (op >= oam + 256) return;
  op[0] = (u8)(y - 1); op[1] = tile; op[2] = at; op[3] = (u8)x;
  op += 4;
}
#define SX(gx) ((i16)(gx) - 32)   /* game px -> NES px: the 222 px wide mall is shifted left by 32 */
#define SY(gy) ((i16)(gy) + 16)
static void big(i16 cx, i16 cy, u8 dir, u8 first, u8 w, u8 pal) { /* a w x w tile sprite rotated to one of 32 directions (frames 0..8 + flips) */
  u8 base, fh = 0, fv = 0, i, j, t, at;
  i16 x, y, xs, dx, dy;
  if (dir <= 8) base = dir;
  else if (dir <= 16) { base = (u8)(16 - dir); fh = 1; }
  else if (dir <= 24) { base = (u8)(dir - 16); fh = 1; fv = 1; }
  else { base = (u8)(32 - dir); fv = 1; }
  at = (u8)(pal | (fh ? 0x40 : 0) | (fv ? 0x80 : 0));
  t = (u8)(first + (w == 2 ? (u8)(base << 2) : (u8)((base << 3) + base)));
  x = SX(cx) - (i16)(w << 2); y = SY(cy) - (i16)(w << 2);
  xs = fh ? x + (i16)((w - 1) << 3) : x; dx = fh ? -8 : 8;      /* a flipped sprite also mirrors the tile layout */
  if (fv) { y += (i16)((w - 1) << 3); dy = -8; } else dy = 8;
  for (j = 0; j < w; j++) {
    x = xs;
    for (i = 0; i < w; i++) { spr(x, y, t++, at); x += dx; }
    y += dy;
  }
}
static void sprites(void) {
  u8 i, show = G.mode == M_PLAY || G.mode == M_INTRO || G.mode == M_LOST || G.mode == M_CLEAR;
  op = oam;
  if (show) {
    if (!(G.pinv && G.mode == M_PLAY && (G.tick & 2))) big(G.px >> FP, G.py >> FP, G.pface, 12, 2, 0);  /* lower OAM index = in front, and survives the 8-per-line limit */
    if (!G.rdead) big(G.rx >> FP, G.ry >> FP, G.rface, 48, 3, G.rwind ? 3 : 1);
    for (i = 0; i < 3; i++)
      if (G.pr[i].kind) spr(SX(G.pr[i].x >> FP) - 4, SY(G.pr[i].y >> FP) - 4, G.pr[i].kind == PK_MOLOTOV ? 3 : 1, 2);
    if (G.bolt.life) spr(SX(G.bolt.x >> FP) - 4, SY(G.bolt.y >> FP) - 4, 2, 2);
    for (i = 0; i < 2; i++)
      if (G.fire[i].life) {
        u8 t = (G.tick >> 2) & 1 ? 8 : 4; i16 x = SX(G.fire[i].x >> FP) - 8, y = SY(G.fire[i].y >> FP) - 8;
        spr(x, y, t, 2); spr(x + 8, y, (u8)(t + 1), 2); spr(x, y + 8, (u8)(t + 2), 2); spr(x + 8, y + 8, (u8)(t + 3), 2);
      }
    for (i = 0; i < 6; i++) /* shop doors that can be used right now: a $ marker */
      if (G.cool[i] == 0 && shop_open(shop_kind[i])) spr(SX(doorx[i]) - 4, SY(doory[i]) - 4, 129, 2);
  }
  nspr = (u8)((op - oam) >> 2);
  for (i = nspr; i < lastspr; i++) oam[(u8)(i << 2)] = 0xFF; /* hide the sprites of the previous frame that are no longer used */
  lastspr = nspr;
}

/* ------------------------------------------------------------------ sound (APU) - filled in below */
static void snd_frame(void);
static void snd_init(void);

/* ------------------------------------------------------------------ input */
static u8 read_pad(void) {
  u8 i, v = 0;
  POKE(0x4016, 1); POKE(0x4016, 0);
  for (i = 0; i < 8; i++) v = (u8)((v << 1) | (PEEK(0x4016) & 1));
  return v; /* A B Select Start Up Down Left Right, A in bit 7 */
}
static u8 evacc;
u8 dbg_pad;
static u8 read_input(void) {
  static u8 prev, sel;
  u8 pad = read_pad(), in = 0, k = 0, edge;
  dbg_pad = pad;
  if (pad & 0x08) in |= IN_UP;
  if (pad & 0x04) in |= IN_DOWN;
  if (pad & 0x02) in |= IN_LEFT;
  if (pad & 0x01) in |= IN_RIGHT;
  if (pad & 0x80) { in |= IN_FIRE; k |= 1; }
  if (pad & 0x10) k |= 1;                      /* Start */
  if (pad & 0x40) k |= 4;                      /* B: next weapon */
  if (pad & 0x20) { if (sel < 255) sel++; } else sel = 0; /* Select held for one second: restart */
  if (sel == 60) k |= 2;
  edge = (u8)(k & ~prev); prev = k;
  if (edge & 1) evacc |= EV_START;
  if (edge & 2) evacc |= EV_RESTART;
  if (edge & 4) { u8 n = (u8)((G.w + 1) % (G.level ? G.level : 1)); evacc |= (u8)(EV_W1 << n); }
  return in;
}

/* ------------------------------------------------------------------ region: a frame is 29780 CPU cycles on NTSC and 33247 on PAL */
u16 region_count; /* loop iterations in one frame (debug) */
static void detect_region(void) {
  u8 c;
  u16 n = 0;
  c = nmi_count; while (nmi_count == c) {}   /* line up with an NMI, then count to the next one */
  c = nmi_count; while (nmi_count == c) n++;
  region_count = n;
  pal = n > REGION_LIMIT;
  if (pal) { per_p = PER_PP; per_t = PER_TP; }
}

/* ------------------------------------------------------------------ main */
#ifdef NES_PROF /* time spent in each part, in video frames (sum of NMI counts): read from RAM by tools/prof.sh */
u16 prof_tick, prof_draw, prof_iter;
#define PMARK(v) { u8 t_ = nmi_count; v += (u8)(t_ - pm_); pm_ = t_; }
#else
#define PMARK(v)
#endif
void main(void) {
  u8 in, ph = 0, hp = 0, start, owed = 0, n;
#ifdef NES_PROF
  u8 pm_ = 0;
#endif
  game_init(1);
  snd_init();
  ppuctrl = 0x88;   /* NMI on, background pattern table $0000, sprite table $1000, 8x8 sprites */
  POKE(0x2000, ppuctrl);
#ifndef NO_DETECT
  detect_region();
#endif
  screen_draw();
  start = nmi_count;
  for (;;) {
    PMARK(prof_iter)
    in = read_input();
    n = (u8)(1 + owed); /* a pass that overran its frame (heavy robot AI) leaves the core owing ticks, so the game speed stays right */
    owed = 0;
    while (n--) {
      if (!pal && ++ph >= 6) { ph = 0; continue; } /* the core runs at 50 Hz: every PAL frame, 5 of every 6 NTSC frames */
      {
        u8 ev = evacc; evacc = 0;
#ifdef AUTOPLAY
        in = bot_input(&ev);
#endif
        game_tick(in, ev);
        snd_frame();
      }
    }
    PMARK(prof_tick)
    if (G.mode != shown_mode) { screen_draw(); start = nmi_count; }
#ifndef NO_HUD
    if (shown_mode == M_PLAY || shown_mode == M_INTRO || shown_mode == M_LOST || shown_mode == M_CLEAR) hud((u8)(hp++ & 3));
#endif
#ifndef NO_SPRITES
    sprites();
#endif
    PMARK(prof_draw)
    owed = (u8)(nmi_count - start); /* video frames that went by while this pass was working */
    if (owed > 2) owed = 2;
    frame_end();
    start = nmi_count;
  }
}

/* ------------------------------------------------------------------ APU
 * pulse 1 = music lead / jingle voice 0, pulse 2 = sound effects / jingle voice 2, triangle = bass / jingle voice 1, noise = drums.
 * Everything steps once per core tick (50 Hz), so the tempo equals the C64 version's. Envelopes are done in software (volume writes). */
#define APU(r) (*(volatile u8 *)(0x4000 + (r)))
typedef struct { u8 duty, vol, sus, step, cnt, rel; u16 per; u8 hi; } Pulse;
static Pulse pu[2];
static u8 mus_on, mus_pos, mus_f, jingle_on, tri_on, nz_vol, nz_dec;
static u8 sfx_n, sfx_t, sfx_prio, sfx_duty; static u16 sfx_p; static i16 sfx_dp;
static const u8 *jp[3]; static u8 jleft[3];
static const u8 INS_VOL[6] = {11, 9, 12, 12, 13, 10}, INS_SUS[6] = {9, 8, 0, 10, 0, 0}, INS_STEP[6] = {6, 12, 5, 8, 2, 3}, INS_DUTY[6] = {2, 1, 2, 1, 2, 1};

static void pulse_write(u8 ch, u8 retrig) {
  Pulse *p = &pu[ch];
  APU(ch * 4) = (u8)((p->duty << 6) | 0x30 | p->vol);
  APU(ch * 4 + 2) = (u8)p->per;
  if (retrig || (u8)(p->per >> 8) != p->hi) { p->hi = (u8)(p->per >> 8); APU(ch * 4 + 3) = p->hi | 0xF8; }
}
static void pulse_note(u8 ch, u16 per, u8 duty, u8 vol, u8 sus, u8 step) {
  Pulse *p = &pu[ch];
  p->per = per; p->duty = duty; p->vol = vol; p->sus = sus; p->step = step; p->cnt = 0; p->rel = 0;
  pulse_write(ch, 1);
}
static void pulse_env(u8 ch) { /* one step of the software envelope */
  Pulse *p = &pu[ch];
  if (p->rel) { p->vol = p->vol >= 2 ? (u8)(p->vol - 2) : 0; }
  else if (p->vol > p->sus && ++p->cnt >= p->step) { p->cnt = 0; p->vol--; }
  APU(ch * 4) = (u8)((p->duty << 6) | 0x30 | p->vol);
}
static void tri_set(u8 on, u16 per) {
  if (on) { APU(0x0A) = (u8)per; APU(0x0B) = (u8)((per >> 8) | 0xF8); APU(8) = 0xFF; } else APU(8) = 0x80;
  tri_on = on;
}
static void noise_hit(u8 per, u8 vol, u8 dec) { APU(0x0C) = (u8)(0x30 | vol); APU(0x0E) = per; APU(0x0F) = 0xF8; nz_vol = vol; nz_dec = dec; }
static u16 hz_period(u16 f) { u32 p; if (f < 55) f = 55; p = (pal ? 103913UL : 111861UL) / f; return (u16)(p > 0 ? p - 1 : 0); }

static void snd_init(void) {
  POKE(0x4015, 0x0F);
  APU(1) = 0x08; APU(5) = 0x08; APU(8) = 0x80; APU(0x0C) = 0x30; APU(0) = 0x30; APU(4) = 0x30;
}
static void sfx_start(u16 f1, u8 frames, u8 duty, u16 f2, u8 prio) {
  u16 p1 = hz_period(f1), p2 = hz_period(f2);
  if (jingle_on) return;                      /* the stinger uses the voices */
  if (sfx_n && prio < sfx_prio) return;
  sfx_prio = prio; sfx_n = frames ? frames : 1; sfx_t = 0; sfx_duty = duty;
  sfx_p = p1; sfx_dp = (i16)(((i16)p2 - (i16)p1) / (i16)sfx_n);
  pulse_note(1, p1, duty, 12, 0, 255);
}
void snd_sfx(i16 f1, u8 frames, u8 wave, i16 f2) {
  static const u8 DUTY[4] = {2, 1, 0, 2}; /* sine / square / saw / triangle */
  sfx_start((u16)f1, frames, DUTY[wave & 3], (u16)f2, 2);
}
void snd_clank(void) { sfx_start(1250, 6, 1, 1530, 1); }
static void jingle_end(void) {
  jingle_on = 0; pu[0].vol = 0; pu[1].vol = 0; pulse_write(0, 0); pulse_write(1, 0); tri_set(0, 0); mus_f = 0;
}
void snd_jingle(u8 kind) {
  const u8 *const *sc = kind == JINGLE_WIN ? JW_S : JL_S;
  u8 v;
  sfx_n = 0; sfx_prio = 0;
  for (v = 0; v < 3; v++) { jp[v] = sc[v]; jleft[v] = 0; }
  pu[0].vol = 0; pu[1].vol = 0; pulse_write(0, 0); pulse_write(1, 0); tri_set(0, 0);
  jingle_on = 1;
}
void snd_new_game(void) { mus_on = 1; if (jingle_on) jingle_end(); mus_pos = 0; mus_f = 0; }
static void jingle_frame(void) { /* one 50 Hz step of the three event streams: wait, note, instrument */
  u8 v, active = 0;
  for (v = 0; v < 3; v++) {
    while (jp[v] && jleft[v] == 0) {
      u8 d = jp[v][0], n = jp[v][1], in = jp[v][2], i = (u8)(in & 0x7F);
      if (n == 255) { if (v == 1) tri_set(0, 0); else pu[v ? 1 : 0].rel = 1; jp[v] = 0; break; }
      jp[v] += 3;
      if (n == 0) { if (v == 1) tri_set(0, 0); else pu[v ? 1 : 0].rel = 1; }  /* gate off */
      else if (n != 254) {
        if (v == 1) tri_set(1, per_t[n]);
        else {
          Pulse *p = &pu[v ? 1 : 0];
          if (in & 0x80) { p->per = per_p[n]; pulse_write((u8)(v ? 1 : 0), 0); }      /* pitch change only */
          else pulse_note((u8)(v ? 1 : 0), per_p[n], INS_DUTY[i], INS_VOL[i], INS_SUS[i], INS_STEP[i]);
        }
      }
      jleft[v] = d;
    }
    if (jp[v]) { active = 1; jleft[v]--; }
  }
  if (jingle_on && !active) jingle_end();
}
static void snd_frame(void) {
  if (sfx_n) { /* effects voice: a linear period glide with a fading volume */
    sfx_p = (u16)((i16)sfx_p + sfx_dp);
    pu[1].per = sfx_p; pu[1].vol = (u8)(12 - (u8)((u16)12 * sfx_t / sfx_n));
    pulse_write(1, 0);
    if (++sfx_t >= sfx_n) { sfx_n = 0; sfx_prio = 0; pu[1].vol = 0; APU(4) = 0x30; }
  }
  if (nz_vol) { nz_vol = nz_vol > nz_dec ? (u8)(nz_vol - nz_dec) : 0; APU(0x0C) = (u8)(0x30 | nz_vol); }
  if (jingle_on) { jingle_frame(); pulse_env(0); if (!sfx_n) pulse_env(1); return; }
  if (!mus_on) return;
  if (mus_f == 0) {
    u8 n = MUS_LEAD[mus_pos], b = MUS_BASS[mus_pos], d = MUS_DRUM[mus_pos];
    if (n) pulse_note(0, per_p[n], 2, 10, 4, 2);
    if (b) tri_set(1, per_t[b]);
    if (d & 1) noise_hit(12, 13, 3);        /* kick */
    else if (d & 2) noise_hit(6, 11, 3);    /* snare */
    else if (d & 4) noise_hit(2, 7, 4);     /* hat */
  } else if (mus_f == 5) { pu[0].rel = 1; tri_set(0, 0); }
  pulse_env(0);
  if (++mus_f >= 7) { mus_f = 0; if (++mus_pos >= 96) mus_pos = 0; }
}
