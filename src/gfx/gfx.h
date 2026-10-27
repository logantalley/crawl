// gfx.h - first-person view and automap (flat color, M1)
#ifndef CRAWL_GFX_H
#define CRAWL_GFX_H
#include <SDL3/SDL.h>
#include "core/map.h"

void view_draw(SDL_Renderer *, const Map *, Pos);
void amap_draw(SDL_Renderer *, const Map *, const Amap *, Pos, int full);

// shared helpers (view.c)
SDL_FColor gfx_rgb(u8 r, u8 g, u8 b, u8 a);
SDL_FColor gfx_hash(const char *s);         // stable color for a look name
void gfx_poly(SDL_Renderer *, const SDL_FPoint *pt, int n, SDL_FColor c); // convex fan

#endif
