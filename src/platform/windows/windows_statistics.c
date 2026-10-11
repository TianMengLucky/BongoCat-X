#include "bongo_cat/statistics_usage_plugin.h"
#include <windows.h>
#include <string.h>
#include <wchar.h>

void bongo_cat_statistics_application_poll(void *context, BongoCatApplicationSample *sample) {
    (void)context;
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
    WCHAR *extension = wcsrchr(name, L'.');
    if (extension && !_wcsicmp(extension, L".exe")) *extension = 0;
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name, -1, sample->name,
        (int)sizeof(sample->name), NULL, NULL)) return;
    sample->active = 1;
}
