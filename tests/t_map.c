// t_map.c - parse the spec's b1 example; lookups; bad input diagnostics
#include "../src/core/map.h"
#include "t.h"

static int bad(const char *src, int want_line, const char *want_sub) {
  Map m;
  Err e = {{0}, 0};
  int rc = map_parse(&m, src, strlen(src), &e);
  int ok = rc != 0 && e.line == want_line && (!want_sub || strstr(e.msg, want_sub));
  if (!ok) fprintf(stderr, "  bad(): rc=%d line=%d msg=\"%s\" (want line %d, \"%s\")\n", rc, e.line, e.msg, want_line, want_sub ? want_sub : "");
  map_free(&m);
  return ok;
}

int main(void) {
  Map m;
  Err e = {{0}, 0};
  const Mark *mk;
  const Zone *z;
  const EdgeProp *ep;
  int rc = map_load(&m, "examples/demo/maps/b1.toml", &e);
  CHECK_EQ(rc, 0);
  if (rc) { fprintf(stderr, "b1: %d: %s\n", e.line, e.msg); return 1; }

  CHECK_STR(m.id, "b1");
  CHECK_STR(m.name, "Thornwall Cellars B1");
  CHECK_STR(m.tiles, "stone");
  CHECK_STR(m.kind, "dungeon");
  CHECK_EQ(m.enc_rate, 6);
  CHECK_STR(m.enc_tbl, "b1_rand");
  CHECK_EQ(m.w, 8);
  CHECK_EQ(m.h, 8);
  CHECK_EQ(m.nleg, 6);

  // markers at the positions the spec states
  mk = map_mark(&m, 7, 0); CHECK(mk && mk->g == 'T'); if (mk) { CHECK_STR(mk->ev, "b1_chest1"); CHECK_STR(mk->look, "chest"); }
  mk = map_mark(&m, 2, 4); CHECK(mk && mk->g == 'C'); if (mk) CHECK_STR(mk->ev, "b1_chest2");
  mk = map_mark(&m, 0, 7); CHECK(mk && mk->g == 'D'); if (mk) CHECK_STR(mk->look, "stairs_dn");
  mk = map_mark(&m, 0, 0); CHECK(mk && mk->g == 'S');
  mk = map_mark(&m, 4, 2); CHECK(mk && mk->g == 'A');
  mk = map_mark(&m, 6, 4); CHECK(mk && mk->g == 'F');
  CHECK(map_mark(&m, 1, 0) == NULL);
  CHECK(map_mark(&m, -1, 0) == NULL);

  // cells
  CHECK_EQ(m.cell[1 * 8 + 1], CL_ROCK);
  CHECK_EQ(m.cell[5 * 8 + 5], CL_ROCK);
  CHECK_EQ(m.cell[0], CL_FLOOR);
  CHECK_EQ(m.cell[0 * 8 + 7], CL_FLOOR);   // T sits on floor

  // edges (both sides agree)
  CHECK_EQ(ED_TYPE(map_edge(&m, 0, 0, DN)), ED_WALL);
  CHECK_EQ(ED_TYPE(map_edge(&m, 0, 0, DS)), ED_OPEN);
  CHECK_EQ(ED_TYPE(map_edge(&m, 0, 1, DN)), ED_OPEN);
  CHECK_EQ(ED_TYPE(map_edge(&m, 2, 0, DE)), ED_WALL);
  CHECK_EQ(ED_TYPE(map_edge(&m, 3, 0, DW)), ED_WALL);
  CHECK_EQ(ED_TYPE(map_edge(&m, 1, 2, DS)), ED_DOOR);
  CHECK_EQ(ED_TYPE(map_edge(&m, 1, 3, DN)), ED_DOOR);
  CHECK_EQ(ED_TYPE(map_edge(&m, 2, 3, DS)), ED_SECRET);
  CHECK_EQ(ED_TYPE(map_edge(&m, 2, 4, DN)), ED_SECRET);
  CHECK_EQ(ED_TYPE(map_edge(&m, 6, 2, DS)), ED_OWGO);
  CHECK_EQ(ED_TYPE(map_edge(&m, 6, 3, DN)), ED_OWNO);
  CHECK_EQ(ED_TYPE(map_edge(&m, 9, 9, DN)), ED_WALL);   // outside

  // [[edge]] at (3,0) s: keyed, so locked (grid has 'd'; the key makes it 'l')
  ep = map_eprop(&m, 3, 0, DS);
  CHECK(ep != NULL);
  if (ep) { CHECK_STR(ep->key, "bronze_key"); CHECK_EQ(ep->x, 3); CHECK_EQ(ep->y, 0); CHECK_EQ(ep->side, DS); }
  CHECK(map_eprop(&m, 3, 1, DN) == ep);            // matches from the other side too
  CHECK(map_eprop(&m, 3, 0, DE) == NULL);
  CHECK_EQ(ED_TYPE(map_edge(&m, 3, 0, DS)), ED_LOCK);
  CHECK_EQ(ED_TYPE(map_edge(&m, 3, 1, DN)), ED_LOCK);

  // zones
  CHECK_EQ(m.nzone, 1);
  z = map_zone(&m, 4, 5, "dark"); CHECK(z != NULL);
  z = map_zone(&m, 7, 7, "dark"); CHECK(z != NULL);
  CHECK(map_zone(&m, 3, 5, "dark") == NULL);
  CHECK(map_zone(&m, 4, 4, "dark") == NULL);
  CHECK(map_zone(&m, 5, 6, "damage") == NULL);
  CHECK(map_zone(&m, 5, 6, NULL) != NULL);

  // foes
  CHECK_EQ(m.nfoe, 1);
  CHECK_STR(m.foe[0].id, "wolf1");
  CHECK_STR(m.foe[0].grp, "wolves");
  CHECK_STR(m.foe[0].look, "foe_wolf");
  CHECK_STR(m.foe[0].ai, "patrol");
  CHECK_EQ(m.foe[0].n, 4);
  CHECK_EQ(m.foe[0].path[1][0], 7);
  CHECK_EQ(m.foe[0].path[1][1], 6);
  CHECK_EQ(m.foe[0].path[3][0], 5);
  CHECK_EQ(m.foe[0].path[3][1], 7);
  map_free(&m);

  // bad input: errors carry source line numbers
  CHECK(bad("id = \"x\"\nmap = \"\"\"\n+-+-+\n|.x.|\n+-+-+\n\"\"\"\n", 4, "vertical edge"));
  CHECK(bad("id = \"x\"\nmap = \"\"\"\n+-+-+\n|. .|\n+*+-+\n\"\"\"\n", 5, "horizontal edge"));
  CHECK(bad("id = \"x\"\nmap = \"\"\"\n+-+-+\n|. .|\n+-+\n\"\"\"\n", 5, "expected 5"));
  CHECK(bad("id = \"x\"\nmap = \"\"\"\n+-+-+\n|. .|\n+-+-+\n|. .|\n\"\"\"\n", 3, "rows"));
  CHECK(bad("id = \"x\"\nmap = \"\"\"\n+-+-+\n|.<Q|\n+-+-+\n\"\"\"\n", 4, "legend"));
  CHECK(bad("id = \"x\"\nmap = \"\"\"\n+-+-+\n|.^.|\n+-+-+\n\"\"\"\n", 4, "vertical edge"));
  CHECK(bad("id = \"x\"\nmap = \"\"\"\n+-+-+\n|. .|\n+>+-+\n\"\"\"\n", 5, "horizontal edge"));
  CHECK(bad("id = \"x\"; kind = \"d\"\nmap = \"\"\"\n+-+-+\n|. .|\n+-+-+\n\"\"\"\n[[edge]]\nat = [0, 0]; side = \"e\"; key = \"k\"\n", 8, "not a door"));
  CHECK(bad("map = \"\"\"\n+-+-+\n|. .|\n+-+-+\n\"\"\"\n[[zone]]\nk = \"dark\"; rect = [1, 0, 2, 1]\n", 7, "outside"));
  CHECK(bad("map = \"\"\"\n+-+-+\n|. .|\n+-+-+\n\"\"\"\n[[foe]]\nid = \"f\"; path = [[0, 0], [2, 0]]\n", 7, "outside"));
  CHECK(bad("id = \"x\"\n", 0, "map"));
  CHECK(bad("id = 3\nmap = \"\"\"\n+-+\n|.|\n+-+\n\"\"\"\n", 1, "string"));
  CHECK(bad("grid = \"bits\"\nmap = \"\"\"\n+-+\n|.|\n+-+\n\"\"\"\n", 1, "edge"));
  CHECK(bad("map = \"\"\"\n+-+\n|.|\n+-+\n\"\"\"\n[legend]\nXY = { ev = \"a\" }\n", 7, "legend"));
  CHECK(bad("a = = 1\n", 1, NULL));

  // a minimal 1x1 map with an open border parses
  {
    const char *s = "map = \"\"\"\n+ +\n|.|\n+-+\n\"\"\"\n";
    CHECK_EQ(map_parse(&m, s, strlen(s), &e), 0);
    CHECK_EQ(m.w, 1);
    CHECK_EQ(ED_TYPE(map_edge(&m, 0, 0, DN)), ED_OPEN);
    map_free(&m);
  }
  // CRLF sources parse the same
  {
    const char *s = "id = \"c\"\r\nmap = \"\"\"\r\n+-+-+\r\n|.d.|\r\n+-+-+\r\n\"\"\"\r\n";
    Err e2 = {{0}, 0};
    CHECK_EQ(map_parse(&m, s, strlen(s), &e2), 0);
    CHECK_EQ(m.w, 2);
    CHECK_EQ(m.h, 1);
    CHECK_EQ(ED_TYPE(map_edge(&m, 0, 0, DE)), ED_DOOR);
    map_free(&m);
  }
  return T_DONE();
}
