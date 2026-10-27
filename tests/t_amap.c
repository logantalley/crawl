// t_amap.c - auto-map: visited cells and edges seen in the view cone
#include "../src/core/map.h"
#include "t.h"

int main(void) {
  Map m;
  Amap a;
  Err e = {{0}, 0};
  Pos p;
  int x, y;
  if (map_load(&m, "examples/demo/maps/b1.toml", &e)) { fprintf(stderr, "b1: %d: %s\n", e.line, e.msg); return 1; }

  // map_rel: forward along dir, + offset to the right
  p.x = 3; p.y = 3; p.dir = DN;
  map_rel(p, 1, 0, &x, &y); CHECK(x == 3 && y == 2);
  map_rel(p, 0, 1, &x, &y); CHECK(x == 4 && y == 3);   // right of north is east
  p.dir = DS;
  map_rel(p, 2, 1, &x, &y); CHECK(x == 2 && y == 5);   // right of south is west
  p.dir = DE;
  map_rel(p, 1, -1, &x, &y); CHECK(x == 4 && y == 2);

  amap_init(&a, &m);
  CHECK(a.vis != NULL && a.seen != NULL);
  CHECK_EQ(a.w, 8);

  // campaign start: (0,1) facing south
  p.x = 0; p.y = 1; p.dir = DS;
  amap_visit(&a, &m, p);
  CHECK(amap_visited(&a, 0, 1));
  CHECK(!amap_visited(&a, 0, 2));
  CHECK(!amap_visited(&a, 0, 0));
  CHECK(amap_seen(&a, 0, 1, DE));        // own walls
  CHECK(amap_seen(&a, 0, 0, DS));        // edge behind us (shared with (0,1) N)
  CHECK(!amap_seen(&a, 0, 0, DN));
  CHECK(amap_seen(&a, 0, 2, DS));        // wall ahead at depth 1
  CHECK(amap_seen(&a, 0, 3, DN));        // same edge, other side
  CHECK(!amap_seen(&a, 0, 3, DS));       // behind that wall: not seen
  CHECK(amap_seen(&a, 1, 2, DS));        // door seen through the open side passage
  CHECK(!amap_seen(&a, 1, 3, DS));       // doors are opaque
  CHECK(amap_seen(&a, 2, 2, DE));        // depth 1, offset -2 (east)
  CHECK(!amap_seen(&a, 3, 2, DE));       // beyond the cone width
  CHECK(!amap_seen(&a, 1, 1, DE));       // rock behind a wall
  CHECK(amap_seen(&a, 1, 1, DS));        // ...but its south wall faces (1,2)

  // from S facing east: the wall at (2,0)E hides (3,0)
  p.x = 0; p.y = 0; p.dir = DE;
  amap_visit(&a, &m, p);
  CHECK(amap_visited(&a, 0, 0));
  CHECK(amap_visited(&a, 0, 1));         // still remembered
  CHECK(amap_seen(&a, 2, 0, DE));
  CHECK(amap_seen(&a, 3, 0, DW));
  CHECK(!amap_seen(&a, 3, 0, DE));
  CHECK(!amap_seen(&a, 3, 0, DS));
  CHECK(amap_seen(&a, 2, 1, DE));        // side passage (2,0)->(2,1) at depth 2
  CHECK(amap_seen(&a, 3, 2, DE));        // depth 3 via (2,2)->(3,2)

  // out of range queries are safe
  CHECK(!amap_visited(&a, -1, 0));
  CHECK(!amap_visited(&a, 8, 0));
  CHECK(!amap_seen(&a, 0, 8, DN));
  CHECK(!amap_seen(&a, 0, 0, 4));

  amap_free(&a);
  map_free(&m);
  return T_DONE();
}
