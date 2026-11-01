/* main.c - window, input, frame loop and presentation (raylib). Everything drawn comes from
 * c2d; raylib only opens the window, reads keys, uploads the layer pixels as textures,
 * composites them with premultiplied blending, and pulls audio from the engine.
 *
 * usage: chopping-mall [--seed N] [--silent] [--frames N] [--screenshot file.png] [--replay inputs.txt] [--window WxH]
 * (--replay is a developer/test flag: feeds recorded per-frame input masks at a fixed 1/60 s step) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "raylib.h"
#include "rlgl.h"
#include "c2d/c2d.h"
#include "game.h"
#include "render.h"
#include "sound/sound_engine.h"

#define AUDIO_RATE 48000

/* ------------------------------------------------------------------ graphics resources */
typedef struct {
  int k;                         /* backing-store scale: surfaces are 960k x 760k */
  c2d *bg, *fr;
  Texture2D tex_bg, tex_w, tex_x, tex_h;
} Gfx;

/* PlutoVG surfaces are premultiplied BGRA bytes; the textures are uploaded as-is and this
 * shader swaps red and blue (so no per-pixel CPU conversion is needed). */
static const char *const FS_BGRA =
    "#version 330\n"
    "in vec2 fragTexCoord;\nin vec4 fragColor;\nuniform sampler2D texture0;\nuniform vec4 colDiffuse;\nout vec4 finalColor;\n"
    "void main() { finalColor = texture(texture0, fragTexCoord).bgra * colDiffuse * fragColor; }\n";

static Texture2D make_texture(int w, int h) {
  Image img = GenImageColor(w, h, BLANK);
  Texture2D t = LoadTextureFromImage(img);
  UnloadImage(img);
  SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
  SetTextureWrap(t, TEXTURE_WRAP_CLAMP);
  return t;
}

static int gfx_create(Gfx *g, int k) {
  memset(g, 0, sizeof *g);
  g->k = k;
  int w = SCREEN_W * k, h = SCREEN_H * k;
  g->bg = c2d_create(SCREEN_W, SCREEN_H, k, 1, 0);
  g->fr = c2d_create(SCREEN_W, SCREEN_H, k, FRAME_LAYERS, 1);
  if (!g->bg || !g->fr) return 0;
  build_bg(g->bg); /* js: buildBG(), once */
  g->tex_bg = make_texture(w, h);
  g->tex_w = make_texture(w, h);
  g->tex_x = make_texture(w, h);
  g->tex_h = make_texture(w, h);
  UpdateTexture(g->tex_bg, c2d_layer_data(g->bg, 0));
  return 1;
}

static void gfx_destroy(Gfx *g) {
  if (g->tex_bg.id) UnloadTexture(g->tex_bg);
  if (g->tex_w.id) UnloadTexture(g->tex_w);
  if (g->tex_x.id) UnloadTexture(g->tex_x);
  if (g->tex_h.id) UnloadTexture(g->tex_h);
  c2d_destroy(g->bg);
  c2d_destroy(g->fr);
  memset(g, 0, sizeof *g);
}

static void upload_layer(Gfx *g, int layer, Texture2D tex) {
  UpdateTexture(tex, c2d_layer_data(g->fr, layer)); /* stride == width * 4 for PlutoVG surfaces */
}

/* premultiplied blending: out = src + dst * (1 - src.a); additive: out = src + dst */
static void blend_premultiplied(void) { rlSetBlendFactors(RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD); BeginBlendMode(BLEND_CUSTOM); }
static void blend_additive(void) { rlSetBlendFactors(RL_ONE, RL_ONE, RL_FUNC_ADD); BeginBlendMode(BLEND_CUSTOM); }

/* draws one layer into the letterboxed viewport, shifted by (sx, sy) logical pixels */
static void draw_layer(const Gfx *g, Texture2D tex, Rectangle dst, double sx, double sy) {
  float w = (float)(SCREEN_W * g->k), h = (float)(SCREEN_H * g->k);
  /* shifting the image by +sx means sampling at x - sx; the clamped wrap repeats edge pixels */
  Rectangle src = {(float)(-sx * g->k), (float)(-sy * g->k), w, h};
  DrawTexturePro(tex, src, dst, (Vector2){0, 0}, 0, WHITE);
}

/* ------------------------------------------------------------------ audio */
static void audio_callback(void *buffer, unsigned int frames) { snd_engine_render((float *)buffer, (int)frames); }

/* ------------------------------------------------------------------ input */
static void read_input(Game *g) {
  if (!IsWindowFocused()) { game_clear_keys(g); return; } /* js: blur handler */
  g->K.ArrowUp = IsKeyDown(KEY_UP); g->K.ArrowDown = IsKeyDown(KEY_DOWN);
  g->K.ArrowLeft = IsKeyDown(KEY_LEFT); g->K.ArrowRight = IsKeyDown(KEY_RIGHT);
  g->K.KeyW = IsKeyDown(KEY_W); g->K.KeyA = IsKeyDown(KEY_A); g->K.KeyS = IsKeyDown(KEY_S); g->K.KeyD = IsKeyDown(KEY_D);
  g->K.Space = IsKeyDown(KEY_SPACE);
  /* edge events, same order as the keydown handler */
  if (IsKeyPressed(KEY_M)) game_key_down(g, GK_M);
  if (IsKeyPressed(KEY_R)) game_key_down(g, GK_R);
  else if (IsKeyPressed(KEY_ENTER)) game_key_down(g, GK_ENTER);
  else if (IsKeyPressed(KEY_SPACE)) game_key_down(g, GK_SPACE);
  if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_KP_1)) game_key_down(g, GK_DIGIT1);
  if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_KP_2)) game_key_down(g, GK_DIGIT2);
  if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_KP_3)) game_key_down(g, GK_DIGIT3);
}

/* developer flag: apply a recorded input mask (same encoding as tools/golden/trace_js.js) */
static void apply_mask(Game *g, int m) {
  g->K = (Keys){0};
  g->K.ArrowUp = !!(m & 1); g->K.ArrowDown = !!(m & 2); g->K.ArrowLeft = !!(m & 4); g->K.ArrowRight = !!(m & 8); g->K.Space = !!(m & 16);
  if (m & 32) game_key_down(g, GK_DIGIT1);
  if (m & 64) game_key_down(g, GK_DIGIT2);
  if (m & 128) game_key_down(g, GK_DIGIT3);
  if (m & 256) game_key_down(g, GK_R);
  if (m & 512) game_key_down(g, GK_ENTER);
  if (m & 1024) for (int i = 0; i < 3; i++) g->S.ammo[i] = MAXA[i];
  if ((m & 2048) && g->R.hp > 0.5) g->R.hp = 0.5;
}

int main(int argc, char **argv) {
  uint32_t seed = (uint32_t)time(NULL) ^ ((uint32_t)clock() << 8);
  int silent = 0, max_frames = -1;
  const char *shot = NULL, *replay_path = NULL;
  int win_w = SCREEN_W, win_h = SCREEN_H;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = (uint32_t)strtoul(argv[++i], NULL, 10);
    else if (!strcmp(argv[i], "--silent")) silent = 1;
    else if (!strcmp(argv[i], "--frames") && i + 1 < argc) max_frames = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) shot = argv[++i];
    else if (!strcmp(argv[i], "--replay") && i + 1 < argc) replay_path = argv[++i];
    else if (!strcmp(argv[i], "--window") && i + 1 < argc) {
      if (sscanf(argv[++i], "%dx%d", &win_w, &win_h) != 2 || win_w < 100 || win_h < 100) { fprintf(stderr, "bad --window\n"); return 2; }
    }
    else { fprintf(stderr, "usage: %s [--seed N] [--silent] [--frames N] [--screenshot file.png] [--replay inputs.txt] [--window WxH]\n", argv[0]); return 2; }
  }

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
  InitWindow(win_w, win_h, "CHOPPING MALL");
  if (!IsWindowReady()) { fprintf(stderr, "could not open a window\n"); return 1; }
  SetWindowMinSize(SCREEN_W / 2, SCREEN_H / 2);
  SetTargetFPS(120); /* vsync normally paces us; this caps drivers that ignore the hint */

  /* audio: if there is no device the game runs silently (commands just pile up and drop) */
  AudioStream stream = {0};
  int audio_ok = 0;
  if (!silent) {
    InitAudioDevice();
    if (IsAudioDeviceReady() && snd_engine_init(AUDIO_RATE)) {
      SetAudioStreamBufferSizeDefault(512);
      stream = LoadAudioStream(AUDIO_RATE, 32, 2);
      SetAudioStreamCallback(stream, audio_callback);
      PlayAudioStream(stream);
      audio_ok = 1;
    } else fprintf(stderr, "no audio device: running silently\n");
  }

  static Game G; /* big: lives in static storage */
  game_init(&G, seed);
  int *replay = NULL, nreplay = 0;
  if (replay_path) {
    FILE *f = fopen(replay_path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", replay_path); return 1; }
    int cap = 1024, v;
    replay = malloc(sizeof(int) * (size_t)cap);
    while (fscanf(f, "%d", &v) == 1) { if (nreplay == cap) replay = realloc(replay, sizeof(int) * (size_t)(cap *= 2)); replay[nreplay++] = v; }
    fclose(f);
    G.best.valid = 0;
  }

  Shader bgra = LoadShaderFromMemory(NULL, FS_BGRA);
  Gfx gfx;
  int k = 1;
  if (!gfx_create(&gfx, k)) { fprintf(stderr, "out of memory\n"); return 1; }

  int frame = 0;
  while (!WindowShouldClose()) {
    double dt = GetFrameTime();
    if (dt > 0.033) dt = 0.033; /* js: Math.min(0.033, ...) */

    if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
    if (replay) { dt = 1.0 / 60; apply_mask(&G, frame < nreplay ? replay[frame] : 0); }
    else read_input(&G);
    update(&G, dt);
    double sx, sy;
    game_pre_draw(&G, &sx, &sy);

    /* backing-store scale from the on-screen size, with hysteresis */
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    double s = fmin((double)sw / SCREEN_W, (double)sh / SCREEN_H);
    int want_k = gfx.k == 1 ? (s >= 1.5 ? 2 : 1) : (s < 1.25 ? 1 : 2);
    if (want_k != gfx.k) { gfx_destroy(&gfx); k = want_k; if (!gfx_create(&gfx, k)) break; }

    draw_frame(gfx.fr, &G);
    upload_layer(&gfx, LAYER_W, gfx.tex_w);
    upload_layer(&gfx, LAYER_X, gfx.tex_x);
    upload_layer(&gfx, LAYER_H, gfx.tex_h);

    BeginDrawing();
    ClearBackground((Color){7, 6, 11, 255}); /* the page background around the canvas */
    BeginShaderMode(bgra);
    float ds = (float)s;
    Rectangle dst = {((float)sw - SCREEN_W * ds) / 2, ((float)sh - SCREEN_H * ds) / 2, SCREEN_W * ds, SCREEN_H * ds};
    draw_layer(&gfx, gfx.tex_bg, dst, sx, sy);        /* opaque background (shaken) */
    blend_premultiplied();
    draw_layer(&gfx, gfx.tex_w, dst, sx, sy);         /* world, source-over */
    EndBlendMode();
    blend_additive();
    draw_layer(&gfx, gfx.tex_x, dst, sx, sy);         /* flames, sparks, bolts: 'lighter' */
    EndBlendMode();
    blend_premultiplied();
    draw_layer(&gfx, gfx.tex_h, dst, 0, 0);           /* HUD, not shaken */
    EndBlendMode();
    EndShaderMode();
    frame++;
    if (max_frames >= 0 && frame >= max_frames) {
      if (shot) {
        rlDrawRenderBatchActive();
        Image img = LoadImageFromScreen();
        ExportImage(img, shot);
        UnloadImage(img);
      }
      EndDrawing();
      break;
    }
    EndDrawing();
  }

  free(replay);
  UnloadShader(bgra);
  gfx_destroy(&gfx);
  if (audio_ok) { UnloadAudioStream(stream); snd_engine_shutdown(); }
  if (!silent) CloseAudioDevice();
  CloseWindow();
  return 0;
}
