// check.h - campaign validation (`crawl check`)
#ifndef CRAWL_CHECK_H
#define CRAWL_CHECK_H
#include <stdio.h>

// Validate the campaign in dir. Prints `path:line: error: msg` / `path: warning: msg`
// lines to out (may be NULL). Returns the number of errors (warnings don't count).
int check_campaign(const char *dir, FILE *out);

#endif
