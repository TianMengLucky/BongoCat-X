#ifndef BONGO_CAT_WINDOWS_AUTOSTART_H
#define BONGO_CAT_WINDOWS_AUTOSTART_H

#ifdef __cplusplus
extern "C" {
#endif
/* Keep the registry Run entry in step with the saved setting and remove
   leftovers of the earlier shortcut and Task Scheduler mechanisms. Best
   effort: failures are not fatal and are not reported. */
void bongo_cat_windows_autostart_sync(bool enabled);
#ifdef __cplusplus
}
#endif
#endif
