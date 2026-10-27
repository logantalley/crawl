// base.h - shared types and helpers (no SDL)
#ifndef CRAWL_BASE_H
#define CRAWL_BASE_H
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int8_t i8; typedef int16_t i16; typedef int32_t i32; typedef int64_t i64;

#define ID_MAX 32               // fixed-size IDs, incl. NUL
typedef char Id[ID_MAX];
#define NELEM(a) ((int)(sizeof(a) / sizeof((a)[0])))

// arena: bump allocator, freed whole
typedef struct { u8 *p; size_t n, cap; } Arena;
void *arena_alloc(Arena *, size_t n);   // zeroed, 16-aligned; grows by chaining is not needed: aborts on OOM
void  arena_free(Arena *);

// files
char *file_read(const char *path, size_t *n); // malloc'd, NUL-terminated; NULL on error
int   file_write(const char *path, const void *p, size_t n); // 0 ok

// error buffer: every loader takes one; first error wins
typedef struct { char msg[256]; int line; } Err;
void err_set(Err *, int line, const char *fmt, ...);

#endif
