#ifndef BONGO_CAT_SAFE_FFI_H
#define BONGO_CAT_SAFE_FFI_H

/* C ABI of the bongo-safe Rust crate (src/rust/bongo-safe): the home of the
   memory-safety-critical parsers — SHA-256, image decoding, the about-page
   contributor feed, audio decoding, and Live2D expression files. Untrusted
   byte streams are parsed in safe Rust instead of hand-rolled C.
   Buffers crossing this boundary are allocated in Rust and released through
   the matching bongo_safe_free_* function, never with free(). */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Optional preferences layout in Settings.extensions_json. Set preserves all
   other keys and leaves the buffer unchanged on invalid JSON or insufficient
   capacity. Get defaults to expanded; neither changes the settings ABI. */
bool bongo_safe_direct_composition_get(const char *extensions, size_t capacity);
bool bongo_safe_direct_composition_set(char *extensions, size_t capacity, bool enabled);
bool bongo_safe_gl_true_transparency_get(const char *extensions, size_t capacity);
bool bongo_safe_gl_true_transparency_set(char *extensions, size_t capacity, bool enabled);
bool bongo_safe_render_system_memory_get(const char *extensions, size_t capacity);
bool bongo_safe_render_system_memory_set(char *extensions, size_t capacity, bool enabled);
bool bongo_safe_statistics_overlay_get(const char *extensions, size_t capacity);
bool bongo_safe_statistics_overlay_set(char *extensions, size_t capacity, bool enabled);
/* Statistics display only; labels have at most 16 Unicode characters. Empty uses the locale. */
#define BONGO_SAFE_STATISTICS_UNIT_CAP 65 /* 16 four-byte UTF-8 characters plus NUL. */
#define BONGO_SAFE_STATISTICS_SCALE_MIN 0.000000001
#define BONGO_SAFE_STATISTICS_SCALE_MAX 1000000000.0
typedef struct BongoSafeStatisticsUnits {
    char count[BONGO_SAFE_STATISTICS_UNIT_CAP];
    char movement[BONGO_SAFE_STATISTICS_UNIT_CAP];
    char scroll[BONGO_SAFE_STATISTICS_UNIT_CAP];
    double movement_scale;
    double scroll_scale;
} BongoSafeStatisticsUnits;
bool bongo_safe_statistics_units_get(const char *extensions, size_t capacity,
    BongoSafeStatisticsUnits *out);
bool bongo_safe_statistics_units_set(char *extensions, size_t capacity,
    const BongoSafeStatisticsUnits *units);
/* Store URLs are validated and stored in the existing settings extensions. */
bool bongo_safe_store_repository_get(const char *extensions, size_t capacity,
    char *out, size_t out_capacity);
bool bongo_safe_store_repository_set(char *extensions, size_t capacity, const char *url);
bool bongo_safe_auto_update_get(const char *extensions, size_t capacity);
bool bongo_safe_auto_update_set(char *extensions, size_t capacity, bool enabled);
/* RGB values above 0xffffff select the default palette (setter: 0x1000000). */
bool bongo_safe_preferences_compact_get(const char *extensions, size_t capacity);
bool bongo_safe_preferences_compact_set(char *extensions, size_t capacity, bool enabled);
uint32_t bongo_safe_preferences_text_get(const char *extensions, size_t capacity);
bool bongo_safe_preferences_text_set(char *extensions, size_t capacity, uint32_t rgb);
uint32_t bongo_safe_preferences_background_get(const char *extensions, size_t capacity);
bool bongo_safe_preferences_background_set(char *extensions, size_t capacity, uint32_t rgb);
uint32_t bongo_safe_preferences_color_get(const char *extensions, size_t capacity, bool secondary);
bool bongo_safe_preferences_color_set(char *extensions, size_t capacity, bool secondary, uint32_t rgb);
bool bongo_safe_preferences_rounded_get(const char *extensions, size_t capacity);
bool bongo_safe_preferences_rounded_set(char *extensions, size_t capacity, bool enabled);
bool bongo_safe_preferences_collapsible_get(const char *extensions, size_t capacity);
bool bongo_safe_preferences_collapsible_set(char *extensions, size_t capacity, bool enabled);

/* --- streaming SHA-256 (64 lowercase hex digits, NUL terminated) --- */

typedef struct bongo_safe_sha256 bongo_safe_sha256;

bongo_safe_sha256 *bongo_safe_sha256_new(void);
void bongo_safe_sha256_update(bongo_safe_sha256 *state, const void *data,
    size_t size);
/* Consumes the state (even on earlier failure paths) and writes 65 bytes. */
void bongo_safe_sha256_finish(bongo_safe_sha256 *state, char output[65]);
void bongo_safe_sha256_bytes(const void *data, size_t size, char output[65]);

/* --- image decoding (RGBA8, 4 bytes per pixel) --- */

/* Decodes PNG/JPEG/GIF/BMP/TGA/WebP/ICO/PNM/HDR bytes, bounded by hard
   dimension and allocation limits. Returns an exactly width * height * 4
   byte buffer (release with bongo_safe_free_pixels) or NULL. */
unsigned char *bongo_safe_image_decode(const unsigned char *data, size_t size,
    int *width, int *height);
/* Header-only dimensions for the same formats. */
bool bongo_safe_image_info(const unsigned char *data, size_t size, int *width,
    int *height);
/* UTF-8 file path; reads headers without retaining the encoded file. */
bool bongo_safe_image_info_file(const char *path, int *width, int *height);
/* byte_count is width*height*4 for RGBA, or size for a native payload. */
void bongo_safe_free_pixels(unsigned char *pixels, size_t byte_count);
/* Native atlases are premultiplied RGBA8 (format 0) or BC7 UNORM (format 1).
   Alpha coverage retains the original image, independently of GPU compression.
   Release pixels using size, not width*height*4. Output must be empty. */
typedef struct BongoSafeNativeTexture {
    unsigned char *pixels;
    size_t size;
    uint32_t width, height, source_width, source_height, format;
    unsigned char alpha[128 * 128];
} BongoSafeNativeTexture;
typedef bool (*bongo_safe_cancelled)(void *userdata);
void bongo_safe_native_texture_cache_root(const char *root);
bool bongo_safe_native_texture_load(const char *path, uint32_t max_width,
    uint32_t max_height, bool bc7, BongoSafeNativeTexture *texture,
    bongo_safe_cancelled cancelled, void *userdata);

/* --- about-page contributor feed --- */


typedef struct bongo_safe_about_person {
    char *name;            /* UTF-8, released by bongo_safe_free_persons */
    char *profile;         /* UTF-8, released by bongo_safe_free_persons */
    unsigned char *pixels; /* avatar_size^2 * 4 RGBA, passes to the caller;
                              release with bongo_safe_free_pixels */
} bongo_safe_about_person;

/* Parses the self-assembled contributor SVG and decodes every avatar
   (contributor-controlled network data). Cancellation is checked between
   records and before each avatar decode. Returns the person count (0 when
   the feed is empty) or -1 on failure or cancellation, leaving
   *out_persons untouched; all buffers are freed internally then. */
int bongo_safe_about_feed_parse(const char *svg, size_t size,
    bongo_safe_cancelled cancelled, void *userdata, int avatar_size,
    int person_cap, bongo_safe_about_person **out_persons);
/* Releases the array and every name/profile; pixel ownership already
   passed to the caller. */
void bongo_safe_free_persons(bongo_safe_about_person *persons, int count);

/* --- audio decoding (interleaved f32 PCM) --- */

/* Decodes WAV/MP3/Ogg Vorbis/FLAC files, capped so a long file cannot
   balloon memory. Returns false on any failure. */
bool bongo_safe_audio_decode_file(const char *path, float **samples,
    uint64_t *frame_count, uint32_t *sample_rate, uint32_t *channels);
void bongo_safe_free_samples(float *samples, size_t sample_count);

/* --- Live2D expression files (.exp3.json) --- */

/* Blend mode of an expression parameter. */
enum bongo_safe_expression_blend {
    BONGO_SAFE_EXPRESSION_BLEND_ADD = 0,
    BONGO_SAFE_EXPRESSION_BLEND_MULTIPLY = 1,
    BONGO_SAFE_EXPRESSION_BLEND_OVERWRITE = 2
};

typedef struct BongoSafeExpressionParameter {
    char *id;      /* UTF-8 parameter id, released by bongo_safe_free_expression */
    float value;
    uint8_t blend; /* a bongo_safe_expression_blend value */
} BongoSafeExpressionParameter;

typedef struct BongoSafeExpression {
    float fade_in_seconds;  /* 1.0 when the file omits it */
    float fade_out_seconds; /* 1.0 when the file omits it */
    BongoSafeExpressionParameter *parameters; /* released by bongo_safe_free_expression */
    int parameter_count;
} BongoSafeExpression;

/* Parses a .exp3.json file. The bundled Cubism JSON parser cannot read
   numbers in scientific notation, which Cubism Editor exports (upstream
   issue #68), so expression files route through here. Returns false on any
   failure, leaving *out_expression untouched. */
bool bongo_safe_expression_parse(const unsigned char *data, size_t size,
    BongoSafeExpression **out_expression);
void bongo_safe_free_expression(BongoSafeExpression *expression);

void bongo_safe_free_string(char *text);

/* --- Mver hand-layout files (resources/<group>/hand-layout.json) --- */

/* Source-sprite regions in image pixels with a top-left origin. */
typedef struct BongoSafeHandRect {
    int x;
    int y;
    int width;
    int height;
} BongoSafeHandRect;

/* Multi-key display layout. The caller pre-fills its defaults; fields absent
   from the document keep those values. */
typedef struct BongoSafeHandLayout {
    BongoSafeHandRect hand;   /* hand art inside a key sprite */
    BongoSafeHandRect bubble; /* shared frame, including its tail */
    BongoSafeHandRect characters; /* glyph region inside the bubble */
    int gap;                  /* spacing around the plus separator */
    int max_keys;             /* simultaneous keys kept per side */
} BongoSafeHandLayout;

/* Parses a hand-layout.json document into the caller's struct. The size
   argument must equal sizeof(BongoSafeHandLayout); a mismatch returns -1 so
   ABI drift fails loudly. Returns a BongoCatResult code (0 = OK) and writes
   a diagnostic into message on failure. */
int bongo_safe_hand_layout_parse(const unsigned char *bytes, size_t size,
    void *layout, size_t layout_size, char *message, size_t message_capacity);

/* --- configuration JSON (bongocat/settings & bongocat/session) --- */

/* Parses configuration JSON into the caller's struct. The struct must be
   pre-defaulted: fields absent from the document keep their values. The
   size arguments must equal sizeof(BongoCatSettings) /
   sizeof(BongoCatSessionState); a mismatch returns -1 so ABI drift fails
   loudly. Returns a BongoCatResult code (0 = OK) and writes a diagnostic
   into message on failure. */
int bongo_safe_settings_parse(const unsigned char *bytes, size_t size,
    void *settings, size_t settings_size, char *message,
    size_t message_capacity);
int bongo_safe_session_parse(const unsigned char *bytes, size_t size,
    void *session, size_t session_size, char *message,
    size_t message_capacity);
/* Serializes the canonical JSON document (pretty-printed, including the
   format and schemaVersion fields). The caller must have validated the
   struct. Returns a BongoCatResult code; the buffer is released with
   bongo_safe_free_json. */
int bongo_safe_settings_write(const void *settings, size_t settings_size,
    unsigned char **json, size_t *length);
int bongo_safe_session_write(const void *session, size_t session_size,
    unsigned char **json, size_t *length);
void bongo_safe_free_json(unsigned char *json, size_t length);

char *bongo_safe_mver_edit(const char *data, size_t length, const char *mode,
    const char *field, int index, const char *row);
bool bongo_safe_mver_labels(const char *data, size_t length, const char *mode,
    bool (*callback)(void *userdata, const char *field, size_t index, const char *label),
    void *userdata);
/* Borrowed immutable, aligned SPIR-V; bytes receives its byte length.
   No parsing or allocation occurs at runtime. Never free this pointer. */
const uint32_t *bongo_safe_corner_shader(bool fragment, size_t *bytes);
int bongo_safe_model_kind_file(const char *path);
bool bongo_safe_inochi_validate_file(const char *path, char *message, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
