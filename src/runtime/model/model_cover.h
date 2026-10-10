#ifndef BONGO_CAT_MODEL_COVER_H
#define BONGO_CAT_MODEL_COVER_H

#include "bongo_cat/app.h"
#include "model_cover_paths.h"

/* Pending cover queue and retry policy. */
void bongo_cat_model_cover_schedule(BongoCatApp *app,
    const BongoCatModelEntry *entry);
/* Refresh the active model after its expression or motion changes. */
void bongo_cat_model_cover_refresh(BongoCatApp *app);
bool bongo_cat_model_cover_pending(const BongoCatApp *app);
bool bongo_cat_model_cover_capture_due(const BongoCatApp *app, uint64_t now);
const char *bongo_cat_model_cover_pending_path(const BongoCatApp *app);
void bongo_cat_model_cover_defer(BongoCatApp *app, const char *reason);
void bongo_cat_model_cover_finish(BongoCatApp *app);

/* Capture workflow and framebuffer readback. */
void bongo_cat_model_cover_capture_before_switch(BongoCatApp *app);
bool bongo_cat_model_cover_capture(BongoCatApp *app, int width, int height);

#endif
