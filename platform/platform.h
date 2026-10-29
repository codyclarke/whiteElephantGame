#pragma once
#include <cstdint>

// The only things the game is allowed to ask of the hardware.
// Each backend (platform/sdl, platform/pico) implements these, plus its own
// main() that runs the loop. If game code compiles against this header on the
// PC, it will run on the handheld.
namespace platform {

// Button bits. Matches the Waveshare Pico-LCD-1.3: a 5-way joystick plus
// A / B / X / Y keys.
enum : uint16_t {
    BTN_UP    = 1 << 0,
    BTN_DOWN  = 1 << 1,
    BTN_LEFT  = 1 << 2,
    BTN_RIGHT = 1 << 3,
    BTN_PRESS = 1 << 4,  // joystick click
    BTN_A     = 1 << 5,
    BTN_B     = 1 << 6,
    BTN_X     = 1 << 7,
    BTN_Y     = 1 << 8,
};

uint16_t buttons();                    // bitmask of buttons held right now
uint32_t millis();                     // milliseconds since power-on
void present(const uint16_t* pixels);  // push a 320x240 RGB565 frame to the screen
void set_backlight(bool on);           // off while the game sleeps (Game::sleeping())

// A few bytes that survive power-off (your character's look). load_data
// returns how many bytes it read: 0 means nothing has been saved yet.
bool save_data(const void* data, uint32_t size);
uint32_t load_data(void* data, uint32_t size);

// Sound: the platform plays whatever audio::mix() produces (game/audio.h).
// These bracket changes to the mixer so they don't race the audio callback.
void audio_lock();
void audio_unlock();

}  // namespace platform
