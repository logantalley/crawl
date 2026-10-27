// t.h - tiny test macros. Each test is a standalone program run from the repo root.
#ifndef CRAWL_T_H
#define CRAWL_T_H
#include <stdio.h>
#include <string.h>

static int t_fail = 0, t_run = 0;
#define CHECK(c) do { t_run++; if (!(c)) { t_fail++; fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_EQ(a, b) do { long long t_a_ = (long long)(a), t_b_ = (long long)(b); t_run++; \
  if (t_a_ != t_b_) { t_fail++; fprintf(stderr, "%s:%d: CHECK_EQ failed: %s == %s (%lld vs %lld)\n", __FILE__, __LINE__, #a, #b, t_a_, t_b_); } } while (0)
#define CHECK_STR(a, b) do { const char *t_a_ = (a), *t_b_ = (b); t_run++; \
  if (!t_a_ || !t_b_ || strcmp(t_a_, t_b_)) { t_fail++; fprintf(stderr, "%s:%d: CHECK_STR failed: %s == %s (\"%s\" vs \"%s\")\n", __FILE__, __LINE__, #a, #b, t_a_ ? t_a_ : "(null)", t_b_ ? t_b_ : "(null)"); } } while (0)
#define T_DONE() (fprintf(stderr, "%s: %d/%d checks passed\n", __FILE__, t_run - t_fail, t_run), t_fail != 0)

#endif
