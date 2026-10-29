#pragma once
#include <cstdint>

// A sound from assets/, converted at build time by tools/wav2sound.py.
struct Sound {
    const char* name;     // path under assets/ without ".wav", e.g. "sounds/steal"
    const int8_t* data;   // mono, audio::SAMPLE_RATE samples per second
    uint32_t length;      // in samples
};

// Sound effects, music loops and fallback beeps, mixed in software so the PC
// and the handheld sound the same. The platform pulls samples with mix();
// the game calls everything else.
namespace audio {

constexpr int SAMPLE_RATE = 16000;

const Sound* find(const char* name);  // nullptr if there's no such WAV

// Play assets/sounds/<name>.wav. If there's no such file yet, beep instead.
void play(const char* name, uint16_t fallback_hz, uint16_t fallback_ms);
void play_sound(const Sound* s);
void beep(uint16_t hz, uint16_t ms);

// Loop assets/music/<name>.wav. Carries on if that track is already playing;
// a name with no WAV means silence.
void play_music(const char* name);
void stop_music();

// Fill out with the next count samples (mono, signed 16-bit).
// Platforms call this from their audio callback.
void mix(int16_t* out, int count);

}  // namespace audio
