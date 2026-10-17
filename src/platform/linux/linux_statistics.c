#include "bongo_cat/statistics_usage_plugin.h"
#include <SDL3/SDL.h>
#include "bongo_cat/app.h"
#include "bongo_cat/statistics.h"
#include "bongo_cat/path.h"
#ifdef BONGO_CAT_HAS_STATISTICS_XCB
#include <xcb/xcb.h>
#include <xcb/screensaver.h>
#include <stdlib.h>
#include <string.h>

static xcb_atom_t atom(xcb_connection_t *connection, const char *name) {
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection,
        xcb_intern_atom(connection, 1, (uint16_t)strlen(name), name), NULL);
    xcb_atom_t value = reply ? reply->atom : XCB_ATOM_NONE;
    free(reply); return value;
}
static void sample_connection(xcb_connection_t *connection, int screen_index, BongoCatApplicationSample *sample) {
    const xcb_query_extension_reply_t *extension = xcb_get_extension_data(connection, &xcb_screensaver_id);
    if (!extension || !extension->present) return;
    xcb_screen_iterator_t screens = xcb_setup_roots_iterator(xcb_get_setup(connection));
    for (int i = 0; i < screen_index && screens.rem; ++i) xcb_screen_next(&screens);
    if (!screens.rem) return;
    xcb_window_t root = screens.data->root;
    xcb_screensaver_query_info_reply_t *idle = xcb_screensaver_query_info_reply(connection,
        xcb_screensaver_query_info(connection, root), NULL);
    if (!idle) return;
    sample->available = 1; sample->idle_ms = idle->ms_since_user_input;
    bool active = idle->state == XCB_SCREENSAVER_STATE_OFF || idle->state == XCB_SCREENSAVER_STATE_DISABLED;
    free(idle);
    xcb_atom_t foreground = active ? atom(connection, "_NET_ACTIVE_WINDOW") : XCB_ATOM_NONE;
    if (!foreground) return;
    xcb_get_property_reply_t *reply = xcb_get_property_reply(connection,
        xcb_get_property(connection, 0, root, foreground, XCB_ATOM_WINDOW, 0, 1), NULL);
    xcb_window_t window = XCB_WINDOW_NONE;
    if (reply && reply->format == 32 && xcb_get_property_value_length(reply) == 4)
        memcpy(&window, xcb_get_property_value(reply), sizeof(window));
    free(reply);
    if (!window) return;
    reply = xcb_get_property_reply(connection,
        xcb_get_property(connection, 0, window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, 128), NULL);
    /* XCB reports a destroyed window as a missing reply; it cannot invoke a
       global Xlib error handler or terminate the desktop pet. */
    if (!reply) return;
    int length = xcb_get_property_value_length(reply);
    const unsigned char *bytes = xcb_get_property_value(reply);
    if (reply->format == 8 && length > 1 && length <= 512 && !reply->bytes_after) {
        const unsigned char *end = memchr(bytes, 0, (size_t)length);
        const unsigned char *id = end && end + 1 < bytes + length && end[1] ? end + 1 : bytes;
        size_t remaining = (size_t)((bytes + length) - id);
        const unsigned char *last = memchr(id, 0, remaining);
        size_t count = last ? (size_t)(last - id) : 0;
        bool valid = count > 0 && count < sizeof(sample->id);
        for (size_t i = 0; valid && i < count; ++i) if (id[i] >= 128) valid = false;
        if (valid) {
            memcpy(sample->id, id, count); memcpy(sample->name, id, count);
            sample->active = strcmp(sample->id, "i3lock") && strcmp(sample->id, "I3lock") &&
                strcmp(sample->id, "xscreensaver") && strcmp(sample->id, "XScreenSaver") &&
                strcmp(sample->id, "xsecurelock") && strcmp(sample->id, "light-locker");
        }
    }
    free(reply);
}
#endif
void bongo_cat_statistics_application_poll(void *context, BongoCatApplicationSample *sample) {
    (void)context;
    *sample = (BongoCatApplicationSample){0};
#ifdef BONGO_CAT_HAS_STATISTICS_XCB
    const char *wayland = SDL_getenv("WAYLAND_DISPLAY");
    if (wayland && wayland[0]) return;
    int screen_index = 0;
    xcb_connection_t *connection = xcb_connect(NULL, &screen_index);
    if (connection && !xcb_connection_has_error(connection)) sample_connection(connection, screen_index, sample);
    if (connection) xcb_disconnect(connection);
    BongoCatApp *app = context;
    char target[BONGO_CAT_PATH_CAP], directory[BONGO_CAT_PATH_CAP];
    if (app && sample->active && !strchr(sample->id, '/') && !strchr(sample->id, '\\') &&
        bongo_cat_statistics_icon_path(app->data_root, sample->id, target) &&
        !bongo_cat_path_is_file(target) &&
        bongo_cat_path_join(directory, sizeof(directory), app->data_root, "application-icons") &&
        bongo_cat_path_create_directory(directory)) {
        const char *roots[] = {"/usr/share/icons/hicolor/48x48/apps", "/usr/share/icons/hicolor/64x64/apps",
            "/usr/share/icons/hicolor/128x128/apps", "/usr/share/pixmaps", "/usr/local/share/pixmaps"};
        char name[272], source[BONGO_CAT_PATH_CAP], temporary[BONGO_CAT_PATH_CAP];
        SDL_snprintf(name, sizeof(name), "%s.png", sample->id);
        SDL_snprintf(temporary, sizeof(temporary), "%s.tmp", target);
        for (unsigned i = 0; i < SDL_arraysize(roots); ++i) {
            if (bongo_cat_path_join(source, sizeof(source), roots[i], name) &&
                bongo_cat_path_is_file(source) && bongo_cat_path_copy_file(source, temporary)) {
                bongo_cat_path_rename(temporary, target);
                break;
            }
        }
        bongo_cat_path_remove(temporary);
    }
#endif
}
