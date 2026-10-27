// map.h - ASCII edge-grid maze: parse, canonical write, movement, auto-map memory
#ifndef CRAWL_MAP_H
#define CRAWL_MAP_H
#include "base.h"

enum { DN, DE, DS, DW };        // facing/side; (d+2)&3 is opposite
extern const int dir_dx[4], dir_dy[4];

// edge byte: low nibble = type, high nibble = runtime flags
enum {
  ED_OPEN, ED_WALL, ED_DOOR, ED_LOCK, ED_SECRET, ED_FALSE,
  ED_OWGO,                      // one-way: may leave this cell through this side
  ED_OWNO,                      // one-way: may not (neighbor's side is ED_OWGO)
};
#define ED_TYPE(e)  ((e) & 15)
#define EF_FOUND    0x10        // secret door discovered
#define EF_UNLOCK   0x20        // locked door opened

enum { CL_FLOOR, CL_ROCK };

typedef struct { char g; Id ev, look; } Mark;              // [legend] glyph -> marker
typedef struct { Id k; int x, y, w, h; } Zone;             // [[zone]]
typedef struct { int x, y, side; Id key; } EdgeProp;       // [[edge]]
#define FOE_PATH 32
typedef struct { Id id, grp, look, ai; int n; i16 path[FOE_PATH][2]; } Foe;

typedef struct Map {
  Id id, tiles, kind, enc_tbl; char name[64]; int enc_rate;
  int w, h;
  u8  (*edge)[4];               // [y*w+x][side]; both cells sharing an edge store it
  u8  *cell;                    // CL_*
  u8  *mark;                    // index+1 into leg, 0 = none
  Mark leg[64]; int nleg;
  Zone zone[32]; int nzone;
  EdgeProp eprop[64]; int neprop;
  Foe foe[32]; int nfoe;
} Map;

int   map_parse(Map *, const char *src, size_t n, Err *);  // 0 ok; src is map TOML
int   map_load(Map *, const char *path, Err *);
char *map_write(const Map *, size_t *n);  // canonical TOML, malloc'd; write(parse(write(m))) == write(m)
void  map_free(Map *);

static inline int map_in(const Map *m, int x, int y) { return x >= 0 && y >= 0 && x < m->w && y < m->h; }
u8   map_edge(const Map *, int x, int y, int side);        // ED_WALL outside the map
void map_set_edge(Map *, int x, int y, int side, u8 e);    // writes both sides (OWGO pairs with OWNO)
const Mark *map_mark(const Map *, int x, int y);           // NULL if none
const Zone *map_zone(const Map *, int x, int y, const char *k); // first zone of kind k covering (x,y)
const EdgeProp *map_eprop(const Map *, int x, int y, int side);  // matches either side of the edge

typedef struct { int x, y, dir; } Pos;
// result of trying to walk forward/back/strafe
enum { MV_OK, MV_WALL, MV_LOCKED, MV_ONEWAY, MV_ROCK };
int  map_step(Map *, Pos *, int dir, int *bumped_secret); // move one cell toward absolute dir; updates *pos on MV_OK
int  map_pass(const Map *, int x, int y, int side);        // 1 if crossing is allowed now
// view cone helper: depth d (0..3) forward, lateral offset o (-2..2, + = right) -> absolute cell
void map_rel(Pos p, int d, int o, int *x, int *y);

// automap memory (auto layer): bitsets sized w*h and w*h*4
typedef struct { int w, h; u8 *vis, *seen; } Amap;
void amap_init(Amap *, const Map *);
void amap_free(Amap *);
void amap_visit(Amap *, const Map *, Pos);   // mark cell visited + edges in the 4-deep view cone seen
int  amap_visited(const Amap *, int x, int y);
int  amap_seen(const Amap *, int x, int y, int side);

#endif
