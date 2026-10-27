// sdl.c - SDL3 init, input map, main loop
#include "plat/plat.h"

int plat_init(Plat *p, const char *title) {
  p->win = NULL; p->ren = NULL;
  if (!SDL_Init(SDL_INIT_VIDEO)) { SDL_Log("SDL_Init: %s", SDL_GetError()); return -1; }
  if (!SDL_CreateWindowAndRenderer(title, 1280, 960, SDL_WINDOW_RESIZABLE, &p->win, &p->ren)) {
    SDL_Log("window: %s", SDL_GetError()); SDL_Quit(); return -1;
  }
  SDL_SetRenderLogicalPresentation(p->ren, VW, VH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
  SDL_SetRenderVSync(p->ren, 1);
  SDL_SetRenderDrawBlendMode(p->ren, SDL_BLENDMODE_BLEND);
  return 0;
}

void plat_quit(Plat *p) {
  if (p->ren) SDL_DestroyRenderer(p->ren);
  if (p->win) SDL_DestroyWindow(p->win);
  p->ren = NULL; p->win = NULL;
  SDL_Quit();
}

int plat_key(SDL_Keycode k) {
  switch (k) {
  case SDLK_UP: case SDLK_W: return IN_FWD;
  case SDLK_DOWN: case SDLK_S: return IN_BACK;
  case SDLK_LEFT: case SDLK_A: return IN_TL;
  case SDLK_RIGHT: case SDLK_D: return IN_TR;
  case SDLK_Q: return IN_SL;
  case SDLK_E: return IN_SR;
  case SDLK_SPACE: case SDLK_RETURN: case SDLK_KP_ENTER: return IN_ACT;
  case SDLK_M: return IN_MAP;
  case SDLK_F: return IN_SEARCH;
  case SDLK_ESCAPE: return IN_QUIT;
  default: return IN_NONE;
  }
}

void plat_run(Plat *p, PlatIn on_in, PlatDraw draw, void *ud) {
  const u64 frame_ns = 1000000000u / 60;
  int run = 1;
  while (run) {
    u64 t0 = SDL_GetTicksNS();
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_EVENT_QUIT) run = 0;
      else if (ev.type == SDL_EVENT_KEY_DOWN) {
        int in = plat_key(ev.key.key);
        if (in == IN_NONE) continue;
        // held keys repeat movement only
        if (ev.key.repeat && in >= IN_ACT) continue;
        if (in == IN_QUIT || on_in(ud, in)) run = 0;
      }
    }
    if (!run) break;
    draw(ud, p->ren, SDL_GetTicks());
    SDL_RenderPresent(p->ren);
    // pace if vsync is off or unavailable
    u64 dt = SDL_GetTicksNS() - t0;
    if (dt < frame_ns) SDL_DelayNS(frame_ns - dt);
  }
}
