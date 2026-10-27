// play.c - load campaign, run the explore loop
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "game/game.h"
#include "gfx/gfx.h"
#include "plat/plat.h"

void game_msg(Game *g, u32 rgb, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(g->msg, sizeof g->msg, fmt, ap);
  va_end(ap);
  g->msg_rgb = rgb; g->msg_t = g->now ? g->now : 1;
  SDL_Log("%s", g->msg);
}

// load the map whose id is the start map (fallback: first listed)
static int load_start(Game *g, Err *e) {
  char path[700];
  int i;
  for (i = 0; i < g->c.nmaps; i++) {
    snprintf(path, sizeof path, "%s/%s", g->dir, g->c.maps[i]);
    memset(&g->m, 0, sizeof g->m);
    if (map_load(&g->m, path, e)) { SDL_Log("%s: %s (line %d)", path, e->msg, e->line); return -1; }
    if (!strcmp(g->m.id, g->c.start_map) || (!g->c.start_map[0] && i == 0)) return 0;
    map_free(&g->m);
  }
  err_set(e, 0, "start map '%s' not in maps", g->c.start_map);
  return -1;
}

static int on_in(void *ud, int in) { return explore_in((Game *)ud, in); }

static void on_draw(void *ud, SDL_Renderer *r, u64 ms) {
  Game *g = (Game *)ud;
  g->now = ms;
  view_draw(r, &g->m, g->p);
  if (ms - g->fade < 120) {     // step fade: dark overlay ramps out
    SDL_FRect a = { 0, 0, VW, VH };
    SDL_SetRenderDrawColor(r, 0, 0, 0, (u8)(160 * (120 - (ms - g->fade)) / 120));
    SDL_RenderFillRect(r, &a);
  }
  amap_draw(r, &g->m, &g->am, g->p, g->big);
  if (g->msg_t && ms - g->msg_t < 2500) {  // message indicator (text is M2)
    SDL_FRect b = { 0, VH - 8, VW, 8 };
    SDL_SetRenderDrawColor(r, (u8)(g->msg_rgb >> 16), (u8)(g->msg_rgb >> 8), (u8)g->msg_rgb, 220);
    SDL_RenderFillRect(r, &b);
  }
}

int play(const char *dir) {
  static Game g;                // large; keep off the stack
  Plat pl;
  Err e;
  memset(&g, 0, sizeof g); memset(&e, 0, sizeof e);
  snprintf(g.dir, sizeof g.dir, "%s", dir);
  if (db_campaign(&g.c, dir, &e)) { fprintf(stderr, "%s: %s (line %d)\n", dir, e.msg, e.line); return 1; }
  if (load_start(&g, &e)) { fprintf(stderr, "%s: %s\n", dir, e.msg); return 1; }
  g.p.x = g.c.start_x; g.p.y = g.c.start_y; g.p.dir = g.c.start_dir & 3;
  if (!map_in(&g.m, g.p.x, g.p.y)) { fprintf(stderr, "%s: start outside map\n", dir); map_free(&g.m); return 1; }
  amap_init(&g.am, &g.m);
  amap_visit(&g.am, &g.m, g.p);
  if (plat_init(&pl, "crawl")) { amap_free(&g.am); map_free(&g.m); return 1; }
  plat_run(&pl, on_in, on_draw, &g);
  plat_quit(&pl);
  amap_free(&g.am); map_free(&g.m);
  return 0;
}
