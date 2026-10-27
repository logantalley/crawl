// db.h - TOML loading (crawl dialect) and campaign header
#ifndef CRAWL_DB_H
#define CRAWL_DB_H
#include "base.h"
#include "../../vendor/tomlc17.h"

// crawl dialect = TOML plus `;` as a key separator outside strings/brackets
// (spec examples write `at = [3, 0]; side = "s"`). db_prep rewrites those to newlines.
char *db_prep(const char *src, size_t n);                 // malloc'd, NUL-terminated
int   db_parse(toml_result_t *, const char *src, size_t n, const char *name, Err *); // 0 ok; caller toml_free
int   db_load(toml_result_t *, const char *path, Err *);

// typed getters: return 0 if present with the right type
int db_str(toml_datum_t t, const char *key, char *out, size_t n);
int db_int(toml_datum_t t, const char *key, i64 *out);

typedef struct {
  Id id, rules, amap; char title[64], ver[16], lic[32], credit[64];
  int party;
  Id start_map; int start_x, start_y, start_dir;
  char maps[16][128]; int nmaps;  // paths relative to campaign dir
  char inc[32][128]; int ninc;    // globs, unexpanded
} Campaign;

int db_campaign(Campaign *, const char *dir, Err *);      // reads <dir>/campaign.toml
int dir_parse(const char *s);                              // "n"/"e"/"s"/"w" -> DN..DW, -1 if bad

#endif
