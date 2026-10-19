#include "statistics_internal.h"
#include "following_overlay.h"
#include "bongo_cat/safe_ffi.h"
void bongo_cat_statistics_overlay_destroy(BongoCatApp *app) {
    bongo_cat_following_overlay_destroy(&app->statistics_overlay);
}
bool bongo_cat_statistics_overlay_event(BongoCatApp *app, const SDL_Event *event) {
    return app && bongo_cat_following_overlay_event(app->statistics_overlay, event);
}
void bongo_cat_statistics_overlay_update(BongoCatApp *app) {
    if (!app) return;
    bool enabled = bongo_safe_statistics_overlay_get(app->settings.extensions_json,
        sizeof(app->settings.extensions_json));
    bongo_cat_following_overlay_update(app, &app->statistics_overlay,
        app->statistics ? app->statistics->overlay : NULL,
        app->statistics ? app->statistics->instance : NULL, enabled, false,
        &app->statistics_overlay_retry_ns);
}
