#ifndef OOPS_APPS_OOPSY_DAISY_H
#define OOPS_APPS_OOPSY_DAISY_H

#include "oops/draw.h"

/*
 * OOPSy-daisy - Download Artificial Intelligence Slop Yourself.
 *
 * An on-device installer for the oops-apps catalogue: it reads the rolling `latest-main`
 * release, lists the titles that ship a `.zip`, and installs the one you pick straight into
 * the console's homebrew folder - no PC in the loop.
 *
 * The catalogue parse, the install-path helper and the screen are pure functions of their
 * inputs, so they are tested on a host (make check). The fetch, the download and the unpack are
 * the console-only half, in oopsy-daisy_main.c, over oops/http.h and oops/zip.h.
 */

#define OOPSY_MAX_ENTRIES 48
#define OOPSY_NAME_LEN    48
#define OOPSY_URL_LEN     256

typedef struct oopsy_entry {
    char name[OOPSY_NAME_LEN];   /* app name, e.g. "neverball" */
    char url[OOPSY_URL_LEN];     /* the title .zip download URL */
} oopsy_entry_t;

typedef struct oopsy_catalog {
    oopsy_entry_t items[OOPSY_MAX_ENTRIES];
    int count;
} oopsy_catalog_t;

typedef enum oopsy_phase {
    OOPSY_LOADING,     /* fetching the catalogue */
    OOPSY_BROWSE,      /* a list to pick from */
    OOPSY_INSTALLING,  /* downloading + unpacking the pick */
    OOPSY_DONE,        /* the pick installed */
    OOPSY_FAILED       /* something went wrong; message says what */
} oopsy_phase_t;

typedef struct oopsy_view {
    const oopsy_catalog_t *cat;  /* may be NULL while loading */
    int selected;                /* highlighted row */
    oopsy_phase_t phase;
    const char *message;         /* status line, or NULL */
} oopsy_view_t;

/* Parse the GitHub "release by tag" JSON into `cat`, keeping only the title `.zip` assets
 * (`<app>-title-<gen>.zip`). Returns the number of entries found. Pure; no allocation. */
int oopsy_parse_catalog(const char *json, oopsy_catalog_t *cat);

/* Build the on-device install directory for a title: "/data/homebrew/<TITLE_ID>". Returns the
 * length written, or -1 if `title_id` is not a 9-character title id or `buf` is too small. */
int oopsy_install_path(const char *title_id, char *buf, int buf_len);

/* Draw the current view into `surf`. Returns the number of text rows drawn. */
int oopsy_render(oops_surface_t *surf, const oopsy_view_t *v);

#endif /* OOPS_APPS_OOPSY_DAISY_H */
