#include "mver_config_text.h"
#include "bongo_cat/utf8.h"
#include "bongo_cat/safe_ffi.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool collect_label(void *userdata, const char *field, size_t index, const char *label) {
    BongoCatMverLabels *labels = userdata;
    char normalized[BONGO_CAT_ID_CAP];
    if (!bongo_cat_utf8_normalize_mver(label, normalized, sizeof(normalized))) return true;
    if (labels->count >= BONGO_CAT_BEHAVIOR_LIMIT) return false;
    if (labels->count == labels->capacity) {
        size_t capacity = labels->capacity ? labels->capacity * 2 : 16;
        if (capacity > BONGO_CAT_BEHAVIOR_LIMIT) capacity = BONGO_CAT_BEHAVIOR_LIMIT;
        BongoCatMverLabelEntry *entries = realloc(labels->entries, capacity * sizeof(*entries));
        if (!entries) return false;
        labels->entries = entries; labels->capacity = capacity;
    }
    BongoCatMverLabelEntry *entry = &labels->entries[labels->count++];
    snprintf(entry->field, sizeof(entry->field), "%s", field);
    snprintf(entry->label, sizeof(entry->label), "%s", normalized);
    entry->index = index;
    return true;
}

void bongo_cat_mver_labels_clear(BongoCatMverLabels *labels) {
    if (!labels) return;
    free(labels->entries);
    *labels = (BongoCatMverLabels){0};
}

bool bongo_cat_mver_labels_load(const char *path, const char *mode,
    BongoCatMverLabels *labels) {
    if (!path || !mode || !labels) return false;
    bongo_cat_mver_labels_clear(labels);
    size_t length;
    char *text = mver_text_read(path, &length);
    if (!text) return false;
    bool found = bongo_safe_mver_labels(text, length, mode, collect_label, labels);
    free(text);
    if (!found) bongo_cat_mver_labels_clear(labels);
    return found;
}

const char *bongo_cat_mver_label(const BongoCatMverLabels *labels,
    const char *field, size_t index) {
    if (!labels || !field) return NULL;
    for (size_t i = 0; i < labels->count; ++i)
        if (labels->entries[i].index == index &&
            strcmp(labels->entries[i].field, field) == 0)
            return labels->entries[i].label;
    return NULL;
}

