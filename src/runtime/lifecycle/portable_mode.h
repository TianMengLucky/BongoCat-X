#ifndef BONGO_CAT_PORTABLE_MODE_H
#define BONGO_CAT_PORTABLE_MODE_H

#include "bongo_cat/app.h"

/* Portable mode keeps config/data/models/... next to the executable instead
   of the system profile. It is selected through the BongoCat.ini file beside
   the executable ("portable = 1") rather than a settings entry: the storage
   root must be known before settings.json can be located. The ini can also
   be edited by hand before the first launch. An explicit --storage-root
   argument always wins over the ini file. */
bool bongo_cat_portable_mode_active(void);

/* Create or update the ini beside the executable. When enabling, the current
   settings are copied into <exe>/config and the existing data / models /
   state trees are merge-copied below <exe> (files already present there are
   kept, the originals are left untouched). The managed data/live2d content
   (including a stashed Cubism Core) is carried as part of the data tree;
   the separate <exe>/live2d drop-in folder is never modified. A failure to
   copy the settings rolls the ini back and reports the reason; individual
   tree-file failures are best-effort warnings. When disabling, the ini
   records "portable = 0": the previous system-profile data is still in
   place and resumes with the next launch. */
bool bongo_cat_portable_mode_set(bool enable, const BongoCatApp *app,
    BongoCatError *error);

/* Executable directory without a trailing separator, for the storage root. */
bool bongo_cat_portable_root(char *output, size_t capacity);

#endif
