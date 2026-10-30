// Handheld backend: runs the game on the Pico with the 2.8" screen, the
// buttons and the I2S amp. Wiring is in WIRING.md; pin numbers in pins.h.
//
// The loop runs the game at a fixed 30 updates a second, like the desktop
// build. Frames go out to the screen by DMA while the next update runs, so
// most of the 16 ms it takes to send one is free.
//
// Build and flash: python tools/pico.py flash steal_night
// Timing and saves are reported over USB (Serial Monitor in VS Code).

#include <cstdio>
#include <cstring>
#include <new>

#include "game/audio.h"
#include "game/game.h"
#include "game/gfx.h"
#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "pico/flash.h"
#include "pico/rand.h"
#include "pico/stdlib.h"
#include "platform/pico/audio_out.h"
#include "platform/pico/lcd.h"
#include "platform/pico/pins.h"
#include "platform/platform.h"

namespace {

constexpr uint32_t STEP_US = 1'000'000 / 30;

struct ButtonPin {
    uint pin;
    uint16_t bit;
};
constexpr ButtonPin BUTTON_PINS[] = {
    {pins::BTN_UP, platform::BTN_UP},       {pins::BTN_DOWN, platform::BTN_DOWN},
    {pins::BTN_LEFT, platform::BTN_LEFT},   {pins::BTN_RIGHT, platform::BTN_RIGHT},
    {pins::BTN_PRESS, platform::BTN_PRESS}, {pins::BTN_A, platform::BTN_A},
    {pins::BTN_B, platform::BTN_B},         {pins::BTN_X, platform::BTN_X},
    {pins::BTN_Y, platform::BTN_Y},
};

// Too big for the stack (the game holds the snow scene, the canvas is the
// whole screen), so they live here.
gfx::Canvas canvas;
alignas(Game) unsigned char game_storage[sizeof(Game)];

// --- Saves ----------------------------------------------------------------------
// The last 4 KB of flash holds the save: a small header, then the game's bytes.

constexpr uint32_t SAVE_OFFSET = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;
constexpr uint32_t SAVE_MAGIC = 0x53415645;  // "SAVE"
struct SaveHeader {
    uint32_t magic;
    uint32_t size;
};
constexpr uint32_t SAVE_MAX = FLASH_SECTOR_SIZE - sizeof(SaveHeader);

uint8_t save_page[FLASH_SECTOR_SIZE];  // flash is written a whole sector at a time

void write_save_sector(void*) {
    flash_range_erase(SAVE_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(SAVE_OFFSET, save_page, FLASH_SECTOR_SIZE);
}

}  // namespace

// --- platform.h -----------------------------------------------------------------

namespace platform {

uint16_t buttons() {
    uint16_t held = 0;
    for (const ButtonPin& b : BUTTON_PINS) {
        if (!gpio_get(b.pin)) held |= b.bit;  // pressed pulls the pin to GND
    }
    return held;
}

uint32_t millis() { return to_ms_since_boot(get_absolute_time()); }

void set_backlight(bool on) {
    lcd::set_backlight(on ? 255 : 0);
    audio_out::set_amp(on);  // asleep: the amp is off too
}

void present(const uint16_t* pixels) { lcd::start_frame(pixels); }

bool save_data(const void* data, uint32_t size) {
    if (size > SAVE_MAX) return false;
    std::memset(save_page, 0xFF, sizeof save_page);
    SaveHeader header{SAVE_MAGIC, size};
    std::memcpy(save_page, &header, sizeof header);
    std::memcpy(save_page + sizeof header, data, size);

    // Erasing flash stops everything for a moment, including the sound
    // interrupt. Switch the amp off so that moment is silent, not a squawk.
    lcd::wait();
    audio_out::set_amp(false);
    int result = flash_safe_execute(write_save_sector, nullptr, 1000);
    audio_out::set_amp(true);
    std::printf("saved %lu bytes%s\n", static_cast<unsigned long>(size), result == PICO_OK ? "" : " -- FAILED");
    return result == PICO_OK;
}

uint32_t load_data(void* data, uint32_t size) {
    const uint8_t* stored = reinterpret_cast<const uint8_t*>(XIP_BASE + SAVE_OFFSET);
    SaveHeader header;
    std::memcpy(&header, stored, sizeof header);
    if (header.magic != SAVE_MAGIC || header.size > SAVE_MAX) return 0;  // never saved
    uint32_t got = header.size < size ? header.size : size;
    std::memcpy(data, stored + sizeof header, got);
    return got;
}

void audio_lock() { audio_out::lock(); }
void audio_unlock() { audio_out::unlock(); }

}  // namespace platform

// --- main -----------------------------------------------------------------------

int main() {
    stdio_init_all();

    for (const ButtonPin& b : BUTTON_PINS) {
        gpio_init(b.pin);
        gpio_set_dir(b.pin, GPIO_IN);
        gpio_pull_up(b.pin);
    }

    lcd::init();
    audio_out::start(audio::SAMPLE_RATE, audio::mix);

    Game& game = *new (game_storage) Game(get_rand_32());
    game.render(canvas);
    platform::present(canvas.px);
    lcd::set_backlight(255);  // on once there's a picture

    uint16_t prev = 0;
    uint64_t next = time_us_64();
    uint64_t busy_us = 0, worst_us = 0;
    int frames = 0;

    for (;;) {
        // Fixed-rate updates; if a frame ran long, catch up (but not forever).
        uint64_t now = time_us_64();
        if (now > next + 250'000) next = now;
        while (now >= next) {
            uint16_t held = platform::buttons();
            game.update({held, static_cast<uint16_t>(held & ~prev)}, 1.0f / 30.0f);
            prev = held;
            next += STEP_US;
        }

        lcd::wait();  // the last frame must be out before drawing over it
        game.render(canvas);
        platform::present(canvas.px);

        // How long a frame of work takes, printed every few seconds.
        uint64_t spent = time_us_64() - now;
        busy_us += spent;
        if (spent > worst_us) worst_us = spent;
        if (++frames == 150) {
            std::printf("frame work: %.1f ms average, %.1f ms worst (budget 33.3)\n", busy_us / 150 / 1000.0,
                        worst_us / 1000.0);
            busy_us = worst_us = 0;
            frames = 0;
        }

        // Rest until the next update is due; asleep, rest longer.
        uint64_t wake = game.sleeping() ? time_us_64() + 30'000 : next;
        if (wake > time_us_64()) sleep_until(from_us_since_boot(wake));
    }
}
