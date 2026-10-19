#ifndef BONGO_CAT_UI_TOOLTIP_H
#define BONGO_CAT_UI_TOOLTIP_H

#include "nuklear_config.h"

void bongo_cat_ui_question_tooltip(struct nk_context *context,
    const char *question, const char *reply);
void bongo_cat_ui_text_tooltip(struct nk_context *context,
    const char *text, struct nk_rect anchor);

#endif
