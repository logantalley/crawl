// t_round.c - canonical writer: parse -> write -> parse -> write is byte-identical
#include "../src/core/map.h"
#include "t.h"
#include <stdlib.h>

// read a file and drop '\r' so a CRLF checkout compares like LF
static char *slurp(const char *path, size_t *n) {
  size_t i, j = 0;
  char *s = file_read(path, n);
  if (!s) { fprintf(stderr, "cannot read %s\n", path); return NULL; }
  for (i = 0; i < *n; i++) if (s[i] != '\r') s[j++] = s[i];
  s[j] = 0;
  *n = j;
  return s;
}

static void same_map(const Map *a, const Map *b) {
  int x, y, s;
  CHECK_EQ(a->w, b->w);
  CHECK_EQ(a->h, b->h);
  if (a->w != b->w || a->h != b->h) return;
  for (y = 0; y < a->h; y++)
    for (x = 0; x < a->w; x++) {
      const Mark *ma = map_mark(a, x, y), *mb = map_mark(b, x, y);
      for (s = 0; s < 4; s++) CHECK_EQ(map_edge(a, x, y, s), map_edge(b, x, y, s));
      CHECK_EQ(a->cell[y * a->w + x], b->cell[y * b->w + x]);
      CHECK((ma == NULL) == (mb == NULL));
      if (ma && mb) { CHECK(ma->g == mb->g); CHECK_STR(ma->ev, mb->ev); CHECK_STR(ma->look, mb->look); }
    }
  CHECK_EQ(a->nleg, b->nleg);
  CHECK_EQ(a->neprop, b->neprop);
  CHECK_EQ(a->nzone, b->nzone);
  CHECK_EQ(a->nfoe, b->nfoe);
  CHECK_EQ(a->enc_rate, b->enc_rate);
  CHECK_STR(a->enc_tbl, b->enc_tbl);
}

static int eq(const char *a, size_t na, const char *b, size_t nb, const char *what) {
  size_t i;
  if (na == nb && !memcmp(a, b, na)) return 1;
  for (i = 0; i < na && i < nb && a[i] == b[i]; i++) {}
  fprintf(stderr, "%s: differ at byte %u (len %u vs %u)\n--- got:\n%.*s\n--- want:\n%.*s\n", what,
          (unsigned)i, (unsigned)na, (unsigned)nb, (int)na, a, (int)nb, b);
  return 0;
}

int main(void) {
  Map m1, m2;
  Err e = {{0}, 0};
  size_t ns, n1, n2, nc;
  char *src, *w1, *w2, *canon;

  // b1 from the spec: canonical form is stable and matches the golden file
  src = slurp("examples/demo/maps/b1.toml", &ns);
  canon = slurp("tests/fixtures/b1_canon.toml", &nc);
  if (!src || !canon) return 1;
  CHECK_EQ(map_parse(&m1, src, ns, &e), 0);
  w1 = map_write(&m1, &n1);
  CHECK(w1 != NULL);
  if (!w1) return 1;
  CHECK_EQ(strlen(w1), n1);
  CHECK(strchr(w1, '\r') == NULL);
  CHECK(eq(w1, n1, canon, nc, "write(parse(b1)) vs b1_canon.toml"));
  CHECK_EQ(map_parse(&m2, w1, n1, &e), 0);
  if (e.msg[0]) fprintf(stderr, "reparse: %d: %s\n", e.line, e.msg);
  w2 = map_write(&m2, &n2);
  CHECK(w2 != NULL);
  if (w2) CHECK(eq(w2, n2, w1, n1, "b1 second write"));
  same_map(&m1, &m2);
  free(w1); free(w2); free(src); free(canon);
  map_free(&m1); map_free(&m2);

  // all-glyphs fixture is already canonical: write(parse(f)) == f
  src = slurp("tests/fixtures/allglyphs.toml", &ns);
  if (!src) return 1;
  CHECK_EQ(map_parse(&m1, src, ns, &e), 0);
  if (e.msg[0]) fprintf(stderr, "allglyphs: %d: %s\n", e.line, e.msg);
  w1 = map_write(&m1, &n1);
  CHECK(w1 != NULL);
  if (w1) CHECK(eq(w1, n1, src, ns, "write(parse(allglyphs)) vs allglyphs.toml"));
  CHECK(map_mark(&m1, 3, 1) != NULL);
  if (map_mark(&m1, 3, 1)) CHECK_STR(map_mark(&m1, 3, 1)->ev, "k_ev\"q\"");
  free(w1); free(src);

  // runtime flags are not persisted; edits round-trip
  map_set_edge(&m1, 2, 0, DE, ED_SECRET | EF_FOUND);
  map_set_edge(&m1, 0, 2, DE, ED_OWGO);   // rock (0,2) -> (1,2) one-way east
  w1 = map_write(&m1, &n1);
  CHECK(w1 && strstr(w1, "|#>. @ .|") != NULL);
  CHECK(w1 && strstr(w1, "|.d.l.s.|") != NULL);
  if (w1) {
    CHECK_EQ(map_parse(&m2, w1, n1, &e), 0);
    CHECK_EQ(map_edge(&m2, 2, 0, DE), ED_SECRET);
    CHECK_EQ(map_edge(&m2, 1, 2, DW), ED_OWNO);
    map_free(&m2);
  }
  free(w1);
  map_free(&m1);

  // empty map struct writes without crashing
  {
    Map z;
    memset(&z, 0, sizeof z);
    w1 = map_write(&z, &n1);
    CHECK(w1 != NULL);
    free(w1);
  }
  return T_DONE();
}
