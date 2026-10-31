#include "game/audio.h"

#include <cstdio>
#include <cstring>

#include "game/tuning.h"
#include "platform/platform.h"

// Defined in the generated sounds_data.cpp.
extern const Sound SOUNDS[];
extern const int SOUND_COUNT;

namespace {

constexpr int VOICES = 4;  // sound effects that can overlap

struct Voice {
    const Sound* sound = nullptr;
    uint32_t pos = 0;
    uint32_t started = 0;  // for replacing the oldest when all are busy
};

// Everything below is shared with the audio callback; change it only while
// holding platform::audio_lock().
Voice voices[VOICES];
uint32_t play_count = 0;
const Sound* music = nullptr;
uint32_t music_pos = 0;
int beep_left = 0;        // samples
uint32_t beep_phase = 0;  // 16.16 fixed point
uint32_t beep_step = 0;
int master = 256;  // master volume, 256 = full; one int, so safe to change without the lock

// "steal" -> "sounds/steal"
const Sound* find_in(const char* folder, const char* name) {
    char full[64];
    std::snprintf(full, sizeof(full), "%s/%s", folder, name);
    return audio::find(full);
}

}  // namespace

namespace audio {

const Sound* find(const char* name) {
    for (int i = 0; i < SOUND_COUNT; ++i) {
        if (std::strcmp(SOUNDS[i].name, name) == 0) return &SOUNDS[i];
    }
    return nullptr;
}

void play(const char* name, uint16_t fallback_hz, uint16_t fallback_ms) {
    if (const Sound* s = find_in("sounds", name)) {
        play_sound(s);
    } else {
        beep(fallback_hz, fallback_ms);
    }
}

void play_sound(const Sound* s) {
    if (!s || s->length == 0) return;
    platform::audio_lock();
    Voice* pick = &voices[0];
    for (Voice& v : voices) {
        if (!v.sound) {
            pick = &v;
            break;
        }
        if (v.started < pick->started) pick = &v;
    }
    *pick = {s, 0, ++play_count};
    platform::audio_unlock();
}

void beep(uint16_t hz, uint16_t ms) {
    platform::audio_lock();
    beep_step = static_cast<uint32_t>((static_cast<uint64_t>(hz) << 16) / SAMPLE_RATE);
    beep_left = SAMPLE_RATE * ms / 1000;
    platform::audio_unlock();
}

void play_music(const char* name) {
    const Sound* s = find_in("music", name);
    platform::audio_lock();
    if (s != music) {
        music = s;
        music_pos = 0;
    }
    platform::audio_unlock();
}

void stop_music() {
    platform::audio_lock();
    music = nullptr;
    platform::audio_unlock();
}

void set_master_volume(float gain) {
    if (gain < 0.0f) gain = 0.0f;
    if (gain > 1.0f) gain = 1.0f;
    master = static_cast<int>(gain * 256.0f);
}

void mix(int16_t* out, int count) {
    const int level = master;
    // Samples are -127..127; a volume of 256 brings them up to full 16-bit range.
    const int music_volume = static_cast<int>(tuning::MUSIC_VOLUME * 256.0f);
    const int sfx_volume = static_cast<int>(tuning::SFX_VOLUME * 256.0f);
    const int beep_level = static_cast<int>(tuning::BEEP_VOLUME * 32767.0f);

    for (int i = 0; i < count; ++i) {
        int sample = 0;

        if (music) {
            sample += music->data[music_pos] * music_volume;
            if (++music_pos >= music->length) music_pos = 0;  // loop
        }

        for (Voice& v : voices) {
            if (!v.sound) continue;
            sample += v.sound->data[v.pos] * sfx_volume;
            if (++v.pos >= v.sound->length) v.sound = nullptr;
        }

        if (beep_left > 0) {
            sample += (beep_phase & 0x8000) ? beep_level : -beep_level;
            beep_phase += beep_step;
            --beep_left;
        }

        sample = sample * level / 256;
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        out[i] = static_cast<int16_t>(sample);
    }
}

}  // namespace audio
