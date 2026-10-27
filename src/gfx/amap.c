// amap.c - auto automap: corner minimap or full screen
#include "gfx/gfx.h"
#include "plat/plat.h"

static void col(SDL_Renderer *r, u8 e) {
  switch (ED_TYPE(e)) {
  case ED_DOOR: SDL_SetRenderDrawColor(r, 240, 220, 60, 255); break;
  case ED_LOCK: SDL_SetRenderDrawColor(r, 250, 140, 30, 255); break;
  case ED_OWGO: case ED_OWNO: SDL_SetRenderDrawColor(r, 60, 220, 230, 255); break;
  case ED_SECRET:
    if (e & EF_FOUND) { SDL_SetRenderDrawColor(r, 240, 220, 60, 255); break; }
    /* fallthrough */
  default: SDL_SetRenderDrawColor(r, 235, 235, 235, 255); break;
  }
}

void amap_draw(SDL_Renderer *r, const Map *m, const Amap *a, Pos p, int full) {
  SDL_FRect box = full ? (SDL_FRect){ 90, 10, 460, 460 } : (SDL_FRect){ VW - 168, 8, 160, 160 };
  float cs, ox, oy;
  int x, y, s;
  if (m->w <= 0 || m->h <= 0) return;
  cs = box.w / m->w < box.h / m->h ? box.w / m->w : box.h / m->h;
  ox = box.x + (box.w - cs * m->w) / 2; oy = box.y + (box.h - cs * m->h) / 2;
  SDL_SetRenderDrawColor(r, 0, 0, 0, full ? 230 : 170);
  SDL_RenderFillRect(r, &box);
  for (y = 0; y < m->h; y++)
    for (x = 0; x < m->w; x++) {
      float x0 = ox + x * cs, y0 = oy + y * cs;
      const Mark *mk;
      if (amap_visited(a, x, y)) {
        SDL_FRect c = { x0, y0, cs, cs };
        SDL_SetRenderDrawColor(r, 60, 70, 90, 255); SDL_RenderFillRect(r, &c);
        if ((mk = map_mark(m, x, y)) != NULL) {
          SDL_FColor k = gfx_hash(mk->look);
          SDL_FRect dot = { x0 + cs * .35f, y0 + cs * .35f, cs * .3f, cs * .3f };
          SDL_SetRenderDrawColorFloat(r, k.r, k.g, k.b, 1); SDL_RenderFillRect(r, &dot);
        }
      }
      for (s = 0; s < 4; s++) {
        u8 e = map_edge(m, x, y, s);
        float ax, ay, bx, by, mx, my;
        if (ED_TYPE(e) == ED_OPEN || !amap_seen(a, x, y, s)) continue;
        if (ED_TYPE(e) == ED_FALSE) e = ED_WALL;
        ax = x0 + (s == DE ? cs : 0); ay = y0 + (s == DS ? cs : 0);
        bx = x0 + (s == DW ? 0 : cs); by = y0 + (s == DN ? 0 : cs);
        col(r, e);
        SDL_RenderLine(r, ax, ay, bx, by);
        if (ED_TYPE(e) == ED_OWGO) {  // chevron pointing out through the passable side
          float t = cs * .2f;
          mx = (ax + bx) / 2; my = (ay + by) / 2;
          SDL_RenderLine(r, mx - dir_dx[s] * t + dir_dy[s] * t, my - dir_dy[s] * t + dir_dx[s] * t, mx, my);
          SDL_RenderLine(r, mx - dir_dx[s] * t - dir_dy[s] * t, my - dir_dy[s] * t - dir_dx[s] * t, mx, my);
        }
      }
    }
  {
    float cx = ox + (p.x + .5f) * cs, cy = oy + (p.y + .5f) * cs, t = cs * .38f;
    int dx = dir_dx[p.dir], dy = dir_dy[p.dir];
    SDL_FPoint tri[3];
    tri[0].x = cx + dx * t;            tri[0].y = cy + dy * t;
    tri[1].x = cx - dx * t - dy * t;   tri[1].y = cy - dy * t + dx * t;
    tri[2].x = cx - dx * t + dy * t;   tri[2].y = cy - dy * t - dx * t;
    gfx_poly(r, tri, 3, gfx_rgb(255, 80, 80, 255));
  }
}
