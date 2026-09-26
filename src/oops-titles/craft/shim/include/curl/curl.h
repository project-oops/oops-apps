/*
 * libcurl's two global calls, for a port that has no libcurl.
 *
 * The Makefile excludes `upstream/src/auth.c`, but `main.c` still includes
 * <curl/curl.h> and calls `curl_global_init` and `curl_global_cleanup` around its main
 * loop. A header on the include path answers them and leaves upstream unpatched. Both
 * do nothing; upstream ignores the return of the first.
 */
#ifndef OOPS_CRAFT_CURL_H
#define OOPS_CRAFT_CURL_H

#define CURL_GLOBAL_DEFAULT 3L

typedef int CURLcode;
#define CURLE_OK 0

static inline CURLcode curl_global_init(long flags) {
    (void)flags;
    return CURLE_OK;
}

static inline void curl_global_cleanup(void) {}

#endif /* OOPS_CRAFT_CURL_H */
