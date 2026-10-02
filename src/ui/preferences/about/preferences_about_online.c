#include "preferences_about_online.h"
#include "preferences_about_svg.h"
#include "bongo_cat/file.h"
#include "bongo_cat/path.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <yyjson.h>
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#else
#include <curl/curl.h>
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

typedef struct fetch_buffer {
    char *data;
    size_t length, capacity;
} fetch_buffer;

static bool fetch_append(fetch_buffer *buffer, const void *data, size_t size) {
    if (size > RESPONSE_LIMIT - buffer->length) return false;
    size_t needed = buffer->length + size;
    if (needed > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 4096;
        while (capacity < needed) capacity *= 2;
        if (capacity > RESPONSE_LIMIT) capacity = RESPONSE_LIMIT;
        char *grown = realloc(buffer->data, capacity + 1);
        if (!grown) return false;
        buffer->data = grown;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->length, data, size);
    buffer->length = needed;
    buffer->data[buffer->length] = 0;
    return true;
}

#ifdef _WIN32
static wchar_t *wide_from(const char *text) {
    int size = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    wchar_t *wide = size > 0 ? (wchar_t *)malloc((size_t)size * sizeof(wchar_t)) : NULL;
    if (wide && MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, size) == 0) {
        free(wide);
        wide = NULL;
    }
    return wide;
}

/* One HTTPS GET; the session is reused across the JSON and avatar calls. */
static bool http_get(void *client, const char *host, const char *path,
    SDL_AtomicInt *cancel, fetch_buffer *out) {
    bool ok = false;
    wchar_t *wide_host = wide_from(host), *wide_path = wide_from(path);
    HINTERNET connection = wide_host ?
        WinHttpConnect(client, wide_host, INTERNET_DEFAULT_HTTPS_PORT, 0) : NULL;
    if (connection && wide_path) {
        HINTERNET request = WinHttpOpenRequest(connection, L"GET", wide_path,
            NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);
        if (request) {
            DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
            WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY,
                &redirect, sizeof(redirect));
            ok = !SDL_GetAtomicInt(cancel) &&
                 WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                 WinHttpReceiveResponse(request, NULL);
            DWORD status = 0, bytes = sizeof(status);
            if (ok)
                ok = WinHttpQueryHeaders(request,
                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    NULL, &status, &bytes, NULL) != FALSE && status == 200;
            Uint64 deadline = SDL_GetTicks() + 15000;
            while (ok && !SDL_GetAtomicInt(cancel)) {
                char chunk[4096];
                DWORD read = 0;
                ok = SDL_GetTicks() < deadline &&
                     WinHttpReadData(request, chunk, sizeof(chunk), &read);
                if (!ok || !read) break;
                ok = fetch_append(out, chunk, read);
            }
            WinHttpCloseHandle(request);
        }
        WinHttpCloseHandle(connection);
    }
    free(wide_host);
    free(wide_path);
    return ok && !SDL_GetAtomicInt(cancel);
}
#else
struct curl_fetch {
    fetch_buffer *buffer;
    SDL_AtomicInt *cancel;
};

static size_t curl_receive(char *data, size_t size, size_t count, void *user) {
    struct curl_fetch *fetch = user;
    if (SDL_GetAtomicInt(fetch->cancel)) return 0;
    return fetch_append(fetch->buffer, data, size * count) ? size * count : 0;
}

static int curl_progress(void *user, curl_off_t a, curl_off_t b,
    curl_off_t c, curl_off_t d) {
    (void)a; (void)b; (void)c; (void)d;
    return SDL_GetAtomicInt((SDL_AtomicInt *)user);
}

static bool http_get(void *client, const char *host, const char *path,
    SDL_AtomicInt *cancel, fetch_buffer *out) {
    char url[1024];
    if (snprintf(url, sizeof(url), "https://%s%s", host, path) < 0) return false;
    struct curl_fetch fetch = {out, cancel};
    curl_easy_setopt(client, CURLOPT_URL, url);
    curl_easy_setopt(client, CURLOPT_USERAGENT, "BongoCat About/1.0");
    curl_easy_setopt(client, CURLOPT_CONNECTTIMEOUT_MS, 3000L);
    curl_easy_setopt(client, CURLOPT_TIMEOUT_MS, 15000L);
    curl_easy_setopt(client, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(client, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(client, CURLOPT_WRITEFUNCTION, curl_receive);
    curl_easy_setopt(client, CURLOPT_WRITEDATA, &fetch);
    curl_easy_setopt(client, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(client, CURLOPT_XFERINFOFUNCTION, curl_progress);
    curl_easy_setopt(client, CURLOPT_XFERINFODATA, cancel);
    return curl_easy_perform(client) == CURLE_OK && out->length;
}
#endif

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

static bool avatar_append(BongoCatAboutRequest *job, void *client,
    const char *avatar_url) {
    char host[256], path[512];
    if (!split_https_url(avatar_url, host, sizeof(host), path, sizeof(path)))
        return false;
    /* Request the small variant so a full page of avatars stays well below
       the response limit. */
    size_t length = strlen(path);
    if (length + 6 >= sizeof(path)) return false;
    memcpy(path + length, strchr(path, '?') ? "&s=64" : "?s=64", 6);
    fetch_buffer image = {0};
    bool ok = http_get(client, host, path, &job->cancel, &image) &&
        image.length &&
        append(job, "data:image/png;base64,", 22) &&
        base64_append(job, image.data, image.length);
    free(image.data);
    return ok;
}

static bool build_contributors_svg(BongoCatAboutRequest *job, void *client) {
    fetch_buffer json = {0};
    if (!http_get(client, CONTRIBUTORS_HOST, CONTRIBUTORS_PATH,
            &job->cancel, &json)) return false;
    yyjson_doc *doc = yyjson_read(json.data, json.length, 0);
    free(json.data);
    yyjson_val *root = doc ? yyjson_doc_get_root(doc) : NULL;
    if (!yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        return false;
    }
    bool ok = append(job,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "xmlns:xlink=\"http://www.w3.org/1999/xlink\">", 96);
    yyjson_arr_iter iter = yyjson_arr_iter_with(root);
    yyjson_val *item;
    int count = 0;
    while (ok && (item = yyjson_arr_iter_next(&iter)) &&
           count < BONGO_ABOUT_CONTRIBUTOR_CAP &&
           !SDL_GetAtomicInt(&job->cancel)) {
        const char *login = yyjson_get_str(yyjson_obj_get(item, "login"));
        const char *profile = yyjson_get_str(yyjson_obj_get(item, "html_url"));
        const char *avatar = yyjson_get_str(yyjson_obj_get(item, "avatar_url"));
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
    yyjson_doc_free(doc);
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
    bool ok = false;
#ifdef _WIN32
    HINTERNET session = WinHttpOpen(L"BongoCat About/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session) {
        WinHttpSetTimeouts(session, 3000, 3000, 4000, 4000);
        ok = build_contributors_svg(job, session);
        WinHttpCloseHandle(session);
    }
#else
    bool initialized = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
    CURL *curl = initialized ? curl_easy_init() : NULL;
    if (curl) {
        ok = build_contributors_svg(job, curl);
        curl_easy_cleanup(curl);
    }
    /* curl is shared with the update service; process lifetime owns its global state. */
#endif
    job->status = ok ? 200 : 0;
    if (job->status == 200) {
        if (decode_response(job, true)) cache_write(job);
        else job->status = 0;
    }
    return complete(job);
}

BongoCatAboutRequest *bongo_cat_about_request(Uint32 event_type,
    Uint32 window_id, const char *cache_root, bool network_only) {
    BongoCatAboutRequest *job = calloc(1, sizeof(*job));
    if (!job)
        return NULL;
    job->limit = RESPONSE_LIMIT;
    job->event_type = event_type;
    job->window_id = window_id;
    job->network_only = network_only;
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
