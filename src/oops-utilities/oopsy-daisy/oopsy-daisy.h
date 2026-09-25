#ifndef OOPS_APPS_OOPSY_DAISY_H
#define OOPS_APPS_OOPSY_DAISY_H

#include "oops/draw.h"

/*
 * OOPSy-DAISY - Download Artificial Intelligence Slop Yourself.
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
    unsigned size;               /* asset size in bytes from the catalogue (0 if absent) */
} oopsy_entry_t;

typedef struct oopsy_catalog {
    oopsy_entry_t items[OOPSY_MAX_ENTRIES];
    int count;
} oopsy_catalog_t;

/* The catalogue-fetch state (not the per-title install state - that lives on each job below). */
typedef enum oopsy_phase {
    OOPSY_LOADING,     /* fetching the catalogue */
    OOPSY_BROWSE,      /* a list to pick from */
    OOPSY_FAILED       /* the fetch failed; message says what */
} oopsy_phase_t;

/* --- The download / install queue ---------------------------------------------------------
 * A title is not installed inline any more: pressing install adds a job to a queue, which is
 * worked one at a time. Each job carries its own state and progress so the queue screen can show
 * a row + a bar for every title, and a failure stops only its own job. `total`/`done` are bytes;
 * until the SDK's download call reports progress they stay 0 and the bar is driven from `state`. */
typedef enum oopsy_job_state {
    OOPSY_JOB_QUEUED,       /* waiting its turn */
    OOPSY_JOB_DOWNLOADING,  /* fetching the .zip */
    OOPSY_JOB_INSTALLING,   /* unpacking into /data/homebrew */
    OOPSY_JOB_DONE,         /* installed */
    OOPSY_JOB_FAILED        /* error; `error` says what */
} oopsy_job_state_t;

typedef struct oopsy_job {
    char name[OOPSY_NAME_LEN];
    char url[OOPSY_URL_LEN];
    oopsy_job_state_t state;
    unsigned total;         /* bytes to fetch (0 = unknown) */
    unsigned done;          /* bytes fetched so far */
    const char *error;      /* set when state == OOPSY_JOB_FAILED */
} oopsy_job_t;

#define OOPSY_MAX_JOBS 32
typedef struct oopsy_queue {
    oopsy_job_t jobs[OOPSY_MAX_JOBS];
    int count;
} oopsy_queue_t;

/* Which screen the view is showing. */
typedef enum oopsy_screen {
    OOPSY_SCREEN_BROWSE,   /* the catalogue list */
    OOPSY_SCREEN_QUEUE     /* the download queue with its progress bars */
} oopsy_screen_t;

typedef struct oopsy_view {
    const oopsy_catalog_t *cat;    /* may be NULL while loading */
    const oopsy_queue_t   *queue;  /* the download queue (may be NULL) */
    int selected;                  /* highlighted catalogue row */
    oopsy_phase_t phase;           /* catalogue-fetch state */
    oopsy_screen_t screen;         /* which screen is shown */
    const char *message;           /* status line, or NULL */
} oopsy_view_t;

/* Queue a title for install if it is not already in the queue (matched by name). Returns the job
 * index, or -1 if the queue is full or the title is already present. Pure. */
int oopsy_queue_add(oopsy_queue_t *q, const oopsy_entry_t *e);

/* The next job still to be worked (QUEUED, DOWNLOADING or INSTALLING), or -1 if the queue is idle.
 * Jobs are worked in order, so this is the one the controller should drive. Pure. */
int oopsy_queue_active(const oopsy_queue_t *q);

/* How many jobs are not yet finished (neither DONE nor FAILED) - for the "Downloads (N)" hint. */
int oopsy_queue_pending(const oopsy_queue_t *q);

/* A job's progress as 0..100, for its bar. Byte-accurate when `total` is known, otherwise a
 * coarse value from the state (queued 0, downloading 50, installing 90, done 100). Pure. */
int oopsy_job_pct(const oopsy_job_t *j);

/* Parse the GitHub "release by tag" JSON into `cat`, keeping only the title `.zip` assets
 * (`<app>-title-<gen>.zip`). Returns the number of entries found. Pure; no allocation. */
int oopsy_parse_catalog(const char *json, oopsy_catalog_t *cat);

/* Build the on-device install directory for a title: "/data/homebrew/<TITLE_ID>". Returns the
 * length written, or -1 if `title_id` is not a 9-character title id or `buf` is too small. */
int oopsy_install_path(const char *title_id, char *buf, int buf_len);

/* Draw the current view into `surf`. Returns the number of text rows drawn. */
int oopsy_render(oops_surface_t *surf, const oopsy_view_t *v);

/* --- The webview bridge (pure) ------------------------------------------------------------
 * The on-device UI is the oops-apps index page, rendered by the SDK webview. The page reads its
 * two pieces of dynamic state through zero-argument bridge functions the payload registers, each
 * of which returns one of these JSON strings; the payload then drives selection and repaints by
 * calling the page's own functions (oopsySelect / oopsyScreen / oopsyRefresh) via eval. The
 * serialisers are pure so the host self-test can pin their shape.
 */

/* Serialise the catalogue as the JSON array `__oopsy_catalog()` returns:
 * `[{"name":"neverball","size":123},...]`. The array index is the catalogue index the controller
 * installs when X is pressed. Returns the length written (excluding the NUL), or -1 if `cap` is
 * too small (the buffer is then left truncated but NUL-terminated where it stopped). Pure. */
int oopsy_catalog_json(const oopsy_catalog_t *cat, char *buf, int cap);

/* Serialise the queue as the JSON array `__oopsy_queue()` returns:
 * `[{"name":"neverball","state":"downloading","pct":42,"error":""},...]`. `state` is one of
 * queued/downloading/installing/done/failed; `pct` is oopsy_job_pct(); `error` is set only for a
 * failed job. Returns the length written, or -1 if `cap` is too small. Pure. */
int oopsy_queue_json(const oopsy_queue_t *q, char *buf, int cap);

/* The on-device UI markup: the oops-apps index page, litehtml-friendly (flex layout, no CSS grid)
 * and wired to the bridge above. Defined in oopsy-daisy_page.c; loaded with
 * oops_webview_load_html(). */
extern const char oopsy_page_html[];

#endif /* OOPS_APPS_OOPSY_DAISY_H */
