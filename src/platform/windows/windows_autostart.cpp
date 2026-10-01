/* Launch-at-login lives in the registry Run key, the mechanism Task Manager
   exposes as startup apps. Registry entries always launch with the user's own
   rights, so configuring one never needs elevation. Leftovers of the earlier
   Startup-folder shortcut and Task Scheduler mechanisms are removed on every
   update so they cannot start a second or an elevated duplicate instance. */
extern "C" {
#include "bongo_cat/common.h"
#include "bongo_cat/platform.h"
}
#include <windows.h>
#include <shlobj.h>
#include <taskschd.h>
#include <string>

namespace {

/* Minimal RAII for the one Task Scheduler query the migration still needs. */
template<class T> struct Com {
    T *p = nullptr;
    Com() = default;
    Com(const Com &) = delete;
    Com &operator=(const Com &) = delete;
    ~Com() { if (p) p->Release(); }
};
struct Bstr {
    BSTR p = nullptr;
    Bstr() = default;
    explicit Bstr(const wchar_t *s) : p(SysAllocString(s)) {}
    Bstr(const Bstr &) = delete;
    Bstr &operator=(const Bstr &) = delete;
    ~Bstr() { SysFreeString(p); }
    operator BSTR() const { return p; }
};
struct Apartment {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Apartment() = default;
    Apartment(const Apartment &) = delete;
    Apartment &operator=(const Apartment &) = delete;
    ~Apartment() { if (SUCCEEDED(hr)) CoUninitialize(); }
};

constexpr wchar_t run_path[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t run_approval_path[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"
    L"\\StartupApproved\\Run";
constexpr wchar_t shortcut_approval_path[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer"
    L"\\StartupApproved\\StartupFolder";

std::wstring user_sid() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return {};
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    auto user = static_cast<TOKEN_USER *>(LocalAlloc(LPTR, size));
    LPWSTR sid = nullptr;
    std::wstring result;
    if (user && GetTokenInformation(token, TokenUser, user, size, &size) &&
        ConvertSidToStringSidW(user->User.Sid, &sid)) result = sid;
    LocalFree(sid);
    LocalFree(user);
    CloseHandle(token);
    return result;
}

HRESULT set_run_value(bool enabled) {
    HKEY key = nullptr;
    LONG code = RegOpenKeyExW(HKEY_CURRENT_USER, run_path, 0,
        KEY_SET_VALUE, &key);
    if (code != ERROR_SUCCESS) return HRESULT_FROM_WIN32(code);
    HRESULT result = S_OK;
    if (enabled) {
        wchar_t executable[BONGO_CAT_PATH_CAP];
        DWORD length = GetModuleFileNameW(nullptr, executable, BONGO_CAT_PATH_CAP);
        if (!length || length >= BONGO_CAT_PATH_CAP)
            result = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        else {
            std::wstring command =
                L"\"" + std::wstring(executable) + L"\" --autostart";
            code = RegSetValueExW(key, BONGO_CAT_NAME_W, 0, REG_SZ,
                reinterpret_cast<const BYTE *>(command.c_str()),
                static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
            if (code != ERROR_SUCCESS) result = HRESULT_FROM_WIN32(code);
        }
    } else {
        code = RegDeleteValueW(key, BONGO_CAT_NAME_W);
        if (code != ERROR_SUCCESS && code != ERROR_FILE_NOT_FOUND)
            result = HRESULT_FROM_WIN32(code);
    }
    RegCloseKey(key);
    return result;
}

/* Task Manager records a disabled Run entry in StartupApproved. Re-enabling
   from settings must clear that record, or the entry stays off despite the
   switch being on. */
void clear_run_approval(void) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, run_approval_path, 0,
            KEY_SET_VALUE, &key) != ERROR_SUCCESS) return;
    RegDeleteValueW(key, BONGO_CAT_NAME_W);
    RegCloseKey(key);
}

void delete_legacy_shortcut(void) {
    PWSTR directory = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Startup, KF_FLAG_DONT_VERIFY,
            nullptr, &directory))) return;
    std::wstring path = std::wstring(directory) + L"\\" BONGO_CAT_NAME_W L".lnk";
    CoTaskMemFree(directory);
    DeleteFileW(path.c_str());
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, shortcut_approval_path, 0,
            KEY_SET_VALUE, &key) != ERROR_SUCCESS) return;
    RegDeleteValueW(key, BONGO_CAT_NAME_W L".lnk");
    RegCloseKey(key);
}

void delete_legacy_task(void) {
    std::wstring sid = user_sid();
    if (sid.empty()) return;
    Apartment apartment;
    if (FAILED(apartment.hr) && apartment.hr != RPC_E_CHANGED_MODE) return;
    Com<ITaskService> service;
    if (FAILED(CoCreateInstance(CLSID_TaskScheduler, nullptr,
            CLSCTX_INPROC_SERVER, IID_ITaskService,
            reinterpret_cast<void **>(&service.p)))) return;
    VARIANT empty;
    VariantInit(&empty);
    if (FAILED(service->Connect(empty, empty, empty, empty))) return;
    Com<ITaskFolder> folder;
    if (FAILED(service->GetFolder(Bstr(L"\\"), &folder.p))) return;
    /* A leftover only risks a duplicate launch; its removal must never be
       able to fail the update itself. */
    (void)folder->DeleteTask(
        Bstr((std::wstring(L"BongoCat.Autostart.") + sid).c_str()), 0);
}

void remove_legacy_autostart(void) {
    delete_legacy_shortcut();
    delete_legacy_task();
}
} // namespace

extern "C" void bongo_cat_windows_autostart_sync(bool enabled) {
    (void)set_run_value(enabled);
    remove_legacy_autostart();
}

extern "C" BongoCatResult bongo_cat_platform_set_autostart(bool enabled,
    bool administrator, BongoCatError *error) {
    /* The parameter selected the removed Task Scheduler variant; registry
       entries ignore it and always launch with user rights. */
    (void)administrator;
    HRESULT hr = set_run_value(enabled);
    if (enabled) clear_run_approval();
    remove_legacy_autostart();
    if (SUCCEEDED(hr)) return BONGO_CAT_OK;
    bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
        "Cannot update Windows autostart (0x%08lx)",
        static_cast<unsigned long>(hr));
    return BONGO_CAT_ERROR_PLATFORM;
}
