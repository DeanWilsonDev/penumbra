#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float x, y; } FPoint;
typedef struct { float r, g, b, a; } FColor;
typedef struct { FPoint position; FColor color; FPoint tex; } Vertex;
typedef struct { int x, y, w, h; } Rect;
typedef struct { float x, y, w, h; } FRect;
typedef struct { int magic; float w, h; int isTarget; } Texture;
typedef struct { int magic; float w, h; } Surface;
typedef struct { int magic; float size; } Font;
typedef struct { uint8_t r, g, b, a; } Color;

#define FAIL(...) do { fprintf(stderr, "stub SDL: " __VA_ARGS__); fprintf(stderr, "\n"); abort(); } while (0)

#ifdef STUB_CORE
static int frame = 0;
static int eventIndex = 0;
static long geometryCalls = 0, triangles = 0, fillRects = 0, textureDraws = 0, rotatedDraws = 0, clipCalls = 0;
static int liveTextures = 0;
static Texture* target = NULL;
static int window, renderer;
static char* clipboard = NULL;

bool SDL_Init(uint32_t flags) { return flags == 0x20; }
void SDL_Quit(void) {
  printf("stub SDL: %d frames, %ld geometry calls, %ld triangles, %ld fill rects, %ld texture draws, %ld rotated, %ld clip changes, %d live textures\n",
         frame, geometryCalls, triangles, fillRects, textureDraws, rotatedDraws, clipCalls, liveTextures);
}
const char* SDL_GetError(void) { return "stub error"; }
const char* SDL_GetPlatform(void) { return "Linux"; }
void SDL_free(void* memory) { free(memory); }
uint64_t SDL_GetTicksNS(void) { return (uint64_t)frame * 16666666ull; }
void* SDL_CreateWindow(const char* title, int w, int h, uint64_t flags) {
  if (!title || w != 960 || h != 680 || flags != (0x20 | 0x2000)) FAIL("bad window arguments %s %d %d %llx", title, w, h, (unsigned long long)flags);
  return &window;
}
void SDL_DestroyWindow(void* w) { if (w != &window) FAIL("bad window"); }
bool SDL_GetWindowSize(void* w, int* width, int* height) { if (w != &window) FAIL("bad window"); *width = 960; *height = 680; return true; }
bool SDL_GetWindowSizeInPixels(void* w, int* width, int* height) { if (w != &window) FAIL("bad window"); *width = 1920; *height = 1360; return true; }
float SDL_GetWindowDisplayScale(void* w) { (void)w; return 2.0f; }
bool SDL_SetWindowTitle(void* w, const char* t) { (void)w; (void)t; return true; }
bool SDL_StartTextInput(void* w) { (void)w; printf("stub SDL: text input on at frame %d\n", frame); return true; }
bool SDL_StopTextInput(void* w) { (void)w; printf("stub SDL: text input off at frame %d\n", frame); return true; }

static float mouseX = 0, mouseY = 0;
static uint32_t buttons = 0;
static const char* typed = "hi";

typedef struct { uint32_t type; uint32_t reserved; uint64_t timestamp; uint8_t payload[112]; } Event;

bool SDL_PollEvent(Event* event) {
  memset(event, 0, sizeof *event);
  int step = eventIndex++;
  if (frame == 3 && step == 0) { event->type = 0x401; event->payload[8] = 1; buttons = 1; return true; }
  if (frame == 4 && step == 0) { event->type = 0x402; event->payload[8] = 1; buttons = 0; return true; }
  if (frame == 8 && step == 0) { event->type = 0x401; event->payload[8] = 1; buttons = 1; return true; }
  if (frame == 9 && step == 0) { event->type = 0x402; event->payload[8] = 1; buttons = 0; return true; }
  if (frame == 11 && step == 0) { event->type = 0x303; const char** text = (const char**)(event->payload + 8); *text = typed; return true; }
  if (frame == 12 && step == 0) { event->type = 0x300; uint32_t key = 8; memcpy(event->payload + 12, &key, 4); uint16_t mod = 1; memcpy(event->payload + 16, &mod, 2); event->payload[20] = 1; return true; }
  if (frame == 13 && step == 0) { event->type = 0x301; uint32_t key = 8; memcpy(event->payload + 12, &key, 4); return true; }
  if (frame == 14 && step == 0) { event->type = 0x403; float wheel = -1.0f; memcpy(event->payload + 12, &wheel, 4); return true; }
  if (frame >= 40 && step == 0) { event->type = 0x100; return true; }
  eventIndex = 0;
  return false;
}
uint32_t SDL_GetMouseState(float* x, float* y) {
  if (frame < 6) { mouseX = 90; mouseY = 133; }
  else if (frame < 30) { mouseX = 120; mouseY = 205; }
  else { mouseX = 300; mouseY = 500; }
  *x = mouseX; *y = mouseY;
  return buttons;
}
uint16_t SDL_GetModState(void) { return 0; }
bool SDL_SetClipboardText(const char* text) { free(clipboard); clipboard = strdup(text); return true; }
bool SDL_HasClipboardText(void) { return clipboard != NULL; }
char* SDL_GetClipboardText(void) { return strdup(clipboard ? clipboard : ""); }

void* SDL_CreateRenderer(void* w, const char* name) { if (w != &window || name != NULL) FAIL("bad renderer arguments"); return &renderer; }
void SDL_DestroyRenderer(void* r) { if (r != &renderer) FAIL("bad renderer"); }
static void CheckRenderer(void* r) { if (r != &renderer) FAIL("bad renderer %p", r); }
bool SDL_SetRenderVSync(void* r, int v) { CheckRenderer(r); return v == 1; }
bool SDL_SetRenderDrawBlendMode(void* r, uint32_t m) { CheckRenderer(r); if (m != 1) FAIL("blend %u", m); return true; }
bool SDL_SetRenderDrawColor(void* r, uint8_t a, uint8_t b, uint8_t c, uint8_t d) { CheckRenderer(r); (void)a; (void)b; (void)c; (void)d; return true; }
bool SDL_RenderClear(void* r) { CheckRenderer(r); return true; }
bool SDL_RenderPresent(void* r) {
  CheckRenderer(r);
  if (target) FAIL("present with a texture target set");
  frame++;
  eventIndex = 0;
  return true;
}
static void CheckFinite(float value, const char* what) { if (!isfinite(value)) FAIL("%s is not finite", what); }
bool SDL_RenderFillRect(void* r, const FRect* rect) {
  CheckRenderer(r);
  if (!rect) FAIL("null fill rect");
  CheckFinite(rect->x, "fill x"); CheckFinite(rect->w, "fill w");
  fillRects++;
  return true;
}
bool SDL_RenderGeometry(void* r, void* texture, const Vertex* vertices, int vertexCount, const int* indices, int indexCount) {
  CheckRenderer(r);
  if (texture) FAIL("unexpected geometry texture");
  if (vertexCount <= 0 || !vertices) FAIL("no vertices");
  if (!indices || indexCount <= 0 || indexCount % 3 != 0) FAIL("index count %d", indexCount);
  for (int i = 0; i < vertexCount; ++i) {
    CheckFinite(vertices[i].position.x, "vertex x"); CheckFinite(vertices[i].position.y, "vertex y");
    const FColor c = vertices[i].color;
    if (!(c.r >= 0 && c.r <= 1 && c.g >= 0 && c.g <= 1 && c.b >= 0 && c.b <= 1 && c.a >= 0 && c.a <= 1)) FAIL("vertex %d colour out of range", i);
    if (vertices[i].position.x < -4000 || vertices[i].position.x > 6000 || vertices[i].position.y < -4000 || vertices[i].position.y > 6000)
      FAIL("vertex %d far away: %f %f", i, vertices[i].position.x, vertices[i].position.y);
  }
  for (int i = 0; i < indexCount; ++i) if (indices[i] < 0 || indices[i] >= vertexCount) FAIL("index %d = %d of %d", i, indices[i], vertexCount);
  geometryCalls++;
  triangles += indexCount / 3;
  return true;
}
bool SDL_SetRenderClipRect(void* r, const Rect* rect) {
  CheckRenderer(r);
  if (rect && (rect->w < 0 || rect->h < 0)) FAIL("negative clip");
  clipCalls++;
  return true;
}
void* SDL_GetRenderTarget(void* r) { CheckRenderer(r); return target; }
static void CheckTexture(Texture* t) { if (!t || t->magic != 0x7e7) FAIL("bad texture %p", (void*)t); }
bool SDL_SetRenderTarget(void* r, Texture* t) { CheckRenderer(r); if (t) { CheckTexture(t); if (!t->isTarget) FAIL("not a target"); } target = t; return true; }
Texture* SDL_CreateTexture(void* r, uint32_t format, int access, int w, int h) {
  CheckRenderer(r);
  if (format != 0x16462004u || access != 2 || w <= 0 || h <= 0 || w > 8192 || h > 8192) FAIL("bad texture %x %d %d %d", format, access, w, h);
  Texture* t = calloc(1, sizeof *t); t->magic = 0x7e7; t->w = (float)w; t->h = (float)h; t->isTarget = 1; liveTextures++;
  return t;
}
Texture* SDL_CreateTextureFromSurface(void* r, Surface* s) {
  CheckRenderer(r);
  if (!s || s->magic != 0x5f) FAIL("bad surface");
  Texture* t = calloc(1, sizeof *t); t->magic = 0x7e7; t->w = s->w; t->h = s->h; liveTextures++;
  return t;
}
void SDL_DestroyTexture(Texture* t) { CheckTexture(t); t->magic = 0; liveTextures--; free(t); }
void SDL_DestroySurface(Surface* s) { if (!s || s->magic != 0x5f) FAIL("bad surface"); s->magic = 0; free(s); }
bool SDL_GetTextureSize(Texture* t, float* w, float* h) { CheckTexture(t); *w = t->w; *h = t->h; return true; }
bool SDL_SetTextureBlendMode(Texture* t, uint32_t m) { CheckTexture(t); return m == 1; }
bool SDL_SetTextureScaleMode(Texture* t, int m) { CheckTexture(t); return m == 1; }
bool SDL_SetTextureAlphaModFloat(Texture* t, float a) { CheckTexture(t); if (a < 0 || a > 1) FAIL("alpha %f", a); return true; }
bool SDL_RenderTexture(void* r, Texture* t, const FRect* source, const FRect* destination) {
  CheckRenderer(r); CheckTexture(t);
  if (source) FAIL("unexpected source rect");
  if (!destination) FAIL("null destination");
  textureDraws++;
  return true;
}
bool SDL_RenderTextureRotated(void* r, Texture* t, const FRect* source, const FRect* destination, double angle, const FPoint* centre, int flip) {
  CheckRenderer(r); CheckTexture(t);
  if (source || !destination || !centre || flip != 0 || !isfinite(angle)) FAIL("bad rotated draw");
  rotatedDraws++;
  return true;
}
#endif

#ifdef STUB_TTF
static int fontsOpen = 0;
static char seen[256][128];
static int seenCount = 0;
bool TTF_Init(void) { return true; }
void TTF_Quit(void) { if (fontsOpen) FAIL("%d fonts still open", fontsOpen); }
Font* TTF_OpenFont(const char* path, float size) {
  FILE* file = fopen(path, "rb");
  if (!file) return NULL;
  fclose(file);
  if (!(size > 0 && size < 400)) FAIL("font size %f", size);
  Font* f = calloc(1, sizeof *f); f->magic = 0xf0; f->size = size; fontsOpen++;
  return f;
}
void TTF_CloseFont(Font* f) { if (!f || f->magic != 0xf0) FAIL("bad font"); f->magic = 0; fontsOpen--; free(f); }
bool TTF_GetStringSize(Font* f, const char* text, size_t length, int* w, int* h) {
  if (!f || f->magic != 0xf0) FAIL("bad font");
  if (length != 0) FAIL("length %zu", length);
  *w = (int)(strlen(text) * f->size * 0.6f); *h = (int)(f->size * 1.25f);
  return true;
}
int TTF_GetFontAscent(Font* f) { if (!f || f->magic != 0xf0) FAIL("bad font"); return (int)(f->size * 0.8f); }
Surface* TTF_RenderText_Blended(Font* f, const char* text, size_t length, Color color) {
  if (!f || f->magic != 0xf0) FAIL("bad font");
  if (length != 0) FAIL("length %zu", length);
  (void)color;
  int known = 0;
  for (int i = 0; i < seenCount; ++i) if (strcmp(seen[i], text) == 0) known = 1;
  if (!known && seenCount < 256) { snprintf(seen[seenCount++], 128, "%s", text); printf("stub TTF: rendered \"%s\" at %.0f\n", text, f->size); }
  Surface* s = calloc(1, sizeof *s); s->magic = 0x5f; s->w = strlen(text) * f->size * 0.6f; s->h = f->size * 1.25f;
  return s;
}
#endif

#ifdef STUB_IMAGE
void* IMG_LoadTexture(void* r, const char* path) { (void)r; (void)path; return NULL; }
#endif
