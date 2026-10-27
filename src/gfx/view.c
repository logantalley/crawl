// view.c - flat-color first-person view; geometry computed per depth plane
#include "gfx/gfx.h"
#include "plat/plat.h"

// half-width of one cell at edge planes 0..4; half-height = 3/4 of that
static const float hw[5] = { 320, 200, 124, 77, 48 };
static const float spr_k[4] = { 1, .62f, .38f, .24f };
#define CX (VW / 2.f)
#define CY (VH / 2.f)

SDL_FColor gfx_rgb(u8 r, u8 g, u8 b, u8 a) {
  SDL_FColor c = { r / 255.f, g / 255.f, b / 255.f, a / 255.f };
  return c;
}

SDL_FColor gfx_hash(const char *s) {
  u32 h = 2166136261u;
  for (; *s; s++) h = (h ^ (u8)*s) * 16777619u;
  return gfx_rgb((u8)(80 + (h & 127)), (u8)(80 + ((h >> 8) & 127)), (u8)(80 + ((h >> 16) & 127)), 255);
}

void gfx_poly(SDL_Renderer *r, const SDL_FPoint *pt, int n, SDL_FColor c) {
  SDL_Vertex v[8]; int idx[18], ni = 0, i;
  if (n > 8) n = 8;
  for (i = 0; i < n; i++) { v[i].position = pt[i]; v[i].color = c; v[i].tex_coord.x = v[i].tex_coord.y = 0; }
  for (i = 1; i + 1 < n; i++) { idx[ni++] = 0; idx[ni++] = i; idx[ni++] = i + 1; }
  SDL_RenderGeometry(r, NULL, v, n, idx, ni);
}

static SDL_FColor shade(SDL_FColor c, float f) { c.r *= f; c.g *= f; c.b *= f; return c; }

// bilinear point in quad q = {tl, tr, br, bl}
static SDL_FPoint qpt(const SDL_FPoint *q, float u, float v) {
  SDL_FPoint a, b, p;
  a.x = q[0].x + (q[1].x - q[0].x) * u; a.y = q[0].y + (q[1].y - q[0].y) * u;
  b.x = q[3].x + (q[2].x - q[3].x) * u; b.y = q[3].y + (q[2].y - q[3].y) * u;
  p.x = a.x + (b.x - a.x) * v; p.y = a.y + (b.y - a.y) * v;
  return p;
}

static void subq(SDL_Renderer *r, const SDL_FPoint *q, float u0, float v0, float u1, float v1, SDL_FColor c) {
  SDL_FPoint s[4];
  s[0] = qpt(q, u0, v0); s[1] = qpt(q, u1, v0); s[2] = qpt(q, u1, v1); s[3] = qpt(q, u0, v1);
  gfx_poly(r, s, 4, c);
}

// visual class of an edge seen from its own cell
enum { VS_NONE, VS_WALL, VS_DOOR, VS_LOCK };
static int vis(u8 e) {
  switch (ED_TYPE(e)) {
  case ED_OPEN: return VS_NONE;
  case ED_DOOR: case ED_OWGO: return VS_DOOR;
  case ED_LOCK: return (e & EF_UNLOCK) ? VS_DOOR : VS_LOCK;
  case ED_SECRET: return (e & EF_FOUND) ? VS_DOOR : VS_WALL;
  default: return VS_WALL;      // wall, false, OWNO
  }
}

static void wall(SDL_Renderer *r, const SDL_FPoint *q, u8 e, float f) {
  int k = vis(e);
  if (k == VS_NONE) return;
  gfx_poly(r, q, 4, shade(gfx_rgb(120, 112, 100, 255), f));
  if (k == VS_WALL) return;
  subq(r, q, .25f, .2f, .75f, 1, shade(gfx_rgb(110, 70, 35, 255), f));
  subq(r, q, .3f, .26f, .7f, 1, shade(gfx_rgb(140, 90, 45, 255), f));
  if (k == VS_LOCK) subq(r, q, .6f, .55f, .65f, .65f, shade(gfx_rgb(230, 190, 60, 255), f));
}

static void sprite(SDL_Renderer *r, int d, int o, SDL_FColor c) {
  float h = (hw[d] + hw[d + 1]) / 2, s = 90 * spr_k[d];
  float x = CX + 2 * o * h, y = CY + h * .75f - s - (d ? 0 : 20);
  SDL_FPoint p[4];
  p[0].x = x; p[0].y = y - s; p[1].x = x + s * .7f; p[1].y = y;
  p[2].x = x; p[2].y = y + s; p[3].x = x - s * .7f; p[3].y = y;
  gfx_poly(r, p, 4, c);
}

static void cell(SDL_Renderer *r, const Map *m, Pos p, int d, int o) {
  int x, y, i, dl = (p.dir + 3) & 3, dr = (p.dir + 1) & 3;
  float n = hw[d], fa = hw[d + 1], f = 1 - .2f * d;
  map_rel(p, d, o, &x, &y);
  if (!map_in(m, x, y) || m->cell[y * m->w + x] == CL_ROCK) return;
  // front (far side) at plane d+1
  {
    SDL_FPoint q[4];
    q[0].x = CX + (2 * o - 1) * fa; q[0].y = CY - fa * .75f;
    q[1].x = CX + (2 * o + 1) * fa; q[1].y = q[0].y;
    q[2].x = q[1].x; q[2].y = CY + fa * .75f;
    q[3].x = q[0].x; q[3].y = q[2].y;
    wall(r, q, map_edge(m, x, y, p.dir), f * .95f);
  }
  // side walls facing the viewer column
  for (i = 0; i < 2; i++) {
    int s = i ? dr : dl; float sx = i ? 1.f : -1.f;
    SDL_FPoint q[4];
    if ((i == 0 && o > 0) || (i == 1 && o < 0)) continue;
    q[0].x = CX + (2 * o + sx) * n;  q[0].y = CY - n * .75f;
    q[1].x = CX + (2 * o + sx) * fa; q[1].y = CY - fa * .75f;
    q[2].x = q[1].x;                 q[2].y = CY + fa * .75f;
    q[3].x = q[0].x;                 q[3].y = CY + n * .75f;
    wall(r, q, map_edge(m, x, y, s), f * .8f);
  }
  // sprites: marker look, FOEs at path[0]
  {
    const Mark *mk = map_mark(m, x, y);
    if (mk) sprite(r, d, o, gfx_hash(mk->look));
    for (i = 0; i < m->nfoe; i++)
      if (m->foe[i].n > 0 && m->foe[i].path[0][0] == x && m->foe[i].path[0][1] == y)
        sprite(r, d, o, gfx_rgb(200, 30, 30, 255));
  }
}

void view_draw(SDL_Renderer *r, const Map *m, Pos p) {
  static const int ord[5] = { -2, 2, -1, 1, 0 };
  SDL_FRect c = { 0, 0, VW, CY }, fl = { 0, CY, VW, CY };
  int d, i, dmax = map_zone(m, p.x, p.y, "dark") ? 0 : 3;
  SDL_SetRenderDrawColor(r, 28, 26, 34, 255); SDL_RenderFillRect(r, &c);
  SDL_SetRenderDrawColor(r, 66, 58, 48, 255); SDL_RenderFillRect(r, &fl);
  if (!dmax) { SDL_SetRenderDrawColor(r, 6, 6, 10, 255); SDL_RenderFillRect(r, &c); SDL_RenderFillRect(r, &fl); }
  for (d = dmax; d >= 0; d--)
    for (i = 0; i < 5; i++) cell(r, m, p, d, ord[i]);
}
