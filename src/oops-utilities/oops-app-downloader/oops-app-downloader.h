#ifndef OOPS_APPS_OOPS_APP_DOWNLOADER_H
#define OOPS_APPS_OOPS_APP_DOWNLOADER_H

#include "oops/draw.h"

/*
 * oops-app-downloader: browse the oops-apps catalogue and install homebrew on the console.
 *
 * The render is a pure function of a small status block, so the screen is testable on a host,
 * the same way net-tool's panel is. The catalogue fetch, download and unpack are the
 * console-only half and are gated on two oops-sdk facilities that do not exist yet (an HTTPS
 * client and an on-device unzip) - see README.md. Until they land, the shell shows which
 * capabilities are ready and which are pending rather than pretending to install anything.
 */

typedef struct oops_dl_status {
    const char *ip;      /* console IP as text, or NULL if the network is not up */
    int net_ready;       /* raw sockets available (oops/net.h) */
    int install_ready;   /* the /data/homebrew install path is writable (oops/fs.h) */
    int http_ready;      /* HTTPS catalogue + download available (pending: oops/http.h) */
    int unzip_ready;     /* on-device unpack available (pending: oops/zip.h) */
} oops_dl_status_t;

/* Draw the bring-up screen for `st` into `surf`. Returns the number of text rows drawn. */
int oops_dl_render(oops_surface_t *surf, const oops_dl_status_t *st);

/* Build the on-device install directory for a title: "/data/homebrew/<TITLE_ID>". Writes into
 * `buf` and returns the length written, or -1 if `title_id` is not a well-formed 9-character
 * title id ([A-Z]{4}[0-9]{5}) or `buf` is too small. Pure; no allocation, no libc. */
int oops_dl_install_path(const char *title_id, char *buf, int buf_len);

#endif /* OOPS_APPS_OOPS_APP_DOWNLOADER_H */
