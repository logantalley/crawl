// db.c - TOML loading (crawl dialect), getters, campaign header
#include "db.h"
#include "map.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// rewrite top-level `;` to '\n'. Strings, comments and [..]/{..} are left alone.
char *db_prep(const char *src, size_t n) {
  char *o = (char *)malloc(n + 1);
  size_t i = 0;
  int depth = 0;
  if (!o) return NULL;
  if (n) memcpy(o, src, n);
  o[n] = 0;
  while (i < n) {
    char c = o[i];
    if (c == '#') {
      while (i < n && o[i] != '\n') i++;
    } else if (c == '"' || c == '\'') {
      if (i + 2 < n && o[i + 1] == c && o[i + 2] == c) {   // multi-line
        int extra = 0;
        i += 3;
        while (i < n) {
          if (c == '"' && o[i] == '\\') { i += 2; continue; }
          if (o[i] == c && i + 2 < n && o[i + 1] == c && o[i + 2] == c) break;
          if (o[i] == c && i + 2 >= n) { i = n; break; }  // unterminated
          i++;
        }
        if (i < n) {
          i += 3;
          while (i < n && o[i] == c && extra < 2) { i++; extra++; }  // """" -> quote + close
        }
      } else {
        i++;
        while (i < n && o[i] != c && o[i] != '\n') {
          if (c == '"' && o[i] == '\\' && i + 1 < n) i++;
          i++;
        }
        if (i < n && o[i] == c) i++;
      }
    } else {
      if (c == '[' || c == '{') depth++;
      else if ((c == ']' || c == '}') && depth > 0) depth--;
      else if (c == ';' && depth == 0) o[i] = '\n';
      i++;
    }
  }
  return o;
}

static void remap_lines(toml_datum_t *d, const int *map, int nmap) {
  int i;
  if (d->lineno > 0 && d->lineno < nmap) d->lineno = map[d->lineno];
  if (d->type == TOML_TABLE) {
    for (i = 0; i < d->u.tab.size; i++) remap_lines(&d->u.tab.value[i], map, nmap);
  } else if (d->type == TOML_ARRAY) {
    for (i = 0; i < d->u.arr.size; i++) remap_lines(&d->u.arr.elem[i], map, nmap);
  }
}

int db_parse(toml_result_t *r, const char *src, size_t n, const char *name, Err *err) {
  char *p;
  int *map, nmap, pl, ol;
  size_t i;
  memset(r, 0, sizeof *r);
  if (n > (size_t)INT_MAX - 1) { err_set(err, 0, "file too big"); return -1; }
  p = db_prep(src, n);
  if (!p) { err_set(err, 0, "out of memory"); return -1; }
  // prepped line -> original line (prep only adds newlines)
  nmap = 2;
  for (i = 0; i < n; i++) nmap += p[i] == '\n';
  map = (int *)malloc(sizeof(int) * (size_t)nmap);
  if (!map) { free(p); err_set(err, 0, "out of memory"); return -1; }
  map[0] = 0; map[1] = 1; pl = 1; ol = 1;
  for (i = 0; i < n; i++) {
    if (p[i] == '\n') {
      if (src[i] == '\n') ol++;
      pl++;
      if (pl < nmap) map[pl] = ol;
    }
  }
  *r = toml_parse_named(p, (int)n, name);
  free(p);
  if (!r->ok) {
    const char *m = r->errmsg;
    int line = 0;
    char buf[200];
    size_t k;
    if (strncmp(m, "(line ", 6) == 0) {
      line = atoi(m + 6);
      if (line > 0 && line < nmap) line = map[line];
      m = strchr(m, ')');
      m = m ? m + 1 : r->errmsg;
      while (*m == ' ') m++;
    }
    k = strlen(m);
    if (k >= sizeof buf) k = sizeof buf - 1;
    memcpy(buf, m, k); buf[k] = 0;
    while (k && (buf[k - 1] == '\n' || buf[k - 1] == '\r')) buf[--k] = 0;
    err_set(err, line, "%s", buf);
    toml_free(*r);
    memset(r, 0, sizeof *r);
    free(map);
    return -1;
  }
  remap_lines(&r->toptab, map, nmap);
  free(map);
  return 0;
}

int db_load(toml_result_t *r, const char *path, Err *err) {
  size_t n;
  char *s = file_read(path, &n);
  int rc;
  if (!s) {
    memset(r, 0, sizeof *r);
    err_set(err, 0, "cannot read file");
    return -1;
  }
  rc = db_parse(r, s, n, path, err);
  free(s);
  return rc;
}

int db_str(toml_datum_t t, const char *key, char *out, size_t n) {
  toml_datum_t v;
  size_t len;
  if (t.type != TOML_TABLE) return 1;
  v = toml_get(t, key);
  if (v.type != TOML_STRING) return 1;
  if (!n) return 2;
  len = (size_t)v.u.str.len;
  if (len >= n) {             // truncated: still fill, but report
    memcpy(out, v.u.str.ptr, n - 1);
    out[n - 1] = 0;
    return 2;
  }
  memcpy(out, v.u.str.ptr, len);
  out[len] = 0;
  return 0;
}

int db_int(toml_datum_t t, const char *key, i64 *out) {
  toml_datum_t v;
  if (t.type != TOML_TABLE) return 1;
  v = toml_get(t, key);
  if (v.type != TOML_INT64) return 1;
  *out = v.u.int64;
  return 0;
}

int dir_parse(const char *s) {
  if (!s || !s[0] || s[1]) return -1;
  switch (s[0]) {
  case 'n': return DN;
  case 'e': return DE;
  case 's': return DS;
  case 'w': return DW;
  default: return -1;
  }
}

// optional string field; 0 ok/absent, -1 on error
static int opt_str(toml_datum_t t, const char *key, char *out, size_t n, int req, Err *err) {
  toml_datum_t v = toml_get(t, key);
  int rc;
  if (v.type == TOML_UNKNOWN) {
    if (req) { err_set(err, t.lineno, "missing '%s'", key); return -1; }
    return 0;
  }
  rc = db_str(t, key, out, n);
  if (rc == 1) { err_set(err, v.lineno, "'%s' must be a string", key); return -1; }
  if (rc == 2) { err_set(err, v.lineno, "'%s' is too long (max %d)", key, (int)n - 1); return -1; }
  return 0;
}

static int str_arr(toml_datum_t t, const char *key, char (*out)[128], int max, int *cnt, int req, Err *err) {
  toml_datum_t v = toml_get(t, key);
  int i;
  *cnt = 0;
  if (v.type == TOML_UNKNOWN) {
    if (req) { err_set(err, 0, "missing '%s'", key); return -1; }
    return 0;
  }
  if (v.type != TOML_ARRAY) { err_set(err, v.lineno, "'%s' must be an array of strings", key); return -1; }
  if (v.u.arr.size > max) { err_set(err, v.lineno, "'%s' has too many entries (max %d)", key, max); return -1; }
  for (i = 0; i < v.u.arr.size; i++) {
    toml_datum_t e = v.u.arr.elem[i];
    if (e.type != TOML_STRING) { err_set(err, e.lineno ? e.lineno : v.lineno, "'%s' must be an array of strings", key); return -1; }
    if (e.u.str.len >= 128) { err_set(err, e.lineno ? e.lineno : v.lineno, "'%s' entry too long", key); return -1; }
    memcpy(out[i], e.u.str.ptr, (size_t)e.u.str.len + 1);
  }
  *cnt = v.u.arr.size;
  return 0;
}

int db_campaign(Campaign *c, const char *dir, Err *err) {
  char path[512];
  toml_result_t r;
  toml_datum_t t, st, at, v;
  char dirs[8];
  int rc = -1;
  memset(c, 0, sizeof *c);
  if (snprintf(path, sizeof path, "%s/campaign.toml", dir) >= (int)sizeof path) {
    err_set(err, 0, "path too long");
    return -1;
  }
  if (db_load(&r, path, err)) return -1;
  t = r.toptab;
  memcpy(c->rules, "lite", 5);
  memcpy(c->amap, "auto", 5);
  c->party = 6;
  if (opt_str(t, "id", c->id, sizeof c->id, 1, err)) goto out;
  if (opt_str(t, "title", c->title, sizeof c->title, 0, err)) goto out;
  if (opt_str(t, "ver", c->ver, sizeof c->ver, 0, err)) goto out;
  if (opt_str(t, "rules", c->rules, sizeof c->rules, 0, err)) goto out;
  if (opt_str(t, "amap", c->amap, sizeof c->amap, 0, err)) goto out;
  if (opt_str(t, "lic", c->lic, sizeof c->lic, 0, err)) goto out;
  if (opt_str(t, "credit", c->credit, sizeof c->credit, 0, err)) goto out;
  if (strcmp(c->amap, "auto") && strcmp(c->amap, "draw") && strcmp(c->amap, "both")) {
    err_set(err, toml_get(t, "amap").lineno, "amap must be auto, draw or both");
    goto out;
  }
  v = toml_get(t, "party");
  if (v.type != TOML_UNKNOWN) {
    if (v.type != TOML_INT64 || v.u.int64 < 1 || v.u.int64 > 6) {
      err_set(err, v.lineno, "party must be an integer 1..6");
      goto out;
    }
    c->party = (int)v.u.int64;
  }
  st = toml_get(t, "start");
  if (st.type != TOML_TABLE) {
    err_set(err, st.lineno, "%s", st.type == TOML_UNKNOWN ? "missing 'start'" : "'start' must be a table");
    goto out;
  }
  if (opt_str(st, "map", c->start_map, sizeof c->start_map, 1, err)) goto out;
  at = toml_get(st, "at");
  if (at.type != TOML_ARRAY || at.u.arr.size != 2 || at.u.arr.elem[0].type != TOML_INT64 ||
      at.u.arr.elem[1].type != TOML_INT64 || at.u.arr.elem[0].u.int64 < 0 || at.u.arr.elem[1].u.int64 < 0 ||
      at.u.arr.elem[0].u.int64 > 4096 || at.u.arr.elem[1].u.int64 > 4096) {
    err_set(err, at.lineno ? at.lineno : st.lineno, "start.at must be [x, y]");
    goto out;
  }
  c->start_x = (int)at.u.arr.elem[0].u.int64;
  c->start_y = (int)at.u.arr.elem[1].u.int64;
  if (db_str(st, "dir", dirs, sizeof dirs) || (c->start_dir = dir_parse(dirs)) < 0) {
    err_set(err, st.lineno, "start.dir must be \"n\", \"e\", \"s\" or \"w\"");
    goto out;
  }
  if (str_arr(t, "maps", c->maps, NELEM(c->maps), &c->nmaps, 1, err)) goto out;
  if (str_arr(t, "inc", c->inc, NELEM(c->inc), &c->ninc, 0, err)) goto out;
  rc = 0;
out:
  toml_free(r);
  return rc;
}
