#include "windows_package.h"
#include "windows_com.hpp"
#include "bongo_cat/common.h"
#include <SDL3/SDL.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <cstdio>
using bongo_cat::ComPtr;
struct TemporaryLink {
    wchar_t path[MAX_PATH]{};
    ~TemporaryLink() { if (path[0]) DeleteFileW(path); }
};
static HRESULT repair_shortcut() {
    using GetApplicationId = LONG (WINAPI *)(UINT32 *, PWSTR);
    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    auto get_id = kernel ? reinterpret_cast<GetApplicationId>(GetProcAddress(kernel, "GetCurrentApplicationUserModelId")) : nullptr;
    if (!get_id) return S_FALSE;
    wchar_t app_id[BONGO_CAT_PATH_CAP]; UINT32 capacity = BONGO_CAT_PATH_CAP;
    LONG code = get_id(&capacity, app_id);
    if (code != ERROR_SUCCESS) return HRESULT_FROM_WIN32(code);
    PWSTR raw_desktop = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Desktop, KF_FLAG_DONT_VERIFY, nullptr, &raw_desktop);
    bongo_cat::TaskMemory<wchar_t> desktop(raw_desktop);
    if (FAILED(hr)) return hr;
    wchar_t path[BONGO_CAT_PATH_CAP];
    int length = swprintf(path, BONGO_CAT_PATH_CAP, L"%ls\\BongoCat.lnk", desktop.get());
    if (length < 0 || length >= BONGO_CAT_PATH_CAP) return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    DWORD attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        code = (LONG)GetLastError();
        return code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND ? S_FALSE : HRESULT_FROM_WIN32(code);
    }
    if (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) return S_FALSE;
    ComPtr<IShellLinkW> old_link, app_link;
    ComPtr<IPersistFile> old_file, app_file;
    if (FAILED(hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(old_link.GetAddressOf())))
        || FAILED(hr = old_link.As(&old_file)) || FAILED(hr = old_file->Load(path, STGM_READ))) return hr;
    wchar_t target[BONGO_CAT_PATH_CAP]{}, executable[BONGO_CAT_PATH_CAP], arguments[BONGO_CAT_PATH_CAP]{};
    if (FAILED(hr = old_link->GetPath(target, BONGO_CAT_PATH_CAP, nullptr, SLGP_RAWPATH))) return hr;
    DWORD count = GetModuleFileNameW(nullptr, executable, BONGO_CAT_PATH_CAP);
    if (!count || count >= BONGO_CAT_PATH_CAP) return E_FAIL;
    if (_wcsicmp(target, executable)) return S_FALSE;
    if (FAILED(hr = old_link->GetArguments(arguments, BONGO_CAT_PATH_CAP))) return hr;
    if (arguments[0]) return S_FALSE;
    wchar_t parsing_name[BONGO_CAT_PATH_CAP];
    length = swprintf(parsing_name, BONGO_CAT_PATH_CAP, L"shell:AppsFolder\\%ls", app_id);
    if (length < 0 || length >= BONGO_CAT_PATH_CAP) return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    PIDLIST_ABSOLUTE raw_item = nullptr;
    hr = SHParseDisplayName(parsing_name, nullptr, &raw_item, 0, nullptr);
    // The PIDL stays opaque and is only passed to Shell APIs or freed.
    bongo_cat::TaskMemory<ITEMIDLIST> item(reinterpret_cast<ITEMIDLIST *>(raw_item));
    if (FAILED(hr)) return hr;
    if (FAILED(hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(app_link.GetAddressOf())))
        || FAILED(hr = app_link->SetIDList(item.get())) || FAILED(hr = app_link.As(&app_file))) return hr;
    TemporaryLink temporary;
    if (!GetTempFileNameW(desktop.get(), L"bcs", 0, temporary.path)) return HRESULT_FROM_WIN32(GetLastError());
    hr = app_file->Save(temporary.path, TRUE);
    if (SUCCEEDED(hr) && !ReplaceFileW(path, temporary.path, nullptr, 0, nullptr, nullptr)) hr = HRESULT_FROM_WIN32(GetLastError());
    if (SUCCEEDED(hr)) SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATHW, path, nullptr);
    return hr;
}
void bongo_cat_windows_package_repair_shortcut() {
    if (!bongo_cat_windows_is_packaged()) return;
    bongo_cat::ComApartment apartment(COINIT_APARTMENTTHREADED);
    HRESULT hr = SUCCEEDED(apartment.status()) || apartment.status() == RPC_E_CHANGED_MODE ? repair_shortcut() : apartment.status();
    if (FAILED(hr)) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot update MSIX desktop application shortcut: 0x%08lx", (unsigned long)hr);
}
