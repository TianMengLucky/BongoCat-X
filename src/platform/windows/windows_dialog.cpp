#include "windows_dialog.h"
#include "bongo_cat/log.h"
#ifdef _WIN32
#include "windows_com.hpp"
#include <SDL3/SDL.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <new>
using bongo_cat::ComPtr;
struct FolderDialogArgs {
    SDL_DialogFileCallback callback = nullptr;
    void *userdata = nullptr;
    HWND owner = nullptr;
    char *location = nullptr;
    bool many = false;
    Uint64 started = 0;
};
struct SdlFree { void operator()(void *p) const noexcept { SDL_free(p); } };
struct PathsFree {
    void operator()(char **paths) const noexcept {
        if (!paths) return;
        for (char **p = paths; *p; ++p) SDL_free(*p);
        SDL_free(paths);
    }
};
struct ArgsFree {
    void operator()(FolderDialogArgs *p) const noexcept { SDL_free(p->location); delete p; }
};
static void cancel(const FolderDialogArgs &a) {
    const char *files[] = {nullptr}; a.callback(a.userdata, files, -1);
}
static char *utf8(const wchar_t *text) {
    if (!text) return nullptr;
    int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, nullptr, 0, nullptr, nullptr);
    auto result = std::unique_ptr<char, SdlFree>(count > 0 ? static_cast<char *>(SDL_malloc(count)) : nullptr);
    if (!result || !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, result.get(), count, nullptr, nullptr)) return nullptr;
    return result.release();
}
static bool show(const FolderDialogArgs &a) {
    bongo_cat::ComApartment apartment(COINIT_APARTMENTTHREADED);
    if (FAILED(apartment.status())) return false;
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(dialog.GetAddressOf())))) return false;
    FILEOPENDIALOGOPTIONS options = 0;
    if (FAILED(dialog->GetOptions(&options))) return false;
    options |= FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR;
    if (a.many) options |= FOS_ALLOWMULTISELECT;
    if (FAILED(dialog->SetOptions(options))) return false;
    if (a.location && a.location[0]) {
        int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, a.location, -1, nullptr, 0);
        auto wide = std::unique_ptr<wchar_t, SdlFree>(count > 0 ? static_cast<wchar_t *>(SDL_malloc((size_t)count * sizeof(wchar_t))) : nullptr);
        if (count > 0 && (!wide || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, a.location, -1, wide.get(), count))) return false;
        ComPtr<IShellItem> folder;
        if (wide && SUCCEEDED(SHCreateItemFromParsingName(wide.get(), nullptr, IID_PPV_ARGS(folder.GetAddressOf()))))
            (void)dialog->SetFolder(folder.Get());
    }
    if (FAILED(dialog->Show(a.owner))) return false;
    if (a.many) {
        ComPtr<IShellItemArray> items;
        DWORD count = 0;
        if (FAILED(dialog->GetResults(items.GetAddressOf())) || !items || FAILED(items->GetCount(&count)) || !count) return false;
        if ((size_t)count >= SIZE_MAX / sizeof(char *) - 1) return false;
        std::unique_ptr<char *, PathsFree> files(static_cast<char **>(SDL_calloc((size_t)count + 1, sizeof(char *))));
        if (!files) return false;
        for (DWORD i = 0; i < count; ++i) {
            ComPtr<IShellItem> item;
            if (FAILED(items->GetItemAt(i, item.GetAddressOf())) || !item) return false;
            PWSTR raw = nullptr;
            HRESULT result = item->GetDisplayName(SIGDN_FILESYSPATH, &raw);
            bongo_cat::TaskMemory<wchar_t> path(raw);
            if (FAILED(result) || !(files.get()[i] = utf8(path.get()))) return false;
        }
        a.callback(a.userdata, const_cast<const char *const *>(files.get()), -1);
    } else {
        ComPtr<IShellItem> item;
        if (FAILED(dialog->GetResult(item.GetAddressOf())) || !item) return false;
        PWSTR raw = nullptr;
        HRESULT result = item->GetDisplayName(SIGDN_FILESYSPATH, &raw);
        bongo_cat::TaskMemory<wchar_t> path(raw);
        std::unique_ptr<char, SdlFree> file(SUCCEEDED(result) ? utf8(path.get()) : nullptr);
        if (!file) return false;
        const char *files[] = {file.get(), nullptr}; a.callback(a.userdata, files, -1);
    }
    return true;
}
static int SDLCALL run(void *userdata) {
    std::unique_ptr<FolderDialogArgs, ArgsFree> args(static_cast<FolderDialogArgs *>(userdata));
    if (!show(*args)) cancel(*args);
    SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE, "[runtime] Folder dialog: stage=completed owner=%p elapsed_ms=%.2f", (void *)args->owner,
        (double)(SDL_GetTicksNS() - args->started) / 1000000.0);
    return 0;
}
void bongo_cat_windows_show_open_folder_dialog(SDL_DialogFileCallback callback, void *userdata,
    SDL_Window *window, const char *location, bool many) {
    if (!callback) return;
    const char *empty[] = {nullptr};
    std::unique_ptr<FolderDialogArgs, ArgsFree> args(new (std::nothrow) FolderDialogArgs{});
    if (!args) { callback(userdata, empty, -1); return; }
    args->callback = callback; args->userdata = userdata; args->many = many; args->started = SDL_GetTicksNS();
    HWND owner = window ? static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr)) : nullptr;
    args->owner = owner;
    if (location && !(args->location = SDL_strdup(location))) { callback(userdata, empty, -1); return; }
    FolderDialogArgs *job = args.release();
    SDL_Thread *thread = SDL_CreateThread(run, "BongoCat_WindowsFolderDialog", job);
    if (!thread) { args.reset(job); callback(userdata, empty, -1); return; }
    SDL_LogInfo(BONGO_CAT_LOG_LIFECYCLE, "[runtime] Folder dialog: stage=opened owner=%p allow_many=%d", (void *)owner, many);
    SDL_DetachThread(thread);
}
#endif
