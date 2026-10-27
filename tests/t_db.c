// t_db.c - db_prep, db_parse line mapping, getters, campaign load, check
#include "../src/core/db.h"
#include "../src/core/map.h"
#include "../src/core/check.h"
#include "t.h"
#include <stdlib.h>

static void prep_is(const char *in, const char *want, int line) {
  char *o = db_prep(in, strlen(in));
  t_run++;
  if (!o || strcmp(o, want)) {
    t_fail++;
    fprintf(stderr, "%s:%d: db_prep(\"%s\") = \"%s\", want \"%s\"\n", __FILE__, line, in, o ? o : "(null)", want);
  }
  free(o);
}
#define PREP(a, b) prep_is(a, b, __LINE__)

int main(void) {
  // db_prep
  PREP("a = 1; b = 2", "a = 1\n b = 2");
  PREP("s = \"x;y\"; t = 'p;q'", "s = \"x;y\"\n t = 'p;q'");
  PREP("s = \"a\\\";b\"; c = 1", "s = \"a\\\";b\"\n c = 1");
  PREP("a = [1; 2]", "a = [1; 2]");
  PREP("t = { a = \"1\"; b = [2;3] }; b = 2", "t = { a = \"1\"; b = [2;3] }\n b = 2");
  PREP("a = 1 # c; d\nb = 2; c = 3", "a = 1 # c; d\nb = 2\n c = 3");
  PREP("m = \"\"\"x;\"y\";\n\"\"\"; z = 1", "m = \"\"\"x;\"y\";\n\"\"\"\n z = 1");
  PREP("m = \"\"\"q\"\"\"\"; z = 1", "m = \"\"\"q\"\"\"\"\n z = 1");
  PREP("l = '''a;b'''; c = 1", "l = '''a;b'''\n c = 1");
  PREP("[[edge]]\nat = [3, 0]; side = \"s\"", "[[edge]]\nat = [3, 0]\n side = \"s\"");
  PREP("a = 'it;s' # don't; x", "a = 'it;s' # don't; x");
  PREP("", "");

  // db_parse: values and line numbers survive the `;` rewrite
  {
    const char *s = "x = 1; y = \"two\"\nz = 3\n[t]\nk = \"v\"; n = 7\n";
    toml_result_t r;
    Err e = {{0}, 0};
    i64 v = 0;
    char buf[8];
    CHECK_EQ(db_parse(&r, s, strlen(s), "mem", &e), 0);
    CHECK(db_int(r.toptab, "x", &v) == 0 && v == 1);
    CHECK(db_str(r.toptab, "y", buf, sizeof buf) == 0);
    CHECK_STR(buf, "two");
    CHECK_EQ(toml_get(r.toptab, "y").lineno, 1);
    CHECK_EQ(toml_get(r.toptab, "z").lineno, 2);
    CHECK_EQ(toml_get(toml_get(r.toptab, "t"), "n").lineno, 4);
    CHECK(db_int(toml_get(r.toptab, "t"), "n", &v) == 0 && v == 7);
    CHECK(db_str(r.toptab, "y", buf, 3) == 2);     // truncated
    CHECK_STR(buf, "tw");
    CHECK(db_str(r.toptab, "x", buf, sizeof buf) == 1);  // wrong type
    CHECK(db_int(r.toptab, "nope", &v) == 1);
    toml_free(r);
  }
  {
    const char *s = "a = 1; b = 2\nc = = 3\n";
    toml_result_t r;
    Err e = {{0}, 0};
    CHECK(db_parse(&r, s, strlen(s), "mem", &e) != 0);
    CHECK_EQ(e.line, 2);
    CHECK(e.msg[0] != 0);
    toml_free(r);   // safe on a failed parse
  }

  // dir_parse
  CHECK_EQ(dir_parse("n"), DN);
  CHECK_EQ(dir_parse("e"), DE);
  CHECK_EQ(dir_parse("s"), DS);
  CHECK_EQ(dir_parse("w"), DW);
  CHECK_EQ(dir_parse("x"), -1);
  CHECK_EQ(dir_parse("ne"), -1);
  CHECK_EQ(dir_parse(""), -1);

  // campaign
  {
    Campaign c;
    Err e = {{0}, 0};
    int rc = db_campaign(&c, "examples/demo", &e);
    CHECK_EQ(rc, 0);
    if (rc) fprintf(stderr, "campaign: %d: %s\n", e.line, e.msg);
    CHECK_STR(c.id, "thornwall");
    CHECK_STR(c.title, "Under Thornwall");
    CHECK_STR(c.ver, "0.1.0");
    CHECK_STR(c.rules, "lite");
    CHECK_STR(c.amap, "both");
    CHECK_EQ(c.party, 6);
    CHECK_STR(c.start_map, "b1");
    CHECK_EQ(c.start_x, 0);
    CHECK_EQ(c.start_y, 1);
    CHECK_EQ(c.start_dir, DS);
    CHECK_EQ(c.nmaps, 1);
    CHECK_STR(c.maps[0], "maps/b1.toml");
    CHECK_STR(c.lic, "CC-BY-4.0");
  }
  {
    Campaign c;
    Err e = {{0}, 0};
    CHECK(db_campaign(&c, "tests/fixtures/no_such_dir", &e) != 0);
    CHECK(e.msg[0] != 0);
  }

  // check: the demo has no errors
  CHECK_EQ(check_campaign("examples/demo", stderr), 0);
  CHECK(check_campaign("tests/fixtures/no_such_dir", NULL) > 0);

  return T_DONE();
}
