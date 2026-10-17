#ifndef BONGO_CAT_HAND_LAYOUT_H
#define BONGO_CAT_HAND_LAYOUT_H

#include "bongo_cat/common.h"
#include "bongo_cat/safe_ffi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fills `layout` with the built-in crop defaults. Callers pre-fill this before
   parsing a document, so fields absent from the file keep those values. */
void bongo_cat_hand_layout_defaults(BongoSafeHandLayout *layout);
/* Scale the 1000-square artwork defaults for larger square sprites. A right
   sprite uses reflected crop coordinates; authored layouts stay in pixels. */
void bongo_cat_hand_layout_defaults_for_image(BongoSafeHandLayout *layout,
    int width, int height, bool right);

/* Whether `<directory>/resources/<group>` is a key folder. `explicit_side`
   receives whether that folder carries its own hand-layout.json: a side with
   its own file uses non-mirrored coordinates, one without mirrors the other. */
bool bongo_cat_hand_layout_side_exists(const char *directory, const char *group,
    bool *explicit_side);

/* Writes `layout` to `<directory>/resources/<group>/hand-layout.json` after
   validating it with the Rust parser. Returns false, leaving any existing file
   untouched, when the folder is missing, the layout is invalid, or the write
   fails (for example on a read-only install). */
bool bongo_cat_hand_layout_file_write(const char *directory, const char *group,
    const BongoSafeHandLayout *layout);

/* Creates the folder's hand-layout.json with the built-in defaults when the
   folder exists and the file is absent. Existing files are left untouched, and
   a missing folder is not created. */
bool bongo_cat_hand_layout_file_ensure(const char *directory, const char *group);

#ifdef __cplusplus
}
#endif

#endif
