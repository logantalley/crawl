// check.c - campaign validation: campaign header, maps, start, reachability
#include "check.h"
#include "db.h"
#include "map.h"
#include <stdlib.h>
#include <string.h>

typedef struct { FILE *out; int err, warn; } Ck;

static void say(Ck *k, const char *path, int line, int is_err, const char *msg) {
  if (is_err) k->err++; else k->warn++;
  if (!k->out) return;
  if (line > 0) fprintf(k->out, "%s:%d: %s: %s\n", path, line, is_err ? "error" : "warning", msg);
  else fprintf(k->out, "%s: %s: %s\n", path, is_err ? "error" : "warning", msg);
}

// can a party walk from (x,y) through side? locked/secret/false count as passable
static int reach_pass(const Map *m, int x, int y, int side) {
  int nx = x + dir_dx[side], ny = y + dir_dy[side], t;
  if (!map_in(m, nx, ny) || m->cell[ny * m->w + nx] == CL_ROCK) return 0;
  t = ED_TYPE(map_edge(m, x, y, side));
  return t != ED_WALL && t != ED_OWNO;
}

static void check_reach(Ck *k, const char *path, const Map *m, int sx, int sy) {
  size_t n = (size_t)m->w * (size_t)m->h;
  int *q = (int *)malloc(sizeof(int) * n);
  u8 *seen = (u8 *)calloc(n, 1);
  int qh = 0, qt = 0, x, y, s;
  char msg[160];
  if (!q || !seen) { free(q); free(seen); say(k, path, 0, 1, "out of memory"); return; }
  seen[sy * m->w + sx] = 1;
  q[qt++] = sy * m->w + sx;
  while (qh < qt) {
    int c = q[qh++];
    x = c % m->w; y = c / m->w;
    for (s = 0; s < 4; s++) {
      int nc = (y + dir_dy[s]) * m->w + (x + dir_dx[s]);
      if (reach_pass(m, x, y, s) && !seen[nc]) { seen[nc] = 1; q[qt++] = nc; }
    }
  }
  for (y = 0; y < m->h; y++)
    for (x = 0; x < m->w; x++) {
      const Mark *mk = map_mark(m, x, y);
      if (mk && !seen[y * m->w + x]) {
        snprintf(msg, sizeof msg, "marker '%c' (ev \"%s\") at (%d,%d) is unreachable from start", mk->g, mk->ev, x, y);
        say(k, path, 0, 0, msg);
      }
    }
  free(q); free(seen);
}

int check_campaign(const char *dir, FILE *out) {
  Ck k;
  Campaign c;
  Err e;
  Map *maps = NULL;
  u8 *ok = NULL;
  char cpath[512], mpath[512], msg[256];
  int i, j, si = -1;
  k.out = out; k.err = 0; k.warn = 0;
  memset(&e, 0, sizeof e);
  snprintf(cpath, sizeof cpath, "%s/campaign.toml", dir);
  if (db_campaign(&c, dir, &e)) {
    say(&k, cpath, e.line, 1, e.msg);
    goto done;
  }
  if (c.nmaps == 0) say(&k, cpath, 0, 1, "campaign lists no maps");
  maps = (Map *)calloc((size_t)(c.nmaps ? c.nmaps : 1), sizeof(Map));
  ok = (u8 *)calloc((size_t)(c.nmaps ? c.nmaps : 1), 1);
  if (!maps || !ok) { say(&k, cpath, 0, 1, "out of memory"); goto done; }
  for (i = 0; i < c.nmaps; i++) {
    Map *m = &maps[i];
    if (snprintf(mpath, sizeof mpath, "%s/%s", dir, c.maps[i]) >= (int)sizeof mpath) {
      say(&k, cpath, 0, 1, "map path too long");
      continue;
    }
    memset(&e, 0, sizeof e);
    if (map_load(m, mpath, &e)) { say(&k, mpath, e.line, 1, e.msg); continue; }
    ok[i] = 1;
    if (!m->id[0]) say(&k, mpath, 0, 1, "map has no id");
    for (j = 0; j < i; j++)
      if (ok[j] && m->id[0] && !strcmp(maps[j].id, m->id)) {
        snprintf(msg, sizeof msg, "duplicate map id \"%s\" (also in %s)", m->id, c.maps[j]);
        say(&k, mpath, 0, 1, msg);
      }
    for (j = 0; j < m->nleg; j++)
      if (!m->leg[j].ev[0]) {
        snprintf(msg, sizeof msg, "legend '%c' has an empty ev", m->leg[j].g);
        say(&k, mpath, 0, 1, msg);
      }
    if (si < 0 && !strcmp(m->id, c.start_map)) si = i;
  }
  if (si < 0) {
    snprintf(msg, sizeof msg, "start map \"%s\" is not among the loaded maps", c.start_map);
    say(&k, cpath, 0, 1, msg);
  } else {
    Map *m = &maps[si];
    snprintf(mpath, sizeof mpath, "%s/%s", dir, c.maps[si]);
    if (!map_in(m, c.start_x, c.start_y)) {
      snprintf(msg, sizeof msg, "start [%d, %d] is outside map \"%s\" (%dx%d)", c.start_x, c.start_y, m->id, m->w, m->h);
      say(&k, cpath, 0, 1, msg);
    } else if (m->cell[c.start_y * m->w + c.start_x] == CL_ROCK) {
      snprintf(msg, sizeof msg, "start [%d, %d] on map \"%s\" is rock", c.start_x, c.start_y, m->id);
      say(&k, cpath, 0, 1, msg);
    } else {
      check_reach(&k, mpath, m, c.start_x, c.start_y);
    }
  }
  for (i = 0; i < c.nmaps; i++)
    if (ok[i] && i != si) {
      snprintf(mpath, sizeof mpath, "%s/%s", dir, c.maps[i]);
      say(&k, mpath, 0, 0, "reachability not checked: no entry point until stairs/warp events are validated");
    }
  say(&k, cpath, 0, 0, "events, dialogue, art and licenses are not validated yet");
done:
  if (maps) for (i = 0; i < c.nmaps; i++) map_free(&maps[i]);
  free(maps); free(ok);
  if (out) fprintf(out, "check: %d error(s), %d warning(s)\n", k.err, k.warn);
  return k.err;
}
