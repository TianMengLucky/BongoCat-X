#include "portable_mode.h"

#include "bongo_cat/file.h"
#include "bongo_cat/path.h"

#include <SDL3/SDL.h>
#include <string.h>
#ifndef _WIN32
#include <strings.h>
#endif

/* The user-facing switch lives in a small ini file beside the executable so
   that it can also be edited by hand before the first launch. The storage
   root must be known before settings.json can be located, which is why the
   portable flag cannot be a settings.json entry. An explicit --storage-root
   argument always wins over the ini file. */
#define BONGO_CAT_PORTABLE_INI "BongoCat.ini"
#define BONGO_CAT_INI_LIMIT 8192

static bool ini_path(char *path, size_t capacity) {
    /* SDL3 owns the cached SDL_GetBasePath() string; it must not be freed. */
    const char *base = SDL_GetBasePath();
    return base && base[0] &&
        bongo_cat_path_join(path, capacity, base, BONGO_CAT_PORTABLE_INI);
}

bool bongo_cat_portable_root(char *output, size_t capacity) {
    const char *base = SDL_GetBasePath();
    size_t length;
    if (!base || !base[0]) return false;
    length = strlen(base);
    /* SDL_GetBasePath() keeps a trailing separator; drop it so that joined
       child paths do not double up. */
    while (length && (base[length - 1] == '/' || base[length - 1] == '\\'))
        --length;
    if (!length || length >= capacity) return false;
    memcpy(output, base, length);
    output[length] = '\0';
    return true;
}

/* Read the ini into a NUL-terminated buffer; false when it cannot be read
   (missing counts as empty) or is too large to rewrite safely. */
static bool read_ini(const char *path, char *buffer, size_t capacity,
    bool *exists) {
    FILE *file = bongo_cat_file_open(path, "rb");
    long size;
    *exists = false;
    buffer[0] = '\0';
    if (!file) return true;
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0 || (size_t)size + 1 >= capacity ||
        fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        fclose(file);
        return false;
    }
    fclose(file);
    buffer[size] = '\0';
    *exists = true;
    return true;
}

/* Match a "portable = <0|1>" line; returns the value, or -1 when the line
   is something else. */
static int portable_line(const char *line) {
    const char *equals = strchr(line, '=');
    char key[16], value[16];
    size_t length;
    if (!equals || (size_t)(equals - line) >= sizeof(key)) return -1;
    length = (size_t)(equals - line);
    while (length && (line[length - 1] == ' ' || line[length - 1] == '\t'))
        --length;
    memcpy(key, line, length);
    key[length] = '\0';
    if (SDL_strcasecmp(key, "portable") != 0) return -1;
    equals++;
    while (*equals == ' ' || *equals == '\t') equals++;
    length = strlen(equals);
    while (length && (equals[length - 1] == ' ' || equals[length - 1] == '\t' ||
        equals[length - 1] == '\r' || equals[length - 1] == '\n'))
        --length;
    if (length >= sizeof(value)) return -1;
    memcpy(value, equals, length);
    value[length] = '\0';
    if (SDL_strcmp(value, "1") == 0 || SDL_strcasecmp(value, "true") == 0 ||
        SDL_strcasecmp(value, "yes") == 0)
        return 1;
    return 0;
}

bool bongo_cat_portable_mode_active(void) {
    char ini[BONGO_CAT_PATH_CAP], buffer[BONGO_CAT_INI_LIMIT];
    bool exists;
    const char *line;
    if (!ini_path(ini, sizeof(ini)) ||
        !read_ini(ini, buffer, sizeof(buffer), &exists))
        return false;
    line = buffer;
    if ((unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB &&
        (unsigned char)line[2] == 0xBF)
        line += 3; /* skip a UTF-8 BOM */
    while (line && *line) {
        int value = portable_line(line);
        if (value >= 0) return value == 1;
        line = strchr(line, '\n');
        if (line) line++;
    }
    return false;
}

static bool append_text(char *out, size_t capacity, size_t *used,
    const char *text, size_t length) {
    if (*used + length + 1 > capacity) return false;
    memcpy(out + *used, text, length);
    *used += length;
    out[*used] = '\0';
    return true;
}

/* Rewrite the ini with the portable line replaced or appended, preserving
   every other line byte for byte. */
static bool write_ini(const char *path, const char *current, bool exists,
    bool enable, BongoCatError *error) {
    char out[BONGO_CAT_INI_LIMIT], entry[32];
    size_t used = 0;
    const char *line = exists ? current : "";
    bool replaced = false;
    SDL_snprintf(entry, sizeof(entry), "portable = %d", enable ? 1 : 0);
    if (!exists) {
        /* A fresh file states what it is for. */
        static const char header[] = "; BongoCat launcher settings\r\n";
        if (!append_text(out, sizeof(out), &used, header,
                sizeof(header) - 1) ||
            !append_text(out, sizeof(out), &used, entry, strlen(entry)) ||
            !append_text(out, sizeof(out), &used, "\r\n", 2))
            return false;
    } else {
        out[0] = '\0';
        while (*line) {
            const char *newline = strchr(line, '\n');
            size_t content = strcspn(line, "\n");
            size_t total = newline ? (size_t)(newline - line) + 1 : content;
            if (!replaced && portable_line(line) >= 0) {
                bool crlf = content && line[content - 1] == '\r';
                if (!append_text(out, sizeof(out), &used, entry,
                        strlen(entry)) ||
                    !append_text(out, sizeof(out), &used,
                        crlf ? "\r\n" : "\n", crlf ? 2 : 1))
                    return false;
                replaced = true;
            } else if (!append_text(out, sizeof(out), &used, line, total)) {
                return false;
            }
            line += total;
        }
        if (!replaced) {
            if (used && out[used - 1] != '\n' &&
                !append_text(out, sizeof(out), &used, "\r\n", 2))
                return false;
            if (!append_text(out, sizeof(out), &used, entry, strlen(entry)) ||
                !append_text(out, sizeof(out), &used, "\r\n", 2))
                return false;
        }
    }
    {
        FILE *file = bongo_cat_file_open(path, "wb");
        bool ok = file && fwrite(out, 1, used, file) == used;
        if (file && fclose(file) != 0) ok = false;
        if (!ok) bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
            "Cannot write %s (is the executable directory writable?)", path);
        return ok;
    }
}

/* Merge-copy a whole storage tree (data / models / state) into the portable
   root. Existing files at the destination are KEPT (never overwritten): a
   re-toggle or a prior portable run must not lose data, and the managed
   <exe>/data/live2d tree this carries over must never fight with the
   <exe>/live2d drop-in folder (a separate directory that is never touched).
   Failures on individual files (e.g. a log the running app holds open) are
   counted and logged but do not abort the migration. */
typedef struct PortableMerge {
    const char *target_root;
    const char *skip_child; /* absolute path skipped at THIS level only */
    unsigned copied;
    unsigned skipped;
    unsigned failed;
} PortableMerge;

static bool merge_tree(const char *source, const char *target,
    PortableMerge *totals, const char *skip_child);

static BongoCatPathVisit merge_visitor(void *userdata, const char *directory,
    const char *name) {
    PortableMerge *stats = (PortableMerge *)userdata;
    char source[BONGO_CAT_PATH_CAP], target[BONGO_CAT_PATH_CAP];
    if (!bongo_cat_path_join(source, sizeof(source), directory, name) ||
        !bongo_cat_path_join(target, sizeof(target),
            stats->target_root, name))
        return BONGO_CAT_PATH_FAILURE;
    if (stats->skip_child && strcmp(source, stats->skip_child) == 0)
        return BONGO_CAT_PATH_CONTINUE; /* migrated as its own top-level tree */
    if (bongo_cat_path_is_dir(source)) {
        PortableMerge nested = { NULL, NULL, 0, 0, 0 };
        if (!merge_tree(source, target, &nested, NULL))
            return BONGO_CAT_PATH_FAILURE;
        stats->copied += nested.copied;
        stats->skipped += nested.skipped;
        stats->failed += nested.failed;
        return BONGO_CAT_PATH_CONTINUE;
    }
    if (bongo_cat_path_is_file(target)) {
        ++stats->skipped;
        return BONGO_CAT_PATH_CONTINUE;
    }
    if (bongo_cat_path_copy_file(source, target))
        ++stats->copied;
    else {
        ++stats->failed;
        SDL_LogWarn(SDL_LOG_CATEGORY_CUSTOM,
            "Portable migration: cannot copy %s", source);
    }
    return BONGO_CAT_PATH_CONTINUE;
}

static bool merge_tree(const char *source, const char *target,
    PortableMerge *totals, const char *skip_child) {
    PortableMerge level = { NULL, NULL, 0, 0, 0 };
    bool ok;
    if (!source || !source[0] || !bongo_cat_path_is_dir(source)) return true;
    /* Source and target coincide (e.g. already portable): nothing to move. */
    if (strcmp(source, target) == 0) return true;
    if (!bongo_cat_path_create_directory(target)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_CUSTOM,
            "Portable migration: cannot create %s", target);
        return false;
    }
    level.target_root = target;
    level.skip_child = skip_child;
    ok = bongo_cat_path_enumerate(source, merge_visitor, &level);
    totals->copied += level.copied;
    totals->skipped += level.skipped;
    totals->failed += level.failed;
    return ok;
}

/* Best-effort migration of one named storage tree below the portable root.
   exclude_dir (may be NULL) is the absolute path of a single top-level
   child that lives elsewhere in the portable layout (e.g. on Linux the
   models folder is inside the data root, but portable keeps models next to
   data) — skipping it prevents duplicating a potentially large tree. */
static void migrate_tree(const char *source_root, const char *portable_root,
    const char *name, const char *exclude_dir, PortableMerge *totals) {
    char target[BONGO_CAT_PATH_CAP];
    PortableMerge stats = { NULL, NULL, 0, 0, 0 };
    if (!source_root || !source_root[0]) return;
    if (!bongo_cat_path_join(target, sizeof(target), portable_root, name))
        return;
    (void)merge_tree(source_root, target, &stats, exclude_dir);
    totals->copied += stats.copied;
    totals->skipped += stats.skipped;
    totals->failed += stats.failed;
}

bool bongo_cat_portable_mode_set(bool enable, const BongoCatApp *app,
    BongoCatError *error) {
    char ini[BONGO_CAT_PATH_CAP], root[BONGO_CAT_PATH_CAP];
    char previous[BONGO_CAT_INI_LIMIT];
    bool exists = false;
    const char *settings_path = app ? app->settings_path : NULL;
    if (!app || !ini_path(ini, sizeof(ini)) ||
        !bongo_cat_portable_root(root, sizeof(root)) ||
        !read_ini(ini, previous, sizeof(previous), &exists)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
            "Cannot read %s", BONGO_CAT_PORTABLE_INI);
        return false;
    }
    if (!enable && !exists) return true; /* nothing to switch off */
    if (!write_ini(ini, previous, exists, enable, error)) {
        if (error && !error->code) bongo_cat_error_set(error,
            BONGO_CAT_ERROR_IO, "Cannot update %s",
            BONGO_CAT_PORTABLE_INI);
        return false;
    }
    if (!enable) return true;
    if (settings_path && settings_path[0] &&
        bongo_cat_path_is_file(settings_path)) {
        char config[BONGO_CAT_PATH_CAP], target[BONGO_CAT_PATH_CAP];
        bool copied;
        if (!bongo_cat_path_join(config, sizeof(config), root, "config") ||
            !bongo_cat_path_create_directory(config) ||
            !bongo_cat_path_join(target, sizeof(target), config,
                "settings.json")) {
            copied = false;
        } else if (bongo_cat_path_is_file(target)) {
            copied = true; /* keep a pre-existing portable config */
        } else {
            copied = bongo_cat_path_copy_file(settings_path, target);
        }
        if (!copied) {
            /* Roll the ini back so the switch does not lie. */
            if (exists) {
                FILE *file = bongo_cat_file_open(ini, "wb");
                if (file) {
                    if (fwrite(previous, 1, strlen(previous), file) !=
                        strlen(previous)) {
                        /* best effort; the write error below dominates */
                    }
                    fclose(file);
                }
            } else
                bongo_cat_file_remove(ini);
            bongo_cat_error_set(error, BONGO_CAT_ERROR_IO,
                "Cannot copy the settings into the executable directory");
            return false;
        }
    }
    /* Carry the existing storage trees over so the portable folder works
       standalone after the restart. This is a merge: files already present
       in the portable tree are kept, and individual copy failures (locked
       files) only warn. The managed <root>/data/live2d content is part of
       the data tree; the <exe>/live2d drop-in folder is never touched. */
    PortableMerge totals = { NULL, NULL, 0, 0, 0 };
    migrate_tree(app->data_root, root, "data", app->models_root, &totals);
    migrate_tree(app->models_root, root, "models", NULL, &totals);
    /* Primary state first; the active tree may be a secondary-pet subdir. */
    migrate_tree(app->primary_state_root[0] ? app->primary_state_root
            : app->state_root, root, "state", app->log_root, &totals);
    if (app->state_root[0] &&
        strcmp(app->state_root, app->primary_state_root) != 0)
        migrate_tree(app->state_root, root, "state", app->log_root, &totals);
    SDL_LogInfo(SDL_LOG_CATEGORY_CUSTOM,
        "Portable migration finished: %u copied, %u already present, %u failed",
        totals.copied, totals.skipped, totals.failed);
    return true;
}
