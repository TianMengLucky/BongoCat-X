#include "windows_package.h"
#include "windows_utf8.h"

#ifdef _WIN32
#include "bongo_cat/common.h"

#include <limits.h>
#include <stdlib.h>
#include <windows.h>
#include <shlobj.h>

typedef LONG (WINAPI *BongoCatGetPackageName)(UINT32 *, PWSTR);

static BongoCatGetPackageName package_api(const char *name) {
    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    return kernel ? (BongoCatGetPackageName)(void *)GetProcAddress(kernel, name)
        : NULL;
}

bool bongo_cat_windows_is_packaged(void) {
    BongoCatGetPackageName get_name = package_api("GetCurrentPackageFullName");
    if (!get_name) return false;
    UINT32 length = 0;
    return get_name(&length, NULL) == ERROR_INSUFFICIENT_BUFFER;
}

static wchar_t *package_family(void) {
    BongoCatGetPackageName get_family =
        package_api("GetCurrentPackageFamilyName");
    if (!get_family) return NULL;
    UINT32 length = 0;
    if (get_family(&length, NULL) != ERROR_INSUFFICIENT_BUFFER || !length)
        return NULL;
    wchar_t *family = calloc(length, sizeof(*family));
    if (!family || get_family(&length, family) != ERROR_SUCCESS) {
        free(family);
        return NULL;
    }
    return family;
}

static bool user_profile(wchar_t output[BONGO_CAT_PATH_CAP]) {
    DWORD length = GetEnvironmentVariableW(L"USERPROFILE", output,
        BONGO_CAT_PATH_CAP);
    if (length > 0 && length < BONGO_CAT_PATH_CAP) return true;
    output[0] = L'\0';
    return SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, output));
}

bool bongo_cat_windows_package_storage_root_for(const wchar_t *profile,
    const wchar_t *family, char *output, size_t capacity) {
    if (!output || !capacity || capacity > INT_MAX) return false;
    output[0] = '\0';
    if (!profile || !profile[0] || !family || !family[0]) return false;
    wchar_t path[BONGO_CAT_PATH_CAP];
    int length = swprintf(path, BONGO_CAT_PATH_CAP,
        L"%ls\\AppData\\Local\\Packages\\%ls\\LocalCache\\Local\\%ls",
        profile, family, BONGO_CAT_STORAGE_NAME_W);
    return length > 0 && length < BONGO_CAT_PATH_CAP &&
        bongo_cat_windows_utf8(path, output, capacity);
}

bool bongo_cat_windows_package_storage_root(char *output, size_t capacity) {
    if (!output || !capacity) return false;
    output[0] = '\0';
    if (!bongo_cat_windows_is_packaged()) return false;
    wchar_t profile[BONGO_CAT_PATH_CAP] = {0};
    wchar_t *family = package_family();
    bool resolved = family && user_profile(profile) &&
        bongo_cat_windows_package_storage_root_for(profile, family,
            output, capacity);
    free(family);
    return resolved;
}
#endif
