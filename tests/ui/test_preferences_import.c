#include "preferences_import_internal.h"
#include "preferences_state.h"
#include "preferences_notice.h"
#include "preferences_model_card.h"
#include "bongo_cat/utf8.h"
#include "bongo_cat/i18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    failures++; \
} } while (0)

static void summary_messages(BongoCatApp *app) {
    BongoCatImportSummary summary = {0};
    BongoCatImportJob *job = calloc(1, sizeof(*job));
    CHECK(job != NULL);
    if (!job) return;
    job->package_id_count = 2;
    strcpy(job->package_ids[0], "Cat A");
    strcpy(job->package_ids[1], "Cat B");
    job->package_imported[0] = job->package_imported[1] = true;
    bongo_cat_preferences_import_merge(&summary, job);
    char message[1024];
    bongo_cat_preferences_import_message(app, &summary, message, sizeof(message));
    CHECK(!strcmp(message, "Imported 2 model(s): Cat A, Cat B"));
    /* Repeated drops of the same packages do not inflate totals or undo
       an earlier successful installation in the current queue. */
    job->package_imported[0] = job->package_imported[1] = false;
    bongo_cat_preferences_import_merge(&summary, job);
    CHECK(summary.count == 2 && summary.imported[0] && summary.imported[1]);
    job->package_id_count = 1;
    strcpy(job->package_ids[0], "Existing Cat");
    bongo_cat_preferences_import_merge(&summary, job);
    job->package_id_count = 0;
    job->failed_count = 4;
    job->failed_name_count = 1;
    strcpy(job->failed_names[0], "broken.zip");
    job->error.code = BONGO_CAT_ERROR_FORMAT;
    bongo_cat_preferences_import_merge(&summary, job);
    bongo_cat_preferences_import_message(app, &summary, message, sizeof(message));
    CHECK(strstr(message, "Imported 2 model(s): Cat A, Cat B") != NULL);
    CHECK(strstr(message, "1 model(s) already exist: Existing Cat") != NULL);
    CHECK(strstr(message, "Failed to import 4 model(s): broken.zip and 3 more") != NULL);
    CHECK(strstr(message, "No supported model") != NULL);
    memset(&summary, 0, sizeof(summary));
    summary.count = 6;
    for (size_t i = 0; i < summary.count; ++i) {
        summary.imported[i] = true;
        for (size_t j = 0; j < 120; j += 3)
            memcpy(summary.ids[i] + j, "\xe7\x8c\xab", 3);
    }
    bongo_cat_preferences_import_message(app, &summary, message, sizeof(message));
    CHECK(strstr(message, "Imported 6 model(s):") != NULL);
    CHECK(strstr(message, " and 3 more") != NULL);
    CHECK(strstr(message, "...") != NULL && bongo_cat_utf8_valid(message));
    char small[28];
    bongo_cat_preferences_import_message(app, &summary, small, sizeof(small));
    CHECK(bongo_cat_utf8_valid(small));
    app->models.count = 1;
    strcpy(app->models.entries[0].id, summary.ids[0]);
    strcpy(app->models.entries[0].display_name, "Display Name");
    summary.count = 1;
    bongo_cat_preferences_import_message(app, &summary, message, sizeof(message));
    CHECK(!strcmp(message, "Imported 1 model(s): Display Name"));
    app->models.count = 0;
    BongoCatError error = {0};
    app->i18n = bongo_cat_i18n_create(BONGO_CAT_NATIVE_SOURCE_DIR
        "/resources/assets/locales", BONGO_CAT_LANG_ZH_CN, &error);
    CHECK(app->i18n != NULL);
    strcpy(summary.ids[0], "\xe6\xb5\x8b\xe8\xaf\x95\xe7\x8c\xab");
    bongo_cat_preferences_import_message(app, &summary, message, sizeof(message));
    CHECK(!strcmp(message, "\xe5\xaf\xbc\xe5\x85\xa5\xe6\x88\x90\xe5\x8a\x9f 1 \xe4\xb8\xaa\xe6\xa8\xa1\xe5\x9e\x8b\xef\xbc\x9a\xe6\xb5\x8b\xe8\xaf\x95\xe7\x8c\xab"));
    bongo_cat_i18n_destroy(app->i18n);
    app->i18n = NULL;
    free(job);
}

static void receipt_packages(void) {
    BongoCatImportDialog *dialog = bongo_cat_preferences_import_create();
    BongoCatImportJob *job = SDL_calloc(1, sizeof(*job));
    CHECK(dialog && job);
    if (dialog && job) {
        job->dialog = dialog;
        BongoCatImportProgressContext progress = {job, 0};
        BongoCatImportReceipt receipt = {0};
        receipt.count = receipt.installed_count = 2;
        strcpy(receipt.ids[0], "Multi Mode Cat");
        strcpy(receipt.ids[1], "Multi Mode Cat~2");
        bongo_cat_preferences_import_receive(&progress, &receipt);
        receipt.count = 1;
        receipt.installed_count = 0;
        strcpy(receipt.ids[0], "Multi Mode Cat~2");
        bongo_cat_preferences_import_receive(&progress, &receipt);
        CHECK(job->package_id_count == 1);
        CHECK(job->package_imported[0]);
        CHECK(!strcmp(job->package_ids[0], "Multi Mode Cat"));
        CHECK(job->resolved_count == 3 && job->installed_count == 2);
    }
    bongo_cat_preferences_import_job_free(job);
    bongo_cat_preferences_import_destroy(dialog);
}

static BongoCatImportJob *completed_job(BongoCatImportDialog *dialog,
    const char *name) {
    BongoCatImportJob *job = SDL_calloc(1, sizeof(*job));
    if (!job) return NULL;
    job->dialog = dialog;
    job->package_id_count = 1;
    job->package_imported[0] = true;
    SDL_strlcpy(job->package_ids[0], name, BONGO_CAT_ID_CAP);
    dialog->worker_job = job;
    dialog->busy = true;
    dialog->references++;
    return job;
}

static void queued_completion(BongoCatApp *app) {
    CHECK(SDL_InitSubSystem(SDL_INIT_EVENTS));
    receipt_packages();
    BongoCatPreferences *preferences = calloc(1, sizeof(*preferences));
    BongoCatImportDialog *dialog = bongo_cat_preferences_import_create();
    CHECK(preferences && dialog);
    if (!preferences || !dialog) {
        free(preferences);
        bongo_cat_preferences_import_destroy(dialog);
        SDL_QuitSubSystem(SDL_INIT_EVENTS);
        return;
    }
    app->preferences = preferences;
    preferences->app = app;
    preferences->import_dialog = dialog;
    BongoCatImportJob *first = completed_job(dialog, "First Cat");
    BongoCatImportJob *pending = SDL_calloc(1, sizeof(*pending));
    CHECK(first && pending);
    if (first && pending) {
        /* No models root: starting the queued worker fails deterministically.
           Its error must be included in the same notification as First Cat. */
        pending->count = 1;
        pending->paths = SDL_calloc(1, sizeof(*pending->paths));
        CHECK(pending->paths != NULL);
        if (pending->paths) {
            pending->paths[0] = SDL_strdup("queued.zip");
            dialog->pending_head = dialog->pending_tail = pending;
            SDL_Event event = {0};
            event.type = dialog->event_type;
            event.user.code = BONGO_CAT_IMPORT_COMPLETE_CODE;
            event.user.data1 = first;
            event.user.data2 = dialog;
            CHECK(bongo_cat_preferences_import_event(dialog, app, &event));
            CHECK(strstr(preferences->notices[0].message, "First Cat") != NULL);
            CHECK(strstr(preferences->notices[0].message, "queued.zip") != NULL);
            CHECK(preferences->notices[0].error);
            CHECK(!dialog->summary.count && !dialog->summary.failed_count);
            CHECK(!dialog->pending_head && !dialog->pending_tail && !dialog->busy);
            event.user.data1 = completed_job(dialog, "Next Cat");
            CHECK(event.user.data1 != NULL);
            CHECK(bongo_cat_preferences_import_event(dialog, app, &event));
            CHECK(!strcmp(preferences->notices[1].message,
                "Imported 1 model(s): Next Cat"));
            CHECK(!preferences->notices[1].error);
        }
    }
    bongo_cat_preferences_import_destroy(dialog);
    app->preferences = NULL;
    free(preferences);
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
}

static void dismiss_notices(void) {
    BongoCatPreferences *value = calloc(1, sizeof(*value));
    CHECK(value != NULL);
    if (!value) return;
    value->ui.layout_scale = 2.0f;
    BongoCatPreferenceNotice *notice = &value->notices[0];
    strcpy(notice->message, "Closable notification");
    notice->bounds = nk_rect(100, 20, 200, 50);
    notice->close_bounds = nk_rect(270, 27, 24, 24);
    SDL_Event event = {0};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 560; event.button.y = 70;
    CHECK(bongo_cat_preferences_notice_event(value, &event));
    CHECK(value->notice_pointer_captured && !notice->closing_ns);
    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    CHECK(bongo_cat_preferences_notice_event(value, &event));
    CHECK(!value->notice_pointer_captured && notice->closing_ns);
    // Releasing outside consumes the click without dismissing the toast.
    notice->closing_ns = 0;
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    CHECK(bongo_cat_preferences_notice_event(value, &event));
    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.x = 20;
    CHECK(bongo_cat_preferences_notice_event(value, &event));
    CHECK(!value->notice_pointer_captured && !notice->closing_ns);
    CHECK(!bongo_cat_preferences_notice_event(value, &event));
    free(value);
}

static void grouped_modes(BongoCatApp *app) {
    BongoCatPreferences *value = calloc(1, sizeof(*value));
    CHECK(value != NULL);
    if (!value) return;
    value->app = app;
    app->models.count = 3;
    for (size_t i = 0; i < 3; ++i) {
        snprintf(app->models.entries[i].id, BONGO_CAT_ID_CAP, "mode-%zu", i);
        strcpy(app->models.entries[i].family_id, i < 2 ? "cat" : "other-cat");
    }
    BongoCatModelEntry *first = &app->models.entries[0];
    BongoCatModelEntry *second = &app->models.entries[1];
    CHECK(bongo_cat_preferences_model_same_family(first, second));
    CHECK(!bongo_cat_preferences_model_same_family(first, &app->models.entries[2]));
    strcpy(app->session.active_model_id, second->id);
    CHECK(bongo_cat_preferences_model_group_entry(value, first, false) == second);
    CHECK(bongo_cat_preferences_model_group_entry(value, second, false) == NULL);
    value->model_selection_pending = true;
    strcpy(value->pending_model_id, first->id);
    CHECK(bongo_cat_preferences_model_group_entry(value, first, false) == first);
    value->model_selection_pending = false;
    bongo_cat_settings_set_model_hidden(&app->settings, first->id, true);
    CHECK(bongo_cat_preferences_model_group_entry(value, second, false) == second);
    bongo_cat_settings_set_model_hidden(&app->settings, first->id, false);
    second->managed = true;
    CHECK(!bongo_cat_preferences_model_same_family(first, second));
    second->managed = false;
    first->family_id[0] = second->family_id[0] = '\0';
    strcpy(first->id, "standard");
    strcpy(second->id, "keyboard");
    first->preset = second->preset = true;
    CHECK(bongo_cat_preferences_model_same_family(first, second));
    second->preset = false;
    CHECK(!bongo_cat_preferences_model_same_family(first, second));
    bongo_cat_models_init(&app->models);
    app->session.active_model_id[0] = '\0';
    free(value);
}

static void fallback_cleanup(BongoCatApp *app) {
    CHECK(SDL_InitSubSystem(SDL_INIT_EVENTS));
    BongoCatPreferences *value = calloc(1, sizeof(*value));
    BongoCatImportDialog *dialog = bongo_cat_preferences_import_create();
    CHECK(value && dialog);
    if (!value || !dialog) {
        free(value);
        bongo_cat_preferences_import_destroy(dialog);
        SDL_QuitSubSystem(SDL_INIT_EVENTS);
        return;
    }
    value->app = app;
    value->import_dialog = dialog;
    app->preferences = value;
    BongoCatImportJob *job = completed_job(dialog, "Fallback Cat");
    CHECK(job != NULL);
    if (job) {
        dialog->worker_generation = job->generation = 2;
        BongoCatImportProgress *stale = SDL_calloc(1, sizeof(*stale));
        CHECK(stale != NULL);
        if (stale) {
            stale->job = job;
            stale->generation = 1; // Emulate a recycled task address.
            stale->installed_count = 1;
            strcpy(stale->package_id, "Fallback Cat");
            SDL_Event event = {0};
            event.user.code = BONGO_CAT_IMPORT_PROGRESS_CODE;
            event.user.data1 = stale;
            CHECK(bongo_cat_preferences_import_progress_event(dialog, app, &event));
            CHECK(job->restored_count == 0 && !job->package_refresh_requested[0]);
        }
        dialog->completion_pending = true;
        bongo_cat_preferences_poll_imports(value);
        CHECK(!dialog->worker_job && !dialog->completion_pending && !dialog->busy);
        CHECK(dialog->references == 1);
        CHECK(strstr(value->notices[0].message, "Fallback Cat") != NULL);
    }
    job = completed_job(dialog, "Closing Cat");
    CHECK(job != NULL);
    if (job) {
        dialog->completion_pending = true;
        ++dialog->references; // Retain an observer to inspect destruction cleanup.
        bongo_cat_preferences_stop_imports(value);
        CHECK(!dialog->active);
        bongo_cat_preferences_import_destroy(dialog);
        CHECK(dialog->references == 1 && !dialog->worker_job && !dialog->pending_head);
        bongo_cat_preferences_import_dialog_release(dialog);
    } else bongo_cat_preferences_import_destroy(dialog);
    app->preferences = NULL;
    free(value);
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
}

int test_preferences_import(void) {
    BongoCatApp *app = calloc(1, sizeof(*app));
    CHECK(app != NULL);
    if (!app) return failures;
    grouped_modes(app);
    fallback_cleanup(app);
    dismiss_notices();
    summary_messages(app);
    queued_completion(app);
    free(app);
    return failures;
}
