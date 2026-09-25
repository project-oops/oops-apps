/*
 * libcurl, answered "there is no network" - the names SuperTux's add-on downloader uses and no
 * others. `../../curl_offline.cpp` defines them.
 *
 * `src/addon/downloader.cpp` includes this unconditionally (`HAVE_LIBCURL` is in upstream's
 * `config.h` template and tested nowhere), so the add-on browser has to link. Every transfer
 * fails as an unreachable host would: `curl_easy_perform` returns `CURLE_COULDNT_CONNECT`, and a
 * transfer added to a multi handle comes back from `curl_multi_info_read` as `CURLMSG_DONE` with
 * that result. Both are paths the downloader already takes on an offline desktop, and the second
 * is the one that matters - a multi handle that never reported its transfers finished would
 * leave the add-on menu's progress dialog open forever.
 *
 * The types are opaque, and the option and result numbers are libcurl's own so that a log line
 * printing one reads the same as it would upstream.
 */
#ifndef STX_SHIM_CURL_CURL_H
#define STX_SHIM_CURL_CURL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stx_curl_easy CURL;
typedef struct stx_curl_multi CURLM;

typedef enum {
  CURLE_OK = 0,
  CURLE_COULDNT_CONNECT = 7,
} CURLcode;

typedef enum {
  CURLM_CALL_MULTI_PERFORM = -1,
  CURLM_OK = 0,
} CURLMcode;

/* libcurl's values: `CURLOPTTYPE_LONG` is 0, `OBJECTPOINT` 10000, `FUNCTIONPOINT` 20000. */
typedef enum {
  CURLOPT_WRITEDATA = 10001,
  CURLOPT_URL = 10002,
  CURLOPT_ERRORBUFFER = 10010,
  CURLOPT_USERAGENT = 10018,
  CURLOPT_PROGRESSDATA = 10057,
  CURLOPT_WRITEFUNCTION = 20011,
  CURLOPT_PROGRESSFUNCTION = 20056,
  CURLOPT_NOPROGRESS = 43,
  CURLOPT_FAILONERROR = 45,
  CURLOPT_FOLLOWLOCATION = 52,
  CURLOPT_NOSIGNAL = 99,
} CURLoption;

#define CURL_ERROR_SIZE 256
#define CURL_GLOBAL_ALL 3L

typedef enum {
  CURLMSG_NONE = 0,
  CURLMSG_DONE = 1,
} CURLMSG;

typedef struct CURLMsg {
  CURLMSG msg;
  CURL *easy_handle;
  union {
    void *whatever;
    CURLcode result;
  } data;
} CURLMsg;

CURLcode curl_global_init(long flags);
void curl_global_cleanup(void);

CURL *curl_easy_init(void);
CURLcode curl_easy_setopt(CURL *handle, CURLoption option, ...);
CURLcode curl_easy_perform(CURL *handle);
void curl_easy_cleanup(CURL *handle);
const char *curl_easy_strerror(CURLcode code);

CURLM *curl_multi_init(void);
CURLMcode curl_multi_add_handle(CURLM *multi, CURL *easy);
CURLMcode curl_multi_remove_handle(CURLM *multi, CURL *easy);
CURLMcode curl_multi_perform(CURLM *multi, int *running_handles);
CURLMsg *curl_multi_info_read(CURLM *multi, int *msgs_in_queue);
CURLMcode curl_multi_cleanup(CURLM *multi);

#ifdef __cplusplus
}
#endif

#endif
