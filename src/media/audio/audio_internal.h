#ifndef BONGO_CAT_AUDIO_INTERNAL_H
#define BONGO_CAT_AUDIO_INTERNAL_H

#include "bongo_cat/audio.h"
#include "bongo_cat/safe_ffi.h"
#include <miniaudio.h>
#ifdef _WIN32
#include <SDL3/SDL_atomic.h>
#endif

#define AUDIO_VOICES 50
#define AUDIO_IDLE_MS 5000

/* App-thread reference counts; the audio callback only reads immutable PCM. */
typedef struct AudioClip {
    float *samples;
    size_t sample_count;
    ma_uint64 frames;
    unsigned int rate, channels, references;
} AudioClip;

typedef struct AudioVoice {
    ma_sound sound;
    /* Each voice has an independent cursor over shared Rust-decoded PCM. */
    ma_audio_buffer buffer;
    AudioClip *clip;
    char path[BONGO_CAT_PATH_CAP];
    char motion_id[BONGO_CAT_BEHAVIOR_ID_CAP];
    uint64_t used;
    uint64_t order;
    bool ready;
} AudioVoice;

struct BongoCatAudio {
    ma_engine engine;
#ifdef _WIN32
    SDL_AtomicInt mixing;
#endif
    /* Stable addresses while playing; allocate only with the engine. */
    AudioVoice *voices;
    uint64_t sequence;
    uint64_t last_play;
    uint64_t next_collect;
    bool initialized;
    bool enabled;
};

void bongo_cat_audio_clip_release(AudioClip *clip);
void bongo_cat_audio_voice_release(AudioVoice *voice);
ma_result bongo_cat_audio_initialize(BongoCatAudio *audio);

/* Explicit time keeps idle reclamation deterministic in offline tests. */
void bongo_cat_audio_collect(BongoCatAudio *audio, uint64_t now_ms);

#endif
