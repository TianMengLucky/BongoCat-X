#ifndef BONGO_CAT_PREFERENCES_ABOUT_H
#define BONGO_CAT_PREFERENCES_ABOUT_H

#include "preferences_about_online.h"
#include "nuklear_config.h"

typedef struct BongoCatPreferences BongoCatPreferences;

/* UI-owned state. Workers publish results through the request objects. */
typedef struct BongoCatAboutState {
    /* Retained for this settings session, including GL asset rebuilds. */
    unsigned int portraits[BONGO_ABOUT_CONTRIBUTOR_CAP];
    BongoCatAboutRequest *contributors_request;
    BongoCatAboutFeed *contributors;
    bool contributors_attempted;
    bool portraits_loaded;
    int portraits_next;
    Uint32 event_type;
    struct nk_user_font detail_font;
    struct nk_user_font title_font, description_font;
    /* Stable addresses: Nuklear text commands retain font pointers until render. */
    struct nk_user_font css_fonts[64];
} BongoCatAboutState;

/* Preferences lifecycle and render-loop integration. */
/* Starts each request once; retries explicitly reset the corresponding flag. */
void bongo_cat_about_refresh(BongoCatPreferences *value);
void bongo_cat_about_overlays(BongoCatPreferences *value, struct nk_context *context);
void bongo_cat_about_clear(BongoCatPreferences *value, bool gl_ready);
void bongo_cat_about_assets_clear(BongoCatPreferences *value, bool gl_ready);
void bongo_cat_about_shutdown(BongoCatPreferences *value);
bool bongo_cat_about_event(BongoCatPreferences *value, const SDL_Event *event);

#endif
