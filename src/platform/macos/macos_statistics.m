#include "bongo_cat/statistics_usage_plugin.h"
#include "bongo_cat/app.h"
#include "bongo_cat/statistics.h"
#include "bongo_cat/path.h"
#import <Cocoa/Cocoa.h>
#include <ApplicationServices/ApplicationServices.h>
#include <math.h>
#include <string.h>

void bongo_cat_statistics_application_poll(void *context, BongoCatApplicationSample *sample) {
    BongoCatApp *host = context;
    *sample = (BongoCatApplicationSample){0};
    sample->available = 1;
    @autoreleasepool {
        CFDictionaryRef session = CGSessionCopyCurrentDictionary();
        if (!session) return;
        CFBooleanRef locked = CFDictionaryGetValue(session, CFSTR("CGSSessionScreenIsLocked"));
        CFBooleanRef console = CFDictionaryGetValue(session, kCGSessionOnConsoleKey);
        bool unlocked = (!locked || !CFBooleanGetValue(locked)) && console && CFBooleanGetValue(console);
        CFRelease(session);
        if (!unlocked) return;
        double idle = CGEventSourceSecondsSinceLastEventType(kCGEventSourceStateHIDSystemState, kCGAnyInputEventType);
        if (!isfinite(idle) || idle < 0.0 || idle > (double)UINT64_MAX / 1000.0) return;
        sample->idle_ms = (uint64_t)(idle * 1000.0);
        NSRunningApplication *app = [[NSWorkspace sharedWorkspace] frontmostApplication];
        const char *id = [[app bundleIdentifier] UTF8String];
        const char *name = [[app localizedName] UTF8String];
        if (!id || !id[0]) id = name;
        if (!id || !id[0] || strlen(id) >= sizeof(sample->id)) return;
        if (!name || !name[0] || strlen(name) >= sizeof(sample->name)) name = id;
        if (!strcmp(id, "com.apple.loginwindow")) return;
        memcpy(sample->id, id, strlen(id) + 1);
        memcpy(sample->name, name, strlen(name) + 1);
        sample->active = 1;
        char target[BONGO_CAT_PATH_CAP], directory[BONGO_CAT_PATH_CAP];
        if (host && bongo_cat_statistics_icon_path(host->data_root, id, target) &&
            !bongo_cat_path_is_file(target) &&
            bongo_cat_path_join(directory, sizeof(directory), host->data_root, "application-icons") &&
            bongo_cat_path_create_directory(directory)) {
            NSImage *icon = [app icon];
            NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                pixelsWide:48 pixelsHigh:48 bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES
                isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:0 bitsPerPixel:0];
            if (icon && bitmap) {
                [NSGraphicsContext saveGraphicsState];
                [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap]];
                [icon drawInRect:NSMakeRect(0, 0, 48, 48)];
                [NSGraphicsContext restoreGraphicsState];
                NSData *data = [bitmap representationUsingType:NSBitmapImageFileTypeBMP properties:@{}];
                [data writeToFile:[NSString stringWithUTF8String:target] options:NSDataWritingAtomic error:NULL];
            }
#if !__has_feature(objc_arc)
            [bitmap release];
#endif
        }
    }
}
