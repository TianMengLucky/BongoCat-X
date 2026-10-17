#ifndef BONGO_CAT_WINDOWS_DIALOG_H
#define BONGO_CAT_WINDOWS_DIALOG_H

#ifdef _WIN32

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_video.h>

#ifdef __cplusplus
extern "C" {
#endif
/*
 * SDL 3.2's Windows folder-dialog backend still uses
 * SHBrowseForFolderW(), which is the small, tree-only "Browse for Folder"
 * dialog. Keep the same asynchronous callback contract here, but use the
 * modern IFileDialog implementation instead.
 */
void bongo_cat_windows_show_open_folder_dialog(
    SDL_DialogFileCallback callback, void *userdata, SDL_Window *window,
    const char *default_location, bool allow_many);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* BONGO_CAT_WINDOWS_DIALOG_H */
