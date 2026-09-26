/*
 * libcurl with no network behind it. `include/curl/curl.h` has why, and which paths
 * upstream takes on each answer.
 *
 * The one piece of state worth having is the multi handle's queue of transfers: each
 * one added is reported back once as `CURLMSG_DONE` with `CURLE_COULDNT_CONNECT`, which
 * is what lets the downloader close the transfer, run its callbacks with `false`, and
 * take the progress dialog down.
 */
#include <curl/curl.h>

#include <cstdarg>
#include <cstring>
#include <vector>

namespace {
const char k_offline[] = "no network on this console";
} // namespace

struct stx_curl_easy {
    char *error_buffer = nullptr;
};

struct stx_curl_multi {
    std::vector<CURL *> pending;
    CURLMsg current{};
};

static void report(CURL *h) {
    if (h && h->error_buffer) {
        std::strncpy(h->error_buffer, k_offline, CURL_ERROR_SIZE - 1);
        h->error_buffer[CURL_ERROR_SIZE - 1] = '\0';
    }
}

extern "C" {

CURLcode curl_global_init(long) {
    return CURLE_OK;
}
void curl_global_cleanup(void) {}

CURL *curl_easy_init(void) {
    return new stx_curl_easy;
}

/* Only the error buffer is kept; every other option describes a transfer that will not
 * happen. The variadic argument is read only for that one, because reading a `long`
 * where a pointer was passed is undefined on this ABI and there is no reason to. */
CURLcode curl_easy_setopt(CURL *h, CURLoption option, ...) {
    if (h && option == CURLOPT_ERRORBUFFER) {
        va_list ap;
        va_start(ap, option);
        h->error_buffer = va_arg(ap, char *);
        va_end(ap);
    }
    return CURLE_OK;
}

CURLcode curl_easy_perform(CURL *h) {
    report(h);
    return CURLE_COULDNT_CONNECT;
}

void curl_easy_cleanup(CURL *h) {
    delete h;
}

const char *curl_easy_strerror(CURLcode code) {
    return code == CURLE_OK ? "No error"
                            : "Couldn't connect to server (no network on this console)";
}

CURLM *curl_multi_init(void) {
    return new stx_curl_multi;
}

CURLMcode curl_multi_add_handle(CURLM *m, CURL *e) {
    if (m && e)
        m->pending.push_back(e);
    return CURLM_OK;
}

CURLMcode curl_multi_remove_handle(CURLM *m, CURL *e) {
    if (m) {
        for (auto it = m->pending.begin(); it != m->pending.end(); ++it) {
            if (*it == e) {
                m->pending.erase(it);
                break;
            }
        }
    }
    return CURLM_OK;
}

CURLMcode curl_multi_perform(CURLM *m, int *running) {
    if (running)
        *running = 0;
    (void)m;
    return CURLM_OK;
}

/* One message per transfer, oldest first, **dequeued as it is read** - libcurl's
 * contract, and the one that cannot loop: a caller that reads without removing the
 * handle still sees each transfer finish exactly once. */
CURLMsg *curl_multi_info_read(CURLM *m, int *in_queue) {
    if (!m || m->pending.empty()) {
        if (in_queue)
            *in_queue = 0;
        return nullptr;
    }
    CURL *e = m->pending.front();
    m->pending.erase(m->pending.begin());
    report(e);
    m->current.msg = CURLMSG_DONE;
    m->current.easy_handle = e;
    m->current.data.result = CURLE_COULDNT_CONNECT;
    if (in_queue)
        *in_queue = static_cast<int>(m->pending.size());
    return &m->current;
}

CURLMcode curl_multi_cleanup(CURLM *m) {
    delete m;
    return CURLM_OK;
}

} // extern "C"
