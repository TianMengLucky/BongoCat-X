#include "bongo_cat/config.h"
#include "bongo_cat/utf8.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

float bongo_cat_settings_motion_volume(const BongoCatSettings *settings,
    const char *behavior_id) {
    if (!settings || !behavior_id) return 100.0f;
    size_t count = settings->motion_volume_count;
    if (count > BONGO_CAT_BEHAVIOR_BINDING_CAP) count = BONGO_CAT_BEHAVIOR_BINDING_CAP;
    for (size_t i = 0; i < count; ++i)
        if (!strcmp(settings->motion_volumes[i].id, behavior_id))
            return settings->motion_volumes[i].percent;
    return 100.0f;
}

bool bongo_cat_settings_set_motion_volume(BongoCatSettings *settings,
    const char *behavior_id, float percent) {
    if (!settings || !behavior_id || !behavior_id[0] ||
        strlen(behavior_id) >= BONGO_CAT_BEHAVIOR_ID_CAP ||
        !bongo_cat_utf8_valid(behavior_id) || !isfinite(percent) ||
        settings->motion_volume_count > BONGO_CAT_BEHAVIOR_BINDING_CAP) return false;
    percent = fmaxf(0.0f, fminf(100.0f, percent));
    size_t count = settings->motion_volume_count, index = 0;
    while (index < count && strcmp(settings->motion_volumes[index].id, behavior_id))
        ++index;
    if (percent == 100.0f) {
        if (index < count) {
            memmove(&settings->motion_volumes[index], &settings->motion_volumes[index + 1],
                (count - index - 1) * sizeof(settings->motion_volumes[0]));
            memset(&settings->motion_volumes[--settings->motion_volume_count], 0,
                sizeof(settings->motion_volumes[0]));
        }
        return true;
    }
    if (index == count) {
        if (count == BONGO_CAT_BEHAVIOR_BINDING_CAP) return false;
        BongoCatMotionVolume *entry = &settings->motion_volumes[count];
        memset(entry, 0, sizeof(*entry));
        snprintf(entry->id, sizeof(entry->id), "%s", behavior_id);
        settings->motion_volume_count++;
    }
    settings->motion_volumes[index].percent = percent;
    return true;
}

void bongo_cat_settings_validate_motion_volumes(BongoCatSettings *settings) {
    if (!settings) return;
    size_t count = settings->motion_volume_count, output = 0;
    if (count > BONGO_CAT_BEHAVIOR_BINDING_CAP) count = BONGO_CAT_BEHAVIOR_BINDING_CAP;
    for (size_t i = 0; i < count; ++i) {
        BongoCatMotionVolume entry = settings->motion_volumes[i];
        entry.id[sizeof(entry.id) - 1] = '\0';
        if (!entry.id[0] || !bongo_cat_utf8_valid(entry.id) ||
            !isfinite(entry.percent)) continue;
        entry.percent = fmaxf(0.0f, fminf(100.0f, entry.percent));
        if (entry.percent == 100.0f) continue;
        size_t existing = 0;
        while (existing < output && strcmp(settings->motion_volumes[existing].id, entry.id))
            ++existing;
        if (existing == output) {
            BongoCatMotionVolume canonical = {0};
            snprintf(canonical.id, sizeof(canonical.id), "%s", entry.id);
            settings->motion_volumes[output++] = canonical;
        }
        settings->motion_volumes[existing].percent = entry.percent;
    }
    memset(&settings->motion_volumes[output], 0,
        (BONGO_CAT_BEHAVIOR_BINDING_CAP - output) * sizeof(settings->motion_volumes[0]));
    settings->motion_volume_count = output;
}
