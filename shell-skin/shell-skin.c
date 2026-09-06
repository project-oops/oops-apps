#include "shell-skin.h"

/*
 * A bounded string sink, so composing a script can never overrun its buffer. On overflow it
 * stops and remembers - the caller returns 0 rather than a truncated script. No stdio, so this
 * compiles freestanding for the target as well as hosted for the test.
 */
typedef struct sink {
    char *out;
    size_t max;
    size_t len;
    int overflow;
} sink_t;

static void put(sink_t *s, const char *str) {
    while (*str) {
        if (s->len + 1 >= s->max) {
            s->overflow = 1;
            return;
        }
        s->out[s->len++] = *str++;
    }
}

/* A 0xRRGGBB colour as a `#rrggbb` CSS literal. */
static void put_hex_colour(sink_t *s, uint32_t rgb) {
    static const char digits[] = "0123456789abcdef";
    char buf[8];
    buf[0] = '#';
    for (int i = 0; i < 6; i++) {
        unsigned int nyb = (rgb >> ((5 - i) * 4)) & 0xF;
        buf[1 + i] = digits[nyb];
    }
    buf[7] = '\0';
    put(s, buf);
}

/*
 * The whole userscript is idempotent by construction: every element it adds carries a known id
 * and is skipped if already present, so re-applying after a shell restart does not stack copies.
 * That is the property that makes a re-apply-on-restart daemon safe.
 */
size_t shell_skin_compose(char *out, size_t max, unsigned int recipes, uint32_t background) {
    if (!out || max == 0) {
        return 0;
    }
    sink_t s = { out, max, 0, 0 };

    put(&s, "(function(){\n");
    put(&s, "  var add=function(id,css){\n");
    put(&s, "    if(document.getElementById(id))return;\n");
    put(&s, "    var e=document.createElement('div');\n");
    put(&s, "    e.id=id; e.style.cssText=css;\n");
    put(&s, "    document.body.appendChild(e);\n");
    put(&s, "  };\n");

    if (recipes & SHELL_SKIN_BACKGROUND) {
        /* A layer behind everything. Needs nothing about the shell's own markup, so it holds
         * whatever the DOM turns out to be. */
        put(&s, "  add('oops-skin-bg','position:fixed;inset:0;z-index:-1;"
                "pointer-events:none;background:");
        put_hex_colour(&s, background);
        put(&s, ";');\n");
    }

    if (recipes & SHELL_SKIN_BANNER) {
        put(&s, "  add('oops-skin-banner','position:fixed;right:12px;bottom:12px;z-index:9999;"
                "font:14px sans-serif;color:#8b9096;pointer-events:none;');\n");
        put(&s, "  var b=document.getElementById('oops-skin-banner');"
                "if(b)b.textContent='oops';\n");
    }

    if (recipes & SHELL_SKIN_HIDE_STORE) {
        /* HYPOTHESIS, not fact: the store tile's selector is unknown until the real SceShellUI
         * DOM is inspected on hardware. Written as a style rule guarded by a placeholder class
         * so it does nothing until the true selector is filled in, rather than guessing at a
         * class that might hide the wrong thing. See the README and obSCEne. */
        put(&s, "  var st=document.createElement('style'); st.id='oops-skin-hide-store';\n");
        put(&s, "  if(!document.getElementById('oops-skin-hide-store')){\n");
        put(&s, "    st.textContent='/* selector unconfirmed: .OOPS_STORE_TILE_TODO"
                "{display:none!important} */';\n");
        put(&s, "    document.head.appendChild(st);\n");
        put(&s, "  }\n");
    }

    put(&s, "})();\n");

    s.out[s.len] = '\0';
    return s.overflow ? 0 : s.len;
}
