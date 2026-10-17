#ifndef BONGO_CAT_SAFE_IMPORT_H
#define BONGO_CAT_SAFE_IMPORT_H
#include <stdbool.h>
#include <stddef.h>
typedef struct BongoSafeImportMetadata {
    const char *config, *directory, *setting, *assets, *overrides, *target, *mode;
    int format;
    bool gamepad_buttons;
} BongoSafeImportMetadata;
typedef struct BongoSafeCalibration {
    double scale, x, y;
    int width, height;
    bool mirror, auto_frame;
} BongoSafeCalibration;
#ifdef __cplusplus
extern "C" {
#endif
bool bongo_safe_import_metadata(const BongoSafeImportMetadata *, size_t size,
    char *error, size_t error_capacity);
bool bongo_safe_tauri_config(const char *path, const char *mode, const int *left,
    size_t left_count, const int *right, size_t right_count,
    const BongoSafeCalibration *calibration, size_t calibration_size);
bool bongo_safe_tauri_calibration(const char *path, const char *mode,
    BongoSafeCalibration *calibration, size_t calibration_size);
bool bongo_safe_mver_chord(const void *row, bool audio, char *output, size_t capacity);
int bongo_safe_tauri_key_code(const char *filename);
bool bongo_safe_mver_render_options(const void *config, void *options, size_t size);
#ifdef __cplusplus
}
#endif
#endif
