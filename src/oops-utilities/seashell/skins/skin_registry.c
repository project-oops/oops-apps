/*
 * Skin registry - the ordered list of skins; index 0 is the fallback for an unknown
 * index or id. Themes are the skins' themes, one per skin.
 */

#include "../skin.h"
#include "../home.h"

extern const home_skin_t g_skin_modern;
extern const home_skin_t g_skin_xmb;
extern const home_skin_t g_skin_blades;
extern const home_skin_t g_skin_list;
extern const home_skin_t g_skin_amber;
extern const home_skin_t g_skin_revolution;

static const home_skin_t *s_skins[] = {&g_skin_modern, &g_skin_xmb,
                                       &g_skin_blades, &g_skin_list,
                                       &g_skin_amber,  &g_skin_revolution};

int home_skin_count(void) {
    return (int)(sizeof(s_skins) / sizeof(s_skins[0]));
}

const home_skin_t *home_skin_at(int index) {
    if (index < 0 || index >= home_skin_count()) {
        return s_skins[0];
    }
    return s_skins[index];
}

const home_skin_t *home_skin_find(const char *id) {
    if (id == 0)
        return s_skins[0];
    int count = home_skin_count();
    for (int i = 0; i < count; i++) {
        const home_skin_t *s = s_skins[i];
        if (s->id == 0)
            continue;
        const char *a = s->id;
        const char *b = id;
        while (*a != '\0' && *b != '\0' && *a == *b) {
            a++;
            b++;
        }
        if (*a == '\0' && *b == '\0') {
            return s;
        }
    }
    return s_skins[0];
}

int home_theme_count(void) {
    return home_skin_count();
}

const home_theme_t *home_theme_at(int index) {
    return &home_skin_at(index)->theme;
}
