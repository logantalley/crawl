// base.c - arena, files, errors
#include "base.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARENA_DEF (1u << 20)

void *arena_alloc(Arena *a, size_t n) {
  uintptr_t base, at;
  size_t off;
  if (!a->p) {
    size_t cap = a->cap ? a->cap : ARENA_DEF;
    if (cap < n + 16) cap = n + 16;
    a->p = (u8 *)malloc(cap);
    if (!a->p) { fputs("arena: out of memory\n", stderr); abort(); }
    a->cap = cap; a->n = 0;
  }
  base = (uintptr_t)a->p;
  at = (base + a->n + 15) & ~(uintptr_t)15;
  off = (size_t)(at - base);
  if (off > a->cap || n > a->cap - off) { fputs("arena: out of memory\n", stderr); abort(); }
  a->n = off + n;
  memset(a->p + off, 0, n);
  return a->p + off;
}

void arena_free(Arena *a) {
  free(a->p);
  a->p = NULL; a->n = 0; a->cap = 0;
}

char *file_read(const char *path, size_t *n) {
  FILE *f = fopen(path, "rb");
  char *buf = NULL;
  size_t cap = 0, len = 0;
  if (n) *n = 0;
  if (!f) return NULL;
  for (;;) {
    size_t got;
    if (cap - len < 4096 + 1) {
      size_t nc = cap ? cap * 2 : 8192;
      char *nb = (char *)realloc(buf, nc);
      if (!nb) { free(buf); fclose(f); return NULL; }
      buf = nb; cap = nc;
    }
    got = fread(buf + len, 1, cap - len - 1, f);
    len += got;
    if (got == 0) break;
  }
  if (ferror(f)) { free(buf); fclose(f); return NULL; }
  fclose(f);
  buf[len] = 0;
  if (n) *n = len;
  return buf;
}

int file_write(const char *path, const void *p, size_t n) {
  FILE *f = fopen(path, "wb");
  int bad;
  if (!f) return -1;
  bad = n && fwrite(p, 1, n, f) != n;
  if (fclose(f) != 0) bad = 1;
  return bad ? -1 : 0;
}

void err_set(Err *e, int line, const char *fmt, ...) {
  va_list ap;
  if (!e || e->msg[0]) return;   // first error wins
  e->line = line;
  va_start(ap, fmt);
  vsnprintf(e->msg, sizeof e->msg, fmt, ap);
  va_end(ap);
  if (!e->msg[0]) { e->msg[0] = '?'; e->msg[1] = 0; }
}
