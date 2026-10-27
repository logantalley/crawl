// plat.h - SDL3 window, input map, main loop
#ifndef CRAWL_PLAT_H
#define CRAWL_PLAT_H
#include <SDL3/SDL.h>
#include "core/base.h"

#define VW 640                  // logical view size
#define VH 480

enum { IN_NONE, IN_FWD, IN_BACK, IN_TL, IN_TR, IN_SL, IN_SR, IN_ACT, IN_MAP, IN_SEARCH, IN_QUIT };

typedef struct { SDL_Window *win; SDL_Renderer *ren; } Plat;

typedef int  (*PlatIn)(void *ud, int in);                    // nonzero = quit
typedef void (*PlatDraw)(void *ud, SDL_Renderer *, u64 ms);

int  plat_init(Plat *, const char *title);  // 0 ok
void plat_quit(Plat *);
int  plat_key(SDL_Keycode k);               // IN_*
void plat_run(Plat *, PlatIn, PlatDraw, void *ud);

#endif
