// game.h - play state
#ifndef CRAWL_GAME_H
#define CRAWL_GAME_H
#include "core/db.h"
#include "core/map.h"

typedef struct {
  Campaign c; char dir[512];
  Map m; Amap am; Pos p;
  int big;                      // full-screen automap
  u64 now, fade;                // ms; fade = time of last step/turn
  char msg[128]; u32 msg_rgb; u64 msg_t;
} Game;

int  play(const char *dir);     // process exit code
void game_msg(Game *, u32 rgb, const char *fmt, ...);
int  explore_in(Game *, int in); // nonzero = quit

#endif
