// t_move.c - walk the spec's b1 example and the all-glyphs fixture
#include "../src/core/map.h"
#include "t.h"

static Map m;

// step and expect a result; on MV_OK also expect the new position
static void go(Pos *p, int dir, int want, int line) {
  int b = 0, r = map_step(&m, p, dir, &b);
  t_run++;
  if (r != want) {
    t_fail++;
    fprintf(stderr, "%s:%d: step %c from (%d,%d) = %d, want %d\n", __FILE__, line, "NESW"[dir], p->x, p->y, r, want);
  }
}
#define GO(p, d, w) go(p, d, w, __LINE__)
#define AT(p, X, Y) do { CHECK_EQ((p).x, X); CHECK_EQ((p).y, Y); } while (0)

int main(void) {
  Err e = {{0}, 0};
  Pos p;
  int b;
  if (map_load(&m, "examples/demo/maps/b1.toml", &e)) { fprintf(stderr, "b1: %d: %s\n", e.line, e.msg); return 1; }

  // from S, walls block, open edges pass
  p.x = 0; p.y = 0; p.dir = DS;
  GO(&p, DN, MV_WALL); AT(p, 0, 0);       // outer wall
  GO(&p, DW, MV_WALL);
  GO(&p, DE, MV_OK); AT(p, 1, 0);
  GO(&p, DS, MV_WALL);                    // wall under (1,0)
  GO(&p, DW, MV_OK);
  GO(&p, DS, MV_OK); AT(p, 0, 1);
  GO(&p, DE, MV_WALL);                    // rock (1,1) is walled
  GO(&p, DS, MV_OK); AT(p, 0, 2);
  GO(&p, DS, MV_WALL);
  CHECK_EQ(p.dir, DS);                    // stepping never changes facing

  // locked door at (3,0)s: blocked until unlocked
  GO(&p, DE, MV_OK); GO(&p, DE, MV_OK); GO(&p, DE, MV_OK); AT(p, 3, 2);
  GO(&p, DN, MV_OK); AT(p, 3, 1);
  GO(&p, DN, MV_LOCKED); AT(p, 3, 1);
  CHECK(!map_pass(&m, 3, 1, DN));
  map_set_edge(&m, 3, 1, DN, ED_LOCK | EF_UNLOCK);
  CHECK(map_edge(&m, 3, 0, DS) == (ED_LOCK | EF_UNLOCK));   // flags copied to the neighbor
  CHECK(map_pass(&m, 3, 1, DN));
  CHECK(map_pass(&m, 3, 0, DS));
  GO(&p, DN, MV_OK); AT(p, 3, 0);
  GO(&p, DW, MV_WALL);
  GO(&p, DS, MV_OK); AT(p, 3, 1);

  // door at (1,2)s passes both ways; secret door at (2,3)s bumps until found
  p.x = 1; p.y = 2;
  GO(&p, DS, MV_OK); AT(p, 1, 3);
  GO(&p, DN, MV_OK); AT(p, 1, 2);
  GO(&p, DS, MV_OK);
  GO(&p, DE, MV_OK); AT(p, 2, 3);
  b = 0;
  CHECK_EQ(map_step(&m, &p, DS, &b), MV_WALL);
  CHECK_EQ(b, 1);
  AT(p, 2, 3);
  CHECK_EQ(map_step(&m, &p, DW, &b), MV_OK);   // ordinary moves clear the flag
  CHECK_EQ(b, 0);
  GO(&p, DE, MV_OK);
  CHECK_EQ(map_step(&m, &p, DS, NULL), MV_WALL);  // NULL bump pointer is fine
  map_set_edge(&m, 2, 3, DS, ED_SECRET | EF_FOUND);
  CHECK_EQ(map_step(&m, &p, DS, &b), MV_OK);
  CHECK_EQ(b, 0);
  AT(p, 2, 4);
  CHECK(map_mark(&m, p.x, p.y) && map_mark(&m, p.x, p.y)->g == 'C');
  GO(&p, DN, MV_OK); AT(p, 2, 3);              // found secret works from both sides

  // one-way 'v' at (6,2)s: south only
  p.x = 4; p.y = 2;
  GO(&p, DE, MV_OK); GO(&p, DE, MV_OK); AT(p, 6, 2);
  CHECK(map_pass(&m, 6, 2, DS));
  CHECK(!map_pass(&m, 6, 3, DN));
  GO(&p, DS, MV_OK); AT(p, 6, 3);
  GO(&p, DN, MV_ONEWAY); AT(p, 6, 3);

  // false wall: map_set_edge writes both sides, then it walks through
  p.x = 0; p.y = 2;
  map_set_edge(&m, 0, 2, DS, ED_FALSE);
  CHECK_EQ(ED_TYPE(map_edge(&m, 0, 3, DN)), ED_FALSE);
  GO(&p, DS, MV_OK); AT(p, 0, 3);
  GO(&p, DN, MV_OK); AT(p, 0, 2);

  // rock blocks even through an open edge
  map_set_edge(&m, 4, 5, DE, ED_OPEN);
  p.x = 4; p.y = 5;
  GO(&p, DE, MV_ROCK); AT(p, 4, 5);

  // the map edge blocks even when the border edge is open
  map_set_edge(&m, 0, 0, DN, ED_OPEN);
  p.x = 0; p.y = 0;
  GO(&p, DN, MV_WALL); AT(p, 0, 0);
  CHECK(!map_pass(&m, 0, 0, DN));
  p.x = 7; p.y = 7;
  GO(&p, DE, MV_WALL);
  GO(&p, DS, MV_WALL);

  // one-way pairing through map_set_edge
  map_set_edge(&m, 4, 6, DE, ED_OWNO);      // (4,6)->(5,6) blocked, (5,6)->(4,6) allowed
  CHECK_EQ(ED_TYPE(map_edge(&m, 5, 6, DW)), ED_OWGO);
  map_free(&m);

  // fixture: every edge glyph
  if (map_load(&m, "tests/fixtures/allglyphs.toml", &e)) { fprintf(stderr, "allglyphs: %d: %s\n", e.line, e.msg); return 1; }
  p.x = 0; p.y = 0; p.dir = DN;
  GO(&p, DE, MV_OK); AT(p, 1, 0);            // 'd' door
  GO(&p, DE, MV_LOCKED);                     // 'l' locked
  GO(&p, DS, MV_ONEWAY);                     // '^' : north only, so (1,0) may not go south
  GO(&p, DW, MV_OK);
  GO(&p, DS, MV_OK); AT(p, 0, 1);            // 'f' false wall
  GO(&p, DN, MV_OK); GO(&p, DS, MV_OK);
  GO(&p, DE, MV_ONEWAY);                     // '<' : west only
  p.x = 1; p.y = 1;
  GO(&p, DW, MV_OK); AT(p, 0, 1);            // '<' westward
  p.x = 1; p.y = 1;
  GO(&p, DN, MV_OK); AT(p, 1, 0);            // '^' northward
  p.x = 1; p.y = 1;
  GO(&p, DE, MV_OK); AT(p, 2, 1);            // '>' eastward
  GO(&p, DW, MV_ONEWAY);
  GO(&p, DN, MV_ONEWAY);                     // 'v' : south only
  p.x = 2; p.y = 0;
  GO(&p, DS, MV_OK); AT(p, 2, 1);            // 'v' southward
  p.x = 2; p.y = 0;
  CHECK_EQ(map_step(&m, &p, DE, &b), MV_WALL);  // 's' secret
  CHECK_EQ(b, 1);
  CHECK_EQ(ED_TYPE(map_edge(&m, 1, 0, DE)), ED_LOCK);
  CHECK(map_eprop(&m, 1, 0, DE) != NULL);
  p.x = 1; p.y = 2;
  GO(&p, DW, MV_ROCK);                       // '#' rock
  GO(&p, DS, MV_WALL);                       // open bottom border still blocks
  CHECK_EQ(ED_TYPE(map_edge(&m, 1, 2, DS)), ED_OPEN);
  map_free(&m);
  return T_DONE();
}
