// main.c - subcommand dispatch
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "core/check.h"
#include "game/game.h"

static int usage(FILE *f) {
  fputs("usage: crawl <cmd> [args]\n"
        "  play <dir>      play a campaign\n"
        "  check <dir>     validate a campaign\n"
#ifndef CRAWL_NO_EDITOR
        "  edit <dir>      editor (M4)\n"
#endif
        "  pack <dir> ...  package a game (M5)\n"
        "  slices <dir>    generate starter slices (M2)\n", f);
  return f == stdout ? 0 : 2;
}

static int todo(const char *cmd, const char *m) {
  fprintf(stderr, "crawl %s: not implemented (%s)\n", cmd, m);
  return 2;
}

int main(int argc, char *argv[]) {
  const char *cmd = argc > 1 ? argv[1] : NULL, *dir = argc > 2 ? argv[2] : ".";
#ifdef CRAWL_NO_EDITOR
  if (!cmd) return play(".");   // TODO M5: appended pak / game.pak lookup
#endif
  if (!cmd) return usage(stderr);
  if (!strcmp(cmd, "help") || !strcmp(cmd, "-h") || !strcmp(cmd, "--help")) return usage(stdout);
  if (!strcmp(cmd, "check")) return check_campaign(dir, stdout) ? 1 : 0;
  if (!strcmp(cmd, "play")) return play(dir);
#ifndef CRAWL_NO_EDITOR
  if (!strcmp(cmd, "edit")) return todo(cmd, "M4");
#endif
  if (!strcmp(cmd, "pack")) return todo(cmd, "M5");
  if (!strcmp(cmd, "slices")) return todo(cmd, "M2");
  fprintf(stderr, "crawl: unknown command '%s'\n", cmd);
  return usage(stderr);
}
