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
void bongo_safe_free_pixels(unsigned char *pixels, size_t pixel_count);

/* --- about-page contributor feed --- */

typedef bool (*bongo_safe_cancelled)(void *userdata);

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

#ifdef __cplusplus
}
#endif

#endif
