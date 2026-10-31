// Hardware bring-up test: run this before the game to check the wiring.
//
// The screen shows colored corners and color bars (to check orientation and
// colors), a button map that lights up as you press, and how fast frames go
// out. Every button plays its own note through the speaker. It starts at a
// slow, safe SPI speed; Y steps up through faster ones, to find the fastest
// the wiring handles cleanly.
//
// First, for troubleshooting, it fills the screen with solid colors for a few
// seconds, and (if the screen's SDO pin is wired to GP4) reads back what chip
// the screen is and what settings it took. Both go out over USB too.
//
// Build and flash: python tools/pico.py flash hw_test

#include <cmath>
#include <cstdio>

#include "game/gfx.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "platform/pico/audio_out.h"
#include "platform/pico/lcd.h"
#include "platform/pico/pins.h"

using gfx::Color;
using gfx::rgb;

namespace {

constexpr int SAMPLE_RATE = 16000;

struct Button {
    uint pin;
    const char* name;
    int x, y;      // where its box goes on screen
    float note;    // Hz
};

constexpr Button BUTTONS[] = {
    {pins::BTN_UP, "UP", 52, 112, 262.0f},     {pins::BTN_DOWN, "DN", 52, 172, 294.0f},
    {pins::BTN_LEFT, "LT", 22, 142, 330.0f},   {pins::BTN_RIGHT, "RT", 82, 142, 349.0f},
    {pins::BTN_PRESS, "PR", 52, 142, 392.0f},  {pins::BTN_A, "A", 262, 142, 440.0f},
    {pins::BTN_B, "B", 232, 172, 494.0f},      {pins::BTN_X, "X", 232, 112, 523.0f},
    {pins::BTN_Y, "Y", 202, 142, 587.0f},
    {pins::BTN_VOL_DOWN, "V-", 132, 176, 659.0f}, {pins::BTN_VOL_UP, "V+", 162, 176, 698.0f},
};
constexpr int BUTTON_COUNT = sizeof BUTTONS / sizeof BUTTONS[0];

// The note playing right now; 0 = quiet. Written by the main loop, read by
// the sound interrupt.
volatile float tone_hz = 0.0f;
float tone_phase = 0.0f;
float tone_level = 0.0f;

void fill_tone(int16_t* out, int count) {
    float hz = tone_hz;
    for (int i = 0; i < count; ++i) {
        float target = hz > 0.0f ? 1.0f : 0.0f;
        tone_level += (target - tone_level) * 0.005f;  // soft start and stop, no clicks
        if (hz > 0.0f) tone_phase += hz / SAMPLE_RATE;
        if (tone_phase >= 1.0f) tone_phase -= 1.0f;
        float wave = std::sin(tone_phase * 6.2831853f);
        out[i] = static_cast<int16_t>(wave * tone_level * 6000.0f);
    }
}

gfx::Canvas canvas;  // 150 KB: keep it off the stack

// Ask the screen about itself; print the raw bytes it sends back.
void report_screen() {
    struct Query { uint8_t cmd; int count; const char* what; };
    const Query queries[] = {
        {0x04, 4, "ID (first byte is filler)"},
        {0xD3, 4, "ILI9341 ID: expect xx 00 93 41"},
        {0x0B, 2, "orientation (MADCTL)"},
        {0x0C, 2, "pixel format: x5 = 16-bit, x6 = 18-bit"},
        {0x09, 5, "status"},
    };
    for (const Query& q : queries) {
        uint8_t bytes[8] = {};
        lcd::read(q.cmd, bytes, q.count);
        std::printf("read %02X:", q.cmd);
        for (int i = 0; i < q.count; ++i) std::printf(" %02X", bytes[i]);
        std::printf("   (%s)\n", q.what);
    }
}

// Whole-screen solid colors, so a pixel format mix-up shows as stripes.
void solid_colors() {
    struct Fill { Color color; const char* name; };
    const Fill fills[] = {{rgb(255, 0, 0), "RED"}, {rgb(0, 255, 0), "GREEN"}, {rgb(0, 0, 255), "BLUE"},
                          {rgb(255, 255, 255), "WHITE"}, {rgb(0, 0, 0), "BLACK"}};
    canvas.set_area(0, gfx::SCREEN_W);
    for (const Fill& f : fills) {
        std::printf("solid %s\n", f.name);
        canvas.clear(f.color);
        lcd::start_frame(canvas.px);
        lcd::wait();
        sleep_ms(1500);
    }
}

void draw(uint32_t held, int frame, float push_ms, float fps) {
    gfx::Canvas& c = canvas;
    c.set_area(0, gfx::SCREEN_W);
    c.clear(rgb(10, 12, 30));

    // Corners: red top-left, green top-right, blue bottom-left, white
    // bottom-right. If they're somewhere else, the picture is flipped.
    c.fill_rect(0, 0, 16, 16, rgb(255, 0, 0));
    c.fill_rect(gfx::SCREEN_W - 16, 0, 16, 16, rgb(0, 255, 0));
    c.fill_rect(0, gfx::SCREEN_H - 16, 16, 16, rgb(0, 0, 255));
    c.fill_rect(gfx::SCREEN_W - 16, gfx::SCREEN_H - 16, 16, 16, rgb(255, 255, 255));
    c.rect(0, 0, gfx::SCREEN_W, gfx::SCREEN_H, rgb(255, 255, 255));

    c.text_centered(gfx::SCREEN_W / 2, 6, "STEAL NIGHT HARDWARE TEST", rgb(255, 210, 60));

    // Color bars, each labeled, so swapped colors are easy to spot.
    struct Bar { Color color; const char* name; };
    const Bar bars[] = {{rgb(255, 0, 0), "RED"},     {rgb(0, 255, 0), "GRN"},   {rgb(0, 0, 255), "BLU"},
                        {rgb(255, 255, 0), "YEL"},   {rgb(0, 255, 255), "CYN"}, {rgb(255, 0, 255), "MAG"},
                        {rgb(255, 255, 255), "WHT"}};
    for (int i = 0; i < 7; ++i) {
        int x = 26 + i * 40;
        c.fill_rect(x, 22, 30, 24, bars[i].color);
        c.text_centered(x + 15, 50, bars[i].name, rgb(220, 220, 220));
    }

    // A smooth gray ramp: steps or speckles here mean a data problem.
    for (int x = 0; x < 256; ++x) {
        uint8_t v = static_cast<uint8_t>(x);
        c.fill_rect(32 + x, 64, 1, 12, rgb(v, v, v));
    }

    // A block that slides back and forth: smooth = frames are getting through.
    int slide = static_cast<int>((std::sin(frame * 0.08f) * 0.5f + 0.5f) * (gfx::SCREEN_W - 60));
    c.fill_rect(30 + slide, 84, 8, 8, rgb(255, 120, 220));

    // The buttons.
    for (int i = 0; i < BUTTON_COUNT; ++i) {
        const Button& b = BUTTONS[i];
        bool on = held & (1u << i);
        c.fill_rect(b.x, b.y, 26, 24, on ? rgb(80, 220, 120) : rgb(40, 44, 70));
        c.rect(b.x, b.y, 26, 24, rgb(160, 170, 200));
        c.text_centered(b.x + 13, b.y + 9, b.name, on ? rgb(0, 0, 0) : rgb(220, 220, 220));
    }
    c.text_centered(160, 140, "PRESS", rgb(140, 150, 180));
    c.text_centered(160, 152, "BUTTONS", rgb(140, 150, 180));

    char line[64];
    std::snprintf(line, sizeof line, "SPI %.1f MHZ  SEND %.1f MS  %.0f FPS", lcd::spi_hz() / 1e6, push_ms, fps);
    c.text_centered(gfx::SCREEN_W / 2, 206, line, rgb(255, 255, 255));
    c.text_centered(gfx::SCREEN_W / 2, 218, "Y: NEXT SPI SPEED", rgb(140, 150, 180));
}

}  // namespace

int main() {
    stdio_init_all();  // printf goes over USB, for a serial monitor

    for (const Button& b : BUTTONS) {
        gpio_init(b.pin);
        gpio_set_dir(b.pin, GPIO_IN);
        gpio_pull_up(b.pin);
    }

    lcd::init();
    lcd::set_backlight(255);
    audio_out::start(SAMPLE_RATE, fill_tone);

    // SPI speeds Y steps through (the Pico rounds each down to what it can do).
    const uint SPEEDS[] = {10'000'000, 20'000'000, 40'000'000, 75'000'000};
    const int SPEED_COUNT = sizeof SPEEDS / sizeof SPEEDS[0];
    int speed = 0;
    lcd::set_spi_hz(SPEEDS[speed]);

    solid_colors();
    report_screen();

    uint32_t prev = 0;
    float push_ms = 0.0f, fps = 0.0f;
    uint64_t fps_start = time_us_64();
    int fps_frames = 0;

    for (int frame = 0;; ++frame) {
        uint32_t held = 0;
        for (int i = 0; i < BUTTON_COUNT; ++i) {
            if (!gpio_get(BUTTONS[i].pin)) held |= 1u << i;  // pressed pulls the pin low
        }
        uint32_t pressed = held & ~prev;
        for (int i = 0; i < BUTTON_COUNT; ++i) {
            if (pressed & (1u << i)) std::printf("pressed %s\n", BUTTONS[i].name);
        }
        prev = held;

        if (pressed & (1u << 8)) {  // Y
            speed = (speed + 1) % SPEED_COUNT;
            lcd::set_spi_hz(SPEEDS[speed]);
        }

        float hz = 0.0f;
        for (int i = 0; i < BUTTON_COUNT; ++i) {
            if (held & (1u << i)) {
                hz = BUTTONS[i].note;
                break;
            }
        }
        tone_hz = hz;

        draw(held, frame, push_ms, fps);
        uint64_t t0 = time_us_64();
        lcd::start_frame(canvas.px);
        lcd::wait();
        push_ms = (time_us_64() - t0) / 1000.0f;

        ++fps_frames;
        uint64_t now = time_us_64();
        if (now - fps_start >= 1'000'000) {
            fps = fps_frames * 1e6f / (now - fps_start);
            std::printf("%.1f fps, frame sent in %.1f ms at %.1f MHz\n", fps, push_ms, lcd::spi_hz() / 1e6);
            report_screen();  // again every second, for a serial monitor that connects late
            fps_frames = 0;
            fps_start = now;
        }
    }
}
