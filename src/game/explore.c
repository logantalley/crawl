// explore.c - maze movement, doors, search
#include <stdio.h>
#include "game/game.h"
#include "plat/plat.h"

static void moved(Game *g) { amap_visit(&g->am, &g->m, g->p); g->fade = g->now; }

static void step(Game *g, int rel) {
  int bs = 0, r = map_step(&g->m, &g->p, (g->p.dir + rel) & 3, &bs);
  switch (r) {
  case MV_OK: moved(g); break;
  case MV_LOCKED: game_msg(g, 0xff8c1e, "Locked."); break;
  case MV_ONEWAY: game_msg(g, 0x3cdce6, "It won't open from this side."); break;
  default: game_msg(g, 0x606060, "Bump."); break;
  }
}

static void act(Game *g) {
  Pos p = g->p;
  u8 e = map_edge(&g->m, p.x, p.y, p.dir);
  if (ED_TYPE(e) == ED_LOCK && !(e & EF_UNLOCK)) {
    const EdgeProp *ep = map_eprop(&g->m, p.x, p.y, p.dir);
    game_msg(g, 0xff8c1e, "Locked. (needs %s)", ep && ep->key[0] ? ep->key : "a key");
  } else game_msg(g, 0x606060, "Nothing happens.");
}

static void search(Game *g) {
  Pos p = g->p;
  u8 e = map_edge(&g->m, p.x, p.y, p.dir);
  if (ED_TYPE(e) == ED_SECRET && !(e & EF_FOUND)) {
    map_set_edge(&g->m, p.x, p.y, p.dir, (u8)(e | EF_FOUND));
    amap_visit(&g->am, &g->m, g->p);
    game_msg(g, 0xf0dc3c, "You find a secret door!");
  } else game_msg(g, 0x606060, "You find nothing.");
}

int explore_in(Game *g, int in) {
  switch (in) {
  case IN_FWD: step(g, 0); break;
  case IN_BACK: step(g, 2); break;
  case IN_SL: step(g, 3); break;
  case IN_SR: step(g, 1); break;
  case IN_TL: g->p.dir = (g->p.dir + 3) & 3; moved(g); break;
  case IN_TR: g->p.dir = (g->p.dir + 1) & 3; moved(g); break;
  case IN_ACT: act(g); break;
  case IN_SEARCH: search(g); break;
  case IN_MAP: g->big = !g->big; break;
  case IN_QUIT: return 1;
  default: break;
  }
  return 0;
}
