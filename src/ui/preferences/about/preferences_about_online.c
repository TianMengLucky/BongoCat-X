#include "preferences_about_online.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"
#include "bongo_cat/safe_http.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "bongo_cat/json_dom.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#define RESPONSE_LIMIT (4u * 1024u * 1024u)
#define CACHE_TTL_NS (SDL_NS_PER_SECOND * 86400LL)

/* Capacity excludes the terminator. Network growth is geometric and bounded;
   disk reads reserve their exact known size without touching a 4 MiB buffer. */
static bool reserve_response(BongoCatAboutRequest *job, size_t capacity) {
    if (capacity > job->limit) return false;
    if (job->response && capacity <= job->capacity) return true;
    char *response = realloc(job->response, capacity + 1);
    if (!response) return false;
    job->response = response;
    job->capacity = capacity;
    job->response[job->length] = 0;
    return true;
}

static bool cache_path(BongoCatAboutRequest *job, char *path, size_t capacity) {
    return job->cache_directory[0] && bongo_cat_path_join(path, capacity,
        job->cache_directory, "contributors-v1.svg");
}

/* SVG parsers modify their input. Only network results need a preserved copy
   for cache writes; disk reads can be parsed in place. */
static bool decode_response(BongoCatAboutRequest *job, bool preserve_response) {
    if (SDL_GetAtomicInt(&job->cancel) || !job->length ||
        memchr(job->response, 0, job->length) || !strstr(job->response, "<svg") ||
        strstr(job->response, "<!DOCTYPE") || strstr(job->response, "<!ENTITY"))
        return false;
    char *copy = preserve_response ? malloc(job->length + 1) : job->response;
    if (!copy) return false;
    if (preserve_response) memcpy(copy, job->response, job->length + 1);
    bool valid = false;
    job->feed = bongo_cat_about_feed_parse(copy, &job->cancel);
    valid = job->feed && job->feed->count;
    if (!valid) {
        bongo_cat_about_feed_free(job->feed);
        job->feed = NULL;
    }
    if (preserve_response) free(copy);
    return valid;
}

static bool cache_read(BongoCatAboutRequest *job) {
    char path[BONGO_CAT_PATH_CAP];
    if (!cache_path(job, path, sizeof(path))) return false;
    SDL_PathInfo info;
    if (!SDL_GetPathInfo(path, &info) || info.type != SDL_PATHTYPE_FILE ||
        !info.size || info.size > job->limit) return false;
    if (!reserve_response(job, (size_t)info.size)) return false;
    FILE *file = bongo_cat_file_open(path, "rb");
    if (!file) return false;
    size_t size = (size_t)info.size;
    bool ok = fread(job->response, 1, size, file) == size;
    if (fclose(file) != 0) ok = false;
    job->length = size;
    job->response[size] = 0;
    if (!ok || !decode_response(job, false)) {
        job->length = 0;
        job->response[0] = 0;
        return false;
    }
    SDL_Time now = 0;
    job->refresh_needed = !SDL_GetCurrentTime(&now) || info.modify_time <= 0 ||
        now < info.modify_time || now - info.modify_time >= CACHE_TTL_NS;
    job->status = 200;
    return true;
}

static void cache_write(BongoCatAboutRequest *job) {
    char path[BONGO_CAT_PATH_CAP], temporary[BONGO_CAT_PATH_CAP + 64];
    if (SDL_GetAtomicInt(&job->cancel) || !cache_path(job, path, sizeof(path)) ||
        !bongo_cat_path_create_directory(job->cache_directory)) return;
    /* A sibling temporary file preserves the previous cache on failed writes. */
    unsigned long long process_id;
#ifdef _WIN32
    process_id = (unsigned long long)GetCurrentProcessId();
#else
    process_id = (unsigned long long)getpid();
#endif
    int length = snprintf(temporary, sizeof(temporary), "%s.%llu.%llu.tmp", path,
        process_id,
        (unsigned long long)SDL_GetCurrentThreadID());
    if (length < 0 || (size_t)length >= sizeof(temporary)) return;
    FILE *file = bongo_cat_file_open(temporary, "wb");
    if (!file) return;
    bool ok = fwrite(job->response, 1, job->length, file) == job->length;
    if (fclose(file) != 0) ok = false;
    if (!ok || SDL_GetAtomicInt(&job->cancel) ||
        !bongo_cat_file_replace(temporary, path, false))
        bongo_cat_file_remove(temporary);
}

static int complete(BongoCatAboutRequest *job) {
    free(job->response);
    job->response = NULL;
    job->length = 0;
    job->capacity = 0;
    SDL_SetAtomicInt(&job->done, 1);
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = job->event_type;
    event.user.windowID = job->window_id;
    SDL_PushEvent(&event);
    return 0;
}

static bool append(BongoCatAboutRequest *job, const void *data, size_t size) {
    if (SDL_GetAtomicInt(&job->cancel) || size > job->limit - job->length)
        return false;
    size_t needed = job->length + size;
    if (!job->response || needed > job->capacity) {
        size_t capacity = job->capacity ? job->capacity : 4096;
        while (capacity < needed)
            capacity = capacity > job->limit / 2 ? job->limit : capacity * 2;
        if (!reserve_response(job, capacity)) return false;
    }
    memcpy(job->response + job->length, data, size);
    job->length += size;
    job->response[job->length] = 0;
    return true;
}

/* The contributor list comes from this fork's GitHub repository: the
   contributors API plus one avatar download per person. The worker assembles
   them into the same self-contained SVG feed the upstream site served, so
   the parser and the on-disk cache stay unchanged. */
static const char CONTRIBUTORS_HOST[] = "api.github.com";
static const char CONTRIBUTORS_PATH[] =
    "/repos/TianMengLucky/BongoCat-X/contributors?per_page=100";
static const char CONTRIBUTORS_SVG_HEADER[] =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" "
    "xmlns:xlink=\"http://www.w3.org/1999/xlink\">";

typedef struct fetch_buffer {
    char *data;
    size_t length, capacity;
} fetch_buffer;

static bool about_cancelled(void *userdata) {
    BongoCatAboutRequest *job = userdata;
    return SDL_GetAtomicInt(&job->cancel) != 0;
}

static bool http_get(bongo_safe_http_client *client, const char *host,
    const char *path, BongoCatAboutRequest *job, fetch_buffer *out) {
    char url[1024], message[256];
    int length = snprintf(url, sizeof(url), "https://%s%s", host, path);
    if (length < 0 || (size_t)length >= sizeof(url)) return false;
    unsigned char *data = NULL;
    int result = bongo_safe_http_get(client, url, job->accelerate_github,
        RESPONSE_LIMIT, about_cancelled, job, &data, &out->length,
        message, sizeof(message));
    if (result != BONGO_SAFE_HTTP_OK) return false;
    out->data = (char *)data;
    return true;
}

static bool svg_text_append(BongoCatAboutRequest *job, const char *text) {
    for (size_t i = 0; text[i]; ++i) {
        char token[8];
        const char *entity = NULL;
        switch (text[i]) {
            case '&': entity = "&amp;"; break;
            case '<': entity = "&lt;"; break;
            case '>': entity = "&gt;"; break;
            case '"': entity = "&quot;"; break;
            case '\'': entity = "&apos;"; break;
            default: break;
        }
        size_t length = entity ? strlen(entity) : 1;
        if (!entity) token[0] = text[i];
        else memcpy(token, entity, length);
        if (!append(job, token, length)) return false;
    }
    return true;
}

static bool base64_append(BongoCatAboutRequest *job, const char *data,
    size_t length) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char chunk[4096];
    size_t used = 0;
    for (size_t i = 0; i < length; i += 3) {
        unsigned value = (unsigned char)data[i] << 16;
        if (i + 1 < length) value |= (unsigned char)data[i + 1] << 8;
        if (i + 2 < length) value |= (unsigned char)data[i + 2];
        chunk[used++] = table[(value >> 18) & 63];
        chunk[used++] = table[(value >> 12) & 63];
        chunk[used++] = i + 1 < length ? table[(value >> 6) & 63] : '=';
        chunk[used++] = i + 2 < length ? table[value & 63] : '=';
        if (used == sizeof(chunk) && !append(job, chunk, used)) return false;
        if (used == sizeof(chunk)) used = 0;
    }
    return !used || append(job, chunk, used);
}

static bool split_https_url(const char *url, char *host, size_t host_cap,
    char *path, size_t path_cap) {
    if (strncmp(url, "https://", 8)) return false;
    url += 8;
    const char *separator = strchr(url, '/');
    if (!separator) return false;
    size_t host_length = (size_t)(separator - url);
    if (host_length >= host_cap || strlen(separator) >= path_cap) return false;
    memcpy(host, url, host_length);
    host[host_length] = 0;
    snprintf(path, path_cap, "%s", separator);
    return true;
}

static bool avatar_append(BongoCatAboutRequest *job, bongo_safe_http_client *client,
    const char *avatar_url) {
    char host[256], path[512];
    if (!split_https_url(avatar_url, host, sizeof(host), path, sizeof(path)))
        return false;
    if (strcmp(host, "avatars.githubusercontent.com") != 0) return false;
    /* Request the small variant so a full page of avatars stays well below
       the response limit. */
    size_t length = strlen(path);
    if (length + 6 >= sizeof(path)) return false;
    memcpy(path + length, strchr(path, '?') ? "&s=64" : "?s=64", 6);
    fetch_buffer image = {0};
    bool ok = http_get(client, host, path, job, &image) &&
        image.length &&
        append(job, "data:image/png;base64,", 22) &&
        base64_append(job, image.data, image.length);
    bongo_safe_free_http_response((unsigned char *)image.data, image.length);
    return ok;
}

static bool build_contributors_svg(BongoCatAboutRequest *job, bongo_safe_http_client *client) {
    fetch_buffer json = {0};
    if (!http_get(client, CONTRIBUTORS_HOST, CONTRIBUTORS_PATH,
            job, &json)) return false;
    BongoJsonDoc *doc = bongo_json_read(json.data, json.length, 0);
    bongo_safe_free_http_response((unsigned char *)json.data, json.length);
    BongoJsonValue *root = doc ? bongo_json_doc_get_root(doc) : NULL;
    if (!bongo_json_is_arr(root)) {
        bongo_json_doc_free(doc);
        return false;
    }
    bool ok = append(job, CONTRIBUTORS_SVG_HEADER,
        sizeof(CONTRIBUTORS_SVG_HEADER) - 1);
    BongoJsonArrIter iter = bongo_json_arr_iter_with(root);
    BongoJsonValue *item;
    int count = 0;
    while (ok && (item = bongo_json_arr_iter_next(&iter)) &&
           count < BONGO_ABOUT_CONTRIBUTOR_CAP &&
           !SDL_GetAtomicInt(&job->cancel)) {
        const char *login = bongo_json_get_str(bongo_json_obj_get(item, "login"));
        const char *profile = bongo_json_get_str(bongo_json_obj_get(item, "html_url"));
        const char *avatar = bongo_json_get_str(bongo_json_obj_get(item, "avatar_url"));
        if (!login || !avatar) continue;
        char fallback[128];
        if (!profile || strncmp(profile, "https://", 8)) {
            snprintf(fallback, sizeof(fallback), "https://github.com/%s", login);
            profile = fallback;
        }
        ok = append(job, "<a href=\"", 9) &&
             svg_text_append(job, profile) &&
             append(job, "\"><title>", 9) &&
             svg_text_append(job, login) &&
             append(job, "</title><image xlink:href=\"", 27) &&
             avatar_append(job, client, avatar) &&
             append(job, "\"/></a>", 7);
        if (ok) count++;
    }
    bongo_json_doc_free(doc);
    /* An empty page means the API answered but listed nobody; surface that
       as a failure so the UI shows its retry notice. */
    if (ok && !count) ok = false;
    return ok && append(job, "</svg>", 6) && !SDL_GetAtomicInt(&job->cancel);
}

static int SDLCALL request_worker(void *user) {
    BongoCatAboutRequest *job = user;
    /* Best effort: background assets should yield CPU time to interaction. */
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_LOW);
    /* Publish stale data before starting the separate background refresh. */
    if (!job->network_only && cache_read(job)) return complete(job);
    if (SDL_GetAtomicInt(&job->cancel)) return complete(job);
    bongo_safe_http_client *client = bongo_safe_http_new();
    bool ok = client && build_contributors_svg(job, client);
    bongo_safe_http_free(client);
    job->status = ok ? 200 : 0;
    if (job->status == 200) {
        if (decode_response(job, true)) cache_write(job);
        else job->status = 0;
    }
    return complete(job);
}

BongoCatAboutRequest *bongo_cat_about_request(Uint32 event_type,
    Uint32 window_id, const char *cache_root, bool network_only,
    bool accelerate_github) {
    BongoCatAboutRequest *job = calloc(1, sizeof(*job));
    if (!job)
        return NULL;
    job->limit = RESPONSE_LIMIT;
    job->event_type = event_type;
    job->window_id = window_id;
    job->network_only = network_only;
    job->accelerate_github = accelerate_github;
    if (cache_root && cache_root[0] &&
        !bongo_cat_path_join(job->cache_directory, sizeof(job->cache_directory),
            cache_root, "about")) job->cache_directory[0] = 0;
    job->thread = SDL_CreateThread(request_worker, "bongocat-about", job);
    if (!job->thread) {
        free(job->response);
        free(job);
        return NULL;
    }
    return job;
}

void bongo_cat_about_request_free(BongoCatAboutRequest *job) {
    if (!job)
        return;
    SDL_SetAtomicInt(&job->cancel, 1);
    if (job->thread) SDL_WaitThread(job->thread, NULL);
    bongo_cat_about_feed_free(job->feed);
    free(job->response);
    free(job);
}
