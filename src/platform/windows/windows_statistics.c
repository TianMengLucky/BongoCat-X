#include "bongo_cat/statistics_usage_plugin.h"
#include <windows.h>
#include <shellapi.h>
#include "bongo_cat/app.h"
#include "bongo_cat/statistics.h"
#include "bongo_cat/path.h"
#include <SDL3/SDL.h>
#include <string.h>
#include <wchar.h>

static void cache_icon(BongoCatApp *app, const WCHAR *executable, const char *id) {
    char target[BONGO_CAT_PATH_CAP], temporary[BONGO_CAT_PATH_CAP], directory[BONGO_CAT_PATH_CAP];
    if (!app || !bongo_cat_statistics_icon_path(app->data_root, id, target) ||
        bongo_cat_path_is_file(target) ||
        !bongo_cat_path_join(directory, sizeof(directory), app->data_root, "application-icons") ||
        !bongo_cat_path_create_directory(directory)) return;
    if (SDL_snprintf(temporary, sizeof(temporary), "%s.tmp", target) >= (int)sizeof(temporary)) return;
    HICON icon = NULL;
    if (!ExtractIconExW(executable, 0, &icon, NULL, 1) || !icon) return;
    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 48; info.bmiHeader.biHeight = -48;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *pixels = NULL;
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bitmap = dc ? CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, NULL, 0) : NULL;
    HGDIOBJ previous = bitmap ? SelectObject(dc, bitmap) : NULL;
    if (bitmap && previous && previous != HGDI_ERROR) {
        memset(pixels, 0, 48 * 48 * 4);
        if (DrawIconEx(dc, 0, 0, icon, 48, 48, 0, NULL, DI_NORMAL)) {
            GdiFlush();
            unsigned char *rgba = pixels;
            bool has_alpha = false;
            for (unsigned i = 0; i < 48 * 48; ++i) has_alpha |= rgba[i * 4 + 3] != 0;
            for (unsigned i = 0; i < 48 * 48; ++i) {
                unsigned char *pixel = rgba + i * 4;
                if (!has_alpha) pixel[3] = 255;
                else if (pixel[3]) {
                    for (unsigned c = 0; c < 3; ++c)
                        pixel[c] = (unsigned char)SDL_min(255u, (unsigned)pixel[c] * 255u / pixel[3]);
                }
            }
            SDL_Surface *surface = SDL_CreateSurfaceFrom(48, 48, SDL_PIXELFORMAT_BGRA32, pixels, 48 * 4);
            if (surface) {
                if (SDL_SaveBMP(surface, temporary)) bongo_cat_path_rename(temporary, target);
                SDL_DestroySurface(surface);
            }
        }
        SelectObject(dc, previous);
    }
    if (bitmap) DeleteObject(bitmap);
    if (dc) DeleteDC(dc);
    DestroyIcon(icon);
    bongo_cat_path_remove(temporary);
}

void bongo_cat_statistics_application_poll(void *context, BongoCatApplicationSample *sample) {
    BongoCatApp *app = context;
    *sample = (BongoCatApplicationSample){0};
    sample->available = 1;
    HDESK desktop = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!desktop) return;
    WCHAR desktop_name[64]; DWORD needed = 0;
    BOOL unlocked = GetUserObjectInformationW(desktop, UOI_NAME, desktop_name,
        sizeof(desktop_name), &needed) && !_wcsicmp(desktop_name, L"Default");
    CloseDesktop(desktop);
    if (!unlocked) return;
    LASTINPUTINFO input = {sizeof(input), 0};
    if (!GetLastInputInfo(&input)) return;
    sample->idle_ms = (DWORD)(GetTickCount() - input.dwTime);
    HWND window = GetForegroundWindow(); DWORD process_id = 0;
    if (!window || !GetWindowThreadProcessId(window, &process_id)) return;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (!process) return;
    WCHAR path[32768]; DWORD length = (DWORD)(sizeof(path) / sizeof(path[0]));
    BOOL found = QueryFullProcessImageNameW(process, 0, path, &length);
    CloseHandle(process);
    if (!found || length >= sizeof(path) / sizeof(path[0]) || window != GetForegroundWindow()) return;
    path[length] = 0;
    WCHAR *name = wcsrchr(path, L'\\'); name = name ? name + 1 : path;
    if (!_wcsicmp(name, L"LockApp.exe") || !_wcsicmp(name, L"LogonUI.exe")) return;
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name, -1, sample->id,
        (int)sizeof(sample->id), NULL, NULL)) return;
    for (char *p = sample->id; *p; ++p) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + ('a' - 'A'));
    cache_icon(app, path, sample->id);
    WCHAR *extension = wcsrchr(name, L'.');
    if (extension && !_wcsicmp(extension, L".exe")) *extension = 0;
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name, -1, sample->name,
        (int)sizeof(sample->name), NULL, NULL)) return;
    sample->active = 1;
}
