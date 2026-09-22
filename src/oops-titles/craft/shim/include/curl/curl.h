/*
 * libcurl's two global calls, for a port that has no libcurl.
 *
 * `upstream/src/auth.c` is the only file that *uses* curl, and the Makefile excludes it - see
 * `shim/craft_offline.c` for why the file goes rather than the dependency being stubbed. But
 * `main.c` includes <curl/curl.h> for itself and calls `curl_global_init` and
 * `curl_global_cleanup` around its main loop, so excluding auth.c is not quite enough.
 *
 * **This is a header, not a patch, and that is the point.** Upstream's include list is left
 * exactly as it is and the answer comes from outside its tree - the same move
 * `extreme-tux-racer` uses to satisfy its Linux branch's GL headers. A patch to delete three
 * lines from `main.c` would be three lines that have to be rebased every time upstream moves,
 * to achieve what an include path already achieves.
 *
 * The two functions are `static inline` and do nothing. On a desktop they initialise and tear
 * down libcurl's global state; with no libcurl there is no state, and upstream ignores the
 * return of the first anyway.
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
