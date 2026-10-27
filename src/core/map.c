// map.c - ASCII edge-grid maze: parse, canonical write, movement, automap
#include "map.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int dir_dx[4] = {0, 1, 0, -1};
const int dir_dy[4] = {-1, 0, 1, 0};

#define MAP_MAX 256             // max W and H

static u8 ow_swap(u8 e) {
  u8 t = (u8)ED_TYPE(e);
  if (t == ED_OWGO) return (u8)((e & 0xF0) | ED_OWNO);
  if (t == ED_OWNO) return (u8)((e & 0xF0) | ED_OWGO);
  return e;
}

u8 map_edge(const Map *m, int x, int y, int side) {
  if (side < 0 || side > 3 || !m->edge || !map_in(m, x, y)) return ED_WALL;
  return m->edge[y * m->w + x][side];
}

void map_set_edge(Map *m, int x, int y, int side, u8 e) {
  int nx, ny;
  if (side < 0 || side > 3 || !m->edge) return;
  nx = x + dir_dx[side]; ny = y + dir_dy[side];
  if (map_in(m, x, y)) m->edge[y * m->w + x][side] = e;
  if (map_in(m, nx, ny)) m->edge[ny * m->w + nx][(side + 2) & 3] = ow_swap(e);
}

const Mark *map_mark(const Map *m, int x, int y) {
  int i;
  if (!m->mark || !map_in(m, x, y)) return NULL;
  i = m->mark[y * m->w + x];
  return i > 0 && i <= m->nleg ? &m->leg[i - 1] : NULL;
}

const Zone *map_zone(const Map *m, int x, int y, const char *k) {
  int i;
  for (i = 0; i < m->nzone; i++) {
    const Zone *z = &m->zone[i];
    if (k && strcmp(z->k, k)) continue;
    if (x >= z->x && y >= z->y && x < z->x + z->w && y < z->y + z->h) return z;
  }
  return NULL;
}

const EdgeProp *map_eprop(const Map *m, int x, int y, int side) {
  int i, nx, ny, ns;
  if (side < 0 || side > 3) return NULL;
  nx = x + dir_dx[side]; ny = y + dir_dy[side]; ns = (side + 2) & 3;
  for (i = 0; i < m->neprop; i++) {
    const EdgeProp *e = &m->eprop[i];
    if ((e->x == x && e->y == y && e->side == side) || (e->x == nx && e->y == ny && e->side == ns)) return e;
  }
  return NULL;
}

void map_free(Map *m) {
  free(m->edge); free(m->cell); free(m->mark);
  m->edge = NULL; m->cell = NULL; m->mark = NULL;
}

// ---- movement ----

int map_pass(const Map *m, int x, int y, int side) {
  u8 e;
  if (side < 0 || side > 3 || !map_in(m, x, y)) return 0;
  if (!map_in(m, x + dir_dx[side], y + dir_dy[side])) return 0;
  e = map_edge(m, x, y, side);
  switch (ED_TYPE(e)) {
  case ED_OPEN: case ED_DOOR: case ED_FALSE: case ED_OWGO: return 1;
  case ED_LOCK: return (e & EF_UNLOCK) != 0;
  case ED_SECRET: return (e & EF_FOUND) != 0;
  default: return 0;
  }
}

int map_step(Map *m, Pos *p, int dir, int *bumped_secret) {
  int nx, ny;
  u8 e;
  if (bumped_secret) *bumped_secret = 0;
  if (dir < 0 || dir > 3) return MV_WALL;
  nx = p->x + dir_dx[dir]; ny = p->y + dir_dy[dir];
  if (!map_in(m, p->x, p->y) || !map_in(m, nx, ny)) return MV_WALL;
  e = map_edge(m, p->x, p->y, dir);
  switch (ED_TYPE(e)) {
  case ED_OPEN: case ED_DOOR: case ED_FALSE: case ED_OWGO: break;
  case ED_LOCK: if (!(e & EF_UNLOCK)) return MV_LOCKED; break;
  case ED_SECRET:
    if (!(e & EF_FOUND)) { if (bumped_secret) *bumped_secret = 1; return MV_WALL; }
    break;
  case ED_OWNO: return MV_ONEWAY;
  default: return MV_WALL;
  }
  if (m->cell[ny * m->w + nx] == CL_ROCK) return MV_ROCK;
  p->x = nx; p->y = ny;
  return MV_OK;
}

void map_rel(Pos p, int d, int o, int *x, int *y) {
  int f = p.dir & 3, r = (p.dir + 1) & 3;
  *x = p.x + dir_dx[f] * d + dir_dx[r] * o;
  *y = p.y + dir_dy[f] * d + dir_dy[r] * o;
}

// ---- automap ----

void amap_init(Amap *a, const Map *m) {
  size_t n = (size_t)m->w * (size_t)m->h;
  a->w = m->w; a->h = m->h;
  a->vis = (u8 *)calloc(n / 8 + 1, 1);
  a->seen = (u8 *)calloc(n * 4 / 8 + 1, 1);
  if (!a->vis || !a->seen) { free(a->vis); free(a->seen); a->vis = a->seen = NULL; a->w = a->h = 0; }
}

void amap_free(Amap *a) {
  free(a->vis); free(a->seen);
  a->vis = a->seen = NULL; a->w = a->h = 0;
}

static int bit_get(const u8 *b, size_t i) { return (b[i >> 3] >> (i & 7)) & 1; }
static void bit_set(u8 *b, size_t i) { b[i >> 3] = (u8)(b[i >> 3] | (1u << (i & 7))); }

int amap_visited(const Amap *a, int x, int y) {
  if (!a->vis || x < 0 || y < 0 || x >= a->w || y >= a->h) return 0;
  return bit_get(a->vis, (size_t)y * a->w + x);
}

int amap_seen(const Amap *a, int x, int y, int side) {
  if (!a->seen || side < 0 || side > 3 || x < 0 || y < 0 || x >= a->w || y >= a->h) return 0;
  return bit_get(a->seen, ((size_t)y * a->w + x) * 4 + side);
}

static void see_edge(Amap *a, int x, int y, int side) {
  int nx = x + dir_dx[side], ny = y + dir_dy[side];
  if (x >= 0 && y >= 0 && x < a->w && y < a->h) bit_set(a->seen, ((size_t)y * a->w + x) * 4 + side);
  if (nx >= 0 && ny >= 0 && nx < a->w && ny < a->h) bit_set(a->seen, ((size_t)ny * a->w + nx) * 4 + ((side + 2) & 3));
}

static int see_thru(u8 e) {
  int t = ED_TYPE(e);
  return t == ED_OPEN || t == ED_OWGO || t == ED_OWNO;
}

static int open_cell(const Map *m, int x, int y) {
  return map_in(m, x, y) && m->cell[y * m->w + x] != CL_ROCK;
}

// Simplified cone: a cell (d,o) is visible if it can be reached from the
// player's cell moving only forward or laterally outward (away from o=0)
// across see-through edges (open/one-way), within |o| <= 1 at d=0 and
// |o| <= 2 at d>=1, staying on non-rock cells. Every edge of a visible cell
// is marked seen; edges behind an opaque edge are not.
void amap_visit(Amap *a, const Map *m, Pos p) {
  u8 vis[4][5];
  int d, o, k, s, x, y, nx, ny;
  int fw = p.dir & 3, rt = (p.dir + 1) & 3, lt = (p.dir + 3) & 3;
  if (!a->vis || a->w != m->w || a->h != m->h || !map_in(m, p.x, p.y)) return;
  bit_set(a->vis, (size_t)p.y * a->w + p.x);
  memset(vis, 0, sizeof vis);
  vis[0][2] = 1;
  for (d = 0; d < 4; d++) {
    int maxo = d == 0 ? 1 : 2;
    for (k = 1; k <= maxo; k++) {
      for (s = -1; s <= 1; s += 2) {
        int prev = s * (k - 1);
        if (!vis[d][prev + 2]) continue;
        map_rel(p, d, prev, &x, &y);
        map_rel(p, d, s * k, &nx, &ny);
        if (see_thru(map_edge(m, x, y, s > 0 ? rt : lt)) && open_cell(m, nx, ny)) vis[d][s * k + 2] = 1;
      }
    }
    for (o = -maxo; o <= maxo; o++) {
      int side;
      if (!vis[d][o + 2]) continue;
      map_rel(p, d, o, &x, &y);
      for (side = 0; side < 4; side++) see_edge(a, x, y, side);
      if (d < 3 && see_thru(map_edge(m, x, y, fw))) {
        map_rel(p, d + 1, o, &nx, &ny);
        if (open_cell(m, nx, ny)) vis[d + 1][o + 2] = 1;
      }
    }
  }
}

// ---- parse ----

static const char *glyph_show(int c, char *buf) {   // buf >= 8
  if (c > 32 && c < 127) { buf[0] = (char)c; buf[1] = 0; }
  else if (c == ' ') memcpy(buf, "space", 6);
  else snprintf(buf, 8, "\\x%02X", (unsigned)(c & 0xFF));
  return buf;
}

static int glyph_ok(int c) {     // legal marker glyph
  return c > 32 && c < 127 && c != '.' && c != '#' && c != '+' && c != '"' && c != '\\' && c != '\'';
}

static int get_str(toml_datum_t t, const char *key, char *out, size_t n, int req, int line, Err *err) {
  toml_datum_t v = toml_get(t, key);
  int rc;
  if (v.type == TOML_UNKNOWN) {
    if (req) { err_set(err, line, "missing '%s'", key); return -1; }
    out[0] = 0;
    return 0;
  }
  rc = db_str(t, key, out, n);
  if (rc == 1) { err_set(err, v.lineno, "'%s' must be a string", key); return -1; }
  if (rc == 2) { err_set(err, v.lineno, "'%s' is too long (max %d chars)", key, (int)n - 1); return -1; }
  return 0;
}

// array of exactly n integers in [-100000, 100000]
static int int_arr(toml_datum_t a, int n, int *out) {
  int i;
  if (a.type != TOML_ARRAY || a.u.arr.size != n) return -1;
  for (i = 0; i < n; i++) {
    toml_datum_t e = a.u.arr.elem[i];
    if (e.type != TOML_INT64 || e.u.int64 < -100000 || e.u.int64 > 100000) return -1;
    out[i] = (int)e.u.int64;
  }
  return 0;
}

static int leg_find(const Map *m, int g) {
  int i;
  for (i = 0; i < m->nleg; i++) if ((unsigned char)m->leg[i].g == (unsigned)g) return i;
  return -1;
}

static int parse_legend(Map *m, toml_datum_t t, Err *err) {
  toml_datum_t lg = toml_get(t, "legend");
  int i;
  if (lg.type == TOML_UNKNOWN) return 0;
  if (lg.type != TOML_TABLE) { err_set(err, lg.lineno, "[legend] must be a table"); return -1; }
  for (i = 0; i < lg.u.tab.size; i++) {
    const char *k = lg.u.tab.key[i];
    toml_datum_t v = lg.u.tab.value[i];
    Mark *mk;
    if (lg.u.tab.len[i] != 1 || !glyph_ok((unsigned char)k[0])) {
      err_set(err, v.lineno, "legend key '%s' must be one printable char other than . # + \" ' \\", k);
      return -1;
    }
    if (v.type != TOML_TABLE) { err_set(err, v.lineno, "legend '%s' must be { ev = \"...\", look = \"...\" }", k); return -1; }
    if (m->nleg >= NELEM(m->leg)) { err_set(err, v.lineno, "too many legend entries (max %d)", NELEM(m->leg)); return -1; }
    mk = &m->leg[m->nleg++];
    mk->g = k[0];
    if (get_str(v, "ev", mk->ev, sizeof mk->ev, 0, v.lineno, err)) return -1;
    if (get_str(v, "look", mk->look, sizeof mk->look, 0, v.lineno, err)) return -1;
  }
  return 0;
}

// edge glyph -> type for the upper/left cell's DS/DE side; -1 if bad
static int edge_type(int c, int horiz) {
  switch (c) {
  case '-': case '|': return ED_WALL;
  case ' ': return ED_OPEN;
  case 'd': return ED_DOOR;
  case 'l': return ED_LOCK;
  case 's': return ED_SECRET;
  case 'f': return ED_FALSE;
  case 'v': return horiz ? ED_OWGO : -1;
  case '^': return horiz ? ED_OWNO : -1;
  case '>': return horiz ? -1 : ED_OWGO;
  case '<': return horiz ? -1 : ED_OWNO;
  default: return -1;
  }
}

static int parse_grid(Map *m, const char *s, int len, int line0, Err *err) {
  char *b = (char *)malloc((size_t)len + 1);
  int n = 0, i, nr = 0, w0 = -1, W, H, x, y, rc = -1;
  int *rs = NULL, *rl = NULL;
  char gb[8];
  if (!b) { err_set(err, line0, "out of memory"); return -1; }
  for (i = 0; i < len; i++) if (s[i] != '\r') b[n++] = s[i];
  b[n] = 0;
  i = 0;
  if (n && b[0] == '\n') { i = 1; line0++; }      // untrimmed leading newline
  if (n > i && b[n - 1] == '\n') b[--n] = 0;
  // count and index rows
  { int j; nr = 1; for (j = i; j < n; j++) nr += b[j] == '\n'; }
  rs = (int *)malloc(sizeof(int) * (size_t)nr);
  rl = (int *)malloc(sizeof(int) * (size_t)nr);
  if (!rs || !rl) { err_set(err, line0, "out of memory"); goto out; }
  { int r = 0, st = i, j;
    for (j = i; j <= n; j++) if (j == n || b[j] == '\n') { rs[r] = st; rl[r] = j - st; r++; st = j + 1; } }
  if (nr < 3 || nr % 2 == 0) { err_set(err, line0, "grid must have 2H+1 rows (odd, >= 3); got %d", nr); goto out; }
  w0 = rl[0];
  if (w0 < 3 || w0 % 2 == 0) { err_set(err, line0, "grid rows must be 2W+1 chars (odd, >= 3); got %d", w0); goto out; }
  for (i = 1; i < nr; i++)
    if (rl[i] != w0) { err_set(err, line0 + i, "grid row is %d chars, expected %d (grid must be rectangular)", rl[i], w0); goto out; }
  W = (w0 - 1) / 2; H = (nr - 1) / 2;
  if (W > MAP_MAX || H > MAP_MAX) { err_set(err, line0, "map too big (max %dx%d)", MAP_MAX, MAP_MAX); goto out; }
  m->w = W; m->h = H;
  m->edge = (u8(*)[4])calloc((size_t)W * H, 4);
  m->cell = (u8 *)calloc((size_t)W * H, 1);
  m->mark = (u8 *)calloc((size_t)W * H, 1);
  if (!m->edge || !m->cell || !m->mark) { err_set(err, line0, "out of memory"); goto out; }
  for (y = 0; y < nr; y++) {
    const char *row = b + rs[y];
    for (x = 0; x < w0; x++) {
      int c = (unsigned char)row[x], t;
      if (y % 2 == 1 && x % 2 == 1) {            // cell
        int cx = x / 2, cy = y / 2, k = cy * W + cx;
        if (c == '.') m->cell[k] = CL_FLOOR;
        else if (c == '#') m->cell[k] = CL_ROCK;
        else if ((t = leg_find(m, c)) >= 0) { m->cell[k] = CL_FLOOR; m->mark[k] = (u8)(t + 1); }
        else {
          err_set(err, line0 + y, "cell (%d,%d): glyph '%s' is not '.', '#' or in [legend]", cx, cy, glyph_show(c, gb));
          goto out;
        }
      } else if (y % 2 == 0 && x % 2 == 1) {     // horizontal edge above cell (x/2, y/2)
        if ((t = edge_type(c, 1)) < 0) {
          err_set(err, line0 + y, "column %d: bad horizontal edge glyph '%s'", x + 1, glyph_show(c, gb));
          goto out;
        }
        map_set_edge(m, x / 2, y / 2 - 1, DS, (u8)t);
      } else if (y % 2 == 1 && x % 2 == 0) {     // vertical edge left of cell (x/2, y/2)
        if ((t = edge_type(c, 0)) < 0) {
          err_set(err, line0 + y, "column %d: bad vertical edge glyph '%s'", x + 1, glyph_show(c, gb));
          goto out;
        }
        map_set_edge(m, x / 2 - 1, y / 2, DE, (u8)t);
      }                                           // corners ignored
    }
  }
  rc = 0;
out:
  free(rs); free(rl); free(b);
  return rc;
}

static int parse_eprops(Map *m, toml_datum_t t, Err *err) {
  toml_datum_t a = toml_get(t, "edge");
  int i;
  if (a.type == TOML_UNKNOWN) return 0;
  if (a.type != TOML_ARRAY) { err_set(err, a.lineno, "'edge' must be [[edge]] tables"); return -1; }
  for (i = 0; i < a.u.arr.size; i++) {
    toml_datum_t e = a.u.arr.elem[i];
    EdgeProp *ep;
    int at[2], ty, fl;
    char side[8];
    if (e.type != TOML_TABLE) { err_set(err, e.lineno ? e.lineno : a.lineno, "'edge' must be [[edge]] tables"); return -1; }
    if (m->neprop >= NELEM(m->eprop)) { err_set(err, e.lineno, "too many [[edge]] (max %d)", NELEM(m->eprop)); return -1; }
    ep = &m->eprop[m->neprop++];
    fl = toml_get(e, "at").lineno;
    if (!fl) fl = e.lineno;
    if (int_arr(toml_get(e, "at"), 2, at)) { err_set(err, fl, "[[edge]] needs at = [x, y]"); return -1; }
    if (!map_in(m, at[0], at[1])) { err_set(err, fl, "[[edge]] at = [%d, %d] is outside the map", at[0], at[1]); return -1; }
    if (get_str(e, "side", side, sizeof side, 1, e.lineno, err)) return -1;
    if ((ep->side = dir_parse(side)) < 0) { err_set(err, fl, "[[edge]] side must be \"n\", \"e\", \"s\" or \"w\""); return -1; }
    ep->x = at[0]; ep->y = at[1];
    if (get_str(e, "key", ep->key, sizeof ep->key, 0, e.lineno, err)) return -1;
    if (ep->key[0]) {
      ty = ED_TYPE(map_edge(m, ep->x, ep->y, ep->side));
      if (ty == ED_DOOR) map_set_edge(m, ep->x, ep->y, ep->side, ED_LOCK);  // keyed door = locked
      else if (ty != ED_LOCK) {
        err_set(err, toml_get(e, "key").lineno, "[[edge]] at [%d, %d] side %s has a key but is not a door ('l' or 'd')", ep->x, ep->y, side);
        return -1;
      }
    }
  }
  return 0;
}

static int parse_zones(Map *m, toml_datum_t t, Err *err) {
  toml_datum_t a = toml_get(t, "zone");
  int i;
  if (a.type == TOML_UNKNOWN) return 0;
  if (a.type != TOML_ARRAY) { err_set(err, a.lineno, "'zone' must be [[zone]] tables"); return -1; }
  for (i = 0; i < a.u.arr.size; i++) {
    toml_datum_t e = a.u.arr.elem[i];
    Zone *z;
    int r[4], fl;
    if (e.type != TOML_TABLE) { err_set(err, e.lineno ? e.lineno : a.lineno, "'zone' must be [[zone]] tables"); return -1; }
    if (m->nzone >= NELEM(m->zone)) { err_set(err, e.lineno, "too many [[zone]] (max %d)", NELEM(m->zone)); return -1; }
    z = &m->zone[m->nzone++];
    if (get_str(e, "k", z->k, sizeof z->k, 1, e.lineno, err)) return -1;
    fl = toml_get(e, "rect").lineno;
    if (!fl) fl = e.lineno;
    if (int_arr(toml_get(e, "rect"), 4, r)) { err_set(err, fl, "[[zone]] needs rect = [x, y, w, h]"); return -1; }
    if (r[0] < 0 || r[1] < 0 || r[2] < 1 || r[3] < 1 || r[0] + r[2] > m->w || r[1] + r[3] > m->h) {
      err_set(err, fl, "[[zone]] rect [%d, %d, %d, %d] is outside the %dx%d map", r[0], r[1], r[2], r[3], m->w, m->h);
      return -1;
    }
    z->x = r[0]; z->y = r[1]; z->w = r[2]; z->h = r[3];
  }
  return 0;
}

static int parse_foes(Map *m, toml_datum_t t, Err *err) {
  toml_datum_t a = toml_get(t, "foe");
  int i, j;
  if (a.type == TOML_UNKNOWN) return 0;
  if (a.type != TOML_ARRAY) { err_set(err, a.lineno, "'foe' must be [[foe]] tables"); return -1; }
  for (i = 0; i < a.u.arr.size; i++) {
    toml_datum_t e = a.u.arr.elem[i], p;
    Foe *f;
    if (e.type != TOML_TABLE) { err_set(err, e.lineno ? e.lineno : a.lineno, "'foe' must be [[foe]] tables"); return -1; }
    if (m->nfoe >= NELEM(m->foe)) { err_set(err, e.lineno, "too many [[foe]] (max %d)", NELEM(m->foe)); return -1; }
    f = &m->foe[m->nfoe++];
    if (get_str(e, "id", f->id, sizeof f->id, 1, e.lineno, err)) return -1;
    if (get_str(e, "grp", f->grp, sizeof f->grp, 0, e.lineno, err)) return -1;
    if (get_str(e, "look", f->look, sizeof f->look, 0, e.lineno, err)) return -1;
    if (get_str(e, "ai", f->ai, sizeof f->ai, 0, e.lineno, err)) return -1;
    p = toml_get(e, "path");
    if (p.type == TOML_UNKNOWN) continue;
    if (p.type != TOML_ARRAY) { err_set(err, p.lineno, "[[foe]] path must be [[x, y], ...]"); return -1; }
    if (p.u.arr.size > FOE_PATH) { err_set(err, p.lineno, "[[foe]] path too long (max %d)", FOE_PATH); return -1; }
    for (j = 0; j < p.u.arr.size; j++) {
      int xy[2];
      if (int_arr(p.u.arr.elem[j], 2, xy)) { err_set(err, p.lineno, "[[foe]] path must be [[x, y], ...]"); return -1; }
      if (!map_in(m, xy[0], xy[1])) { err_set(err, p.lineno, "[[foe]] '%s' path point [%d, %d] is outside the map", f->id, xy[0], xy[1]); return -1; }
      f->path[j][0] = (i16)xy[0]; f->path[j][1] = (i16)xy[1];
    }
    f->n = p.u.arr.size;
  }
  return 0;
}

static int parse_body(Map *m, toml_datum_t t, Err *err) {
  toml_datum_t v;
  char grid[16];
  if (get_str(t, "id", m->id, sizeof m->id, 0, 0, err)) return -1;
  if (get_str(t, "name", m->name, sizeof m->name, 0, 0, err)) return -1;
  if (get_str(t, "tiles", m->tiles, sizeof m->tiles, 0, 0, err)) return -1;
  if (get_str(t, "kind", m->kind, sizeof m->kind, 0, 0, err)) return -1;
  if (get_str(t, "grid", grid, sizeof grid, 0, 0, err)) return -1;
  if (grid[0] && strcmp(grid, "edge")) { err_set(err, toml_get(t, "grid").lineno, "grid must be \"edge\""); return -1; }
  v = toml_get(t, "enc");
  if (v.type != TOML_UNKNOWN) {
    i64 rate = 0;
    toml_datum_t rv;
    if (v.type != TOML_TABLE) { err_set(err, v.lineno, "enc must be { rate = N, table = \"...\" }"); return -1; }
    rv = toml_get(v, "rate");
    if (rv.type != TOML_UNKNOWN && (db_int(v, "rate", &rate) || rate < 0 || rate > 100)) {
      err_set(err, v.lineno, "enc.rate must be an integer 0..100");
      return -1;
    }
    m->enc_rate = (int)rate;
    if (get_str(v, "table", m->enc_tbl, sizeof m->enc_tbl, 0, v.lineno, err)) return -1;
  }
  if (parse_legend(m, t, err)) return -1;
  v = toml_get(t, "map");
  if (v.type != TOML_STRING) { err_set(err, v.lineno, "%s", v.type == TOML_UNKNOWN ? "missing 'map' grid" : "'map' must be a string"); return -1; }
  if (parse_grid(m, v.u.str.ptr, v.u.str.len, v.lineno, err)) return -1;
  if (parse_eprops(m, t, err)) return -1;
  if (parse_zones(m, t, err)) return -1;
  if (parse_foes(m, t, err)) return -1;
  return 0;
}

int map_parse(Map *m, const char *src, size_t n, Err *err) {
  toml_result_t r;
  int rc;
  memset(m, 0, sizeof *m);
  if (db_parse(&r, src, n, NULL, err)) return -1;
  rc = parse_body(m, r.toptab, err);
  toml_free(r);
  if (rc) { map_free(m); memset(m, 0, sizeof *m); }
  return rc;
}

int map_load(Map *m, const char *path, Err *err) {
  size_t n;
  char *s = file_read(path, &n);
  int rc;
  if (!s) {
    memset(m, 0, sizeof *m);
    err_set(err, 0, "cannot read file");
    return -1;
  }
  rc = map_parse(m, s, n, err);
  free(s);
  return rc;
}

// ---- canonical write ----

typedef struct { char *p; size_t n, cap; int bad; } Buf;

static void bput(Buf *b, const char *s, size_t n) {
  if (b->bad) return;
  if (b->cap - b->n < n + 1) {
    size_t nc = b->cap ? b->cap : 1024;
    char *np;
    while (nc - b->n < n + 1) nc *= 2;
    np = (char *)realloc(b->p, nc);
    if (!np) { b->bad = 1; return; }
    b->p = np; b->cap = nc;
  }
  memcpy(b->p + b->n, s, n);
  b->n += n;
  b->p[b->n] = 0;
}
static void bs(Buf *b, const char *s) { bput(b, s, strlen(s)); }
static void bc(Buf *b, char c) { bput(b, &c, 1); }
static void bint(Buf *b, int v) { char t[16]; snprintf(t, sizeof t, "%d", v); bs(b, t); }
static void bpad(Buf *b, int n) { while (n-- > 0) bc(b, ' '); }

// escaped length of a basic string, quotes included
static int qlen(const char *s) {
  int n = 2;
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\' || c == '\n' || c == '\t' || c == '\r') n += 2;
    else if (c < 0x20 || c == 0x7f) n += 6;
    else n++;
  }
  return n;
}

static void bq(Buf *b, const char *s) {
  bc(b, '"');
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    char t[8];
    if (c == '"') bs(b, "\\\"");
    else if (c == '\\') bs(b, "\\\\");
    else if (c == '\n') bs(b, "\\n");
    else if (c == '\t') bs(b, "\\t");
    else if (c == '\r') bs(b, "\\r");
    else if (c < 0x20 || c == 0x7f) { snprintf(t, sizeof t, "\\u%04X", (unsigned)c); bs(b, t); }
    else bc(b, (char)c);
  }
  bc(b, '"');
}

static void bkv(Buf *b, const char *k, int w, const char *v) {  // `k<pad> = "v"\n`
  bs(b, k); bpad(b, w - (int)strlen(k)); bs(b, " = "); bq(b, v); bc(b, '\n');
}

static int bare_ok(int c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

// glyph for the horizontal edge above cell (x,y) / vertical edge left of cell (x,y)
static char hglyph(const Map *m, int x, int y) {
  u8 e = map_in(m, x, y - 1) ? map_edge(m, x, y - 1, DS) : ow_swap(map_edge(m, x, y, DN));
  switch (ED_TYPE(e)) {
  case ED_OPEN: return ' ';
  case ED_DOOR: return 'd';
  case ED_LOCK: return 'l';
  case ED_SECRET: return 's';
  case ED_FALSE: return 'f';
  case ED_OWGO: return 'v';
  case ED_OWNO: return '^';
  default: return '-';
  }
}

static char vglyph(const Map *m, int x, int y) {
  u8 e = map_in(m, x - 1, y) ? map_edge(m, x - 1, y, DE) : ow_swap(map_edge(m, x, y, DW));
  switch (ED_TYPE(e)) {
  case ED_OPEN: return ' ';
  case ED_DOOR: return 'd';
  case ED_LOCK: return 'l';
  case ED_SECRET: return 's';
  case ED_FALSE: return 'f';
  case ED_OWGO: return '>';
  case ED_OWNO: return '<';
  default: return '|';
  }
}

static char cglyph(const Map *m, int x, int y) {
  const Mark *mk = map_mark(m, x, y);
  if (mk) return mk->g;
  return m->cell[y * m->w + x] == CL_ROCK ? '#' : '.';
}

char *map_write(const Map *m, size_t *n) {
  Buf b = {0};
  int x, y, i, j, ord[64], maxk = 0, maxe = 0;
  static const char sides[] = "nesw";
  if (n) *n = 0;
  bkv(&b, "id", 5, m->id);
  bkv(&b, "name", 5, m->name);
  bkv(&b, "grid", 5, "edge");
  bkv(&b, "tiles", 5, m->tiles);
  bkv(&b, "kind", 5, m->kind);
  if (m->enc_rate > 0) {
    bs(&b, "enc   = { rate = "); bint(&b, m->enc_rate);
    bs(&b, ", table = "); bq(&b, m->enc_tbl); bs(&b, " }\n");
  }
  bs(&b, "map = \"\"\"\n");
  if (m->cell && m->edge) {
    for (y = 0; y <= m->h; y++) {
      for (x = 0; x < m->w; x++) { bc(&b, '+'); bc(&b, hglyph(m, x, y)); }
      bs(&b, "+\n");
      if (y == m->h) break;
      for (x = 0; x <= m->w; x++) {
        bc(&b, vglyph(m, x, y));
        if (x < m->w) bc(&b, cglyph(m, x, y));
      }
      bc(&b, '\n');
    }
  }
  bs(&b, "\"\"\"\n");
  if (m->nleg > 0) {
    int nl = m->nleg > 64 ? 64 : m->nleg;
    for (i = 0; i < nl; i++) ord[i] = i;
    for (i = 1; i < nl; i++)          // insertion sort by glyph
      for (j = i; j > 0 && (unsigned char)m->leg[ord[j - 1]].g > (unsigned char)m->leg[ord[j]].g; j--) {
        int t = ord[j]; ord[j] = ord[j - 1]; ord[j - 1] = t;
      }
    for (i = 0; i < nl; i++) {
      const Mark *mk = &m->leg[i];
      int kw = bare_ok((unsigned char)mk->g) ? 1 : 3, el = qlen(mk->ev);
      if (kw > maxk) maxk = kw;
      if (el > maxe) maxe = el;
    }
    bs(&b, "\n[legend]\n");
    for (i = 0; i < nl; i++) {
      const Mark *mk = &m->leg[ord[i]];
      int kw = bare_ok((unsigned char)mk->g) ? 1 : 3;
      if (kw == 1) bc(&b, mk->g);
      else { bc(&b, '"'); bc(&b, mk->g); bc(&b, '"'); }
      bpad(&b, maxk - kw);
      bs(&b, " = { ev = "); bq(&b, mk->ev); bc(&b, ',');
      bpad(&b, 1 + maxe - qlen(mk->ev));
      bs(&b, "look = "); bq(&b, mk->look); bs(&b, " }\n");
    }
  }
  for (i = 0; i < m->neprop; i++) {
    const EdgeProp *e = &m->eprop[i];
    char sd[2];
    sd[0] = sides[e->side & 3]; sd[1] = 0;
    bs(&b, "\n[[edge]]\nat = ["); bint(&b, e->x); bs(&b, ", "); bint(&b, e->y);
    bs(&b, "]; side = "); bq(&b, sd);
    if (e->key[0]) { bs(&b, "; key = "); bq(&b, e->key); }
    bc(&b, '\n');
  }
  for (i = 0; i < m->nzone; i++) {
    const Zone *z = &m->zone[i];
    bs(&b, "\n[[zone]]\nk = "); bq(&b, z->k);
    bs(&b, "; rect = ["); bint(&b, z->x); bs(&b, ", "); bint(&b, z->y);
    bs(&b, ", "); bint(&b, z->w); bs(&b, ", "); bint(&b, z->h); bs(&b, "]\n");
  }
  for (i = 0; i < m->nfoe; i++) {
    const Foe *f = &m->foe[i];
    bs(&b, "\n[[foe]]\nid = "); bq(&b, f->id);
    bs(&b, "; grp = "); bq(&b, f->grp);
    bs(&b, "; look = "); bq(&b, f->look);
    bs(&b, "\nai = "); bq(&b, f->ai);
    bs(&b, "; path = [");
    for (j = 0; j < f->n && j < FOE_PATH; j++) {
      if (j) bs(&b, ", ");
      bc(&b, '['); bint(&b, f->path[j][0]); bs(&b, ", "); bint(&b, f->path[j][1]); bc(&b, ']');
    }
    bs(&b, "]\n");
  }
  if (b.bad) { free(b.p); return NULL; }
  if (n) *n = b.n;
  return b.p;
}
