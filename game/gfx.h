#pragma once
#include <cstdint>

#include "game/sprites.h"

// Software drawing into an RGB565 frame buffer the size of the handheld's
// 320x240 screen. RGB565 is the screen's native format, so the handheld can
// send this buffer straight out.
//
// The game itself is 240x240 in the middle (W x H below), with a 40-pixel
// strip on each side. Drawing normally lands in the game area, in game
// coordinates; set_area() points it at a strip instead.
namespace gfx {

constexpr int W = 240;  // the game area
constexpr int H = 240;
constexpr int SCREEN_W = 320;  // the whole screen
constexpr int SCREEN_H = 240;
constexpr int STRIP_W = (SCREEN_W - W) / 2;  // each side strip

using Color = uint16_t;

constexpr Color rgb(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<Color>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Sprite pixels with this value are see-through (magenta, set by png2sprite.py).
constexpr Color TRANSPARENT_KEY = 0xF81F;

// Palette swap: art drawn in exactly these four grays can be recolored at
// draw time with a 4-color ramp (highlight, light, mid, shadow). Any other
// color in the art is drawn as-is.
constexpr Color RAMP_KEYS[4] = {rgb(255, 255, 255), rgb(192, 192, 192), rgb(128, 128, 128), rgb(64, 64, 64)};

// Half brightness, for dim backgrounds and inactive segments.
constexpr Color dim(Color c) { return static_cast<Color>((c >> 1) & 0x7BEF); }

// Width in pixels of a string drawn with Canvas::text.
int text_width(const char* s, int scale = 1);

struct Canvas {
    Color px[SCREEN_W * SCREEN_H];

    // Where drawing goes: x = 0 is screen column `left`, and anything outside
    // `width` columns is clipped. game_area() puts it back.
    void set_area(int left, int width) {
        left_ = left;
        width_ = width;
    }
    void game_area() { set_area(STRIP_W, W); }

    void clear(Color c);  // just the current area
    void pixel(int x, int y, Color c);
    void fill_rect(int x, int y, int w, int h, Color c);
    void rect(int x, int y, int w, int h, Color c);  // 1px outline
    void fill_circle(int cx, int cy, int r, Color c);
    void line(int x0, int y0, int x1, int y1, Color c, bool dotted = false);

    // Draw one frame of a sprite with its top-left corner at x, y.
    // flip mirrors it left-to-right. ramp (4 colors) recolors the RAMP_KEYS
    // grays; scale draws each pixel as a scale x scale block.
    void sprite(const Sprite* s, int x, int y, int frame = 0, bool flip = false, const Color* ramp = nullptr,
                int scale = 1);

    // 5x7 font, 6px per character at scale 1. Returns the width drawn.
    int text(int x, int y, const char* s, Color c, int scale = 1);
    void text_centered(int cx, int y, const char* s, Color c, int scale = 1);

    // Centered text that breaks onto new lines at spaces to fit max_width.
    // Returns the number of lines drawn.
    int text_wrapped(int cx, int y, const char* s, Color c, int max_width, int scale = 1);

    // Shift rows y0..y1 sideways along a sine wave (the drunk sway).
    void wave(int y0, int y1, float amplitude, float phase);

private:
    int left_ = STRIP_W;
    int width_ = W;
};

}  // namespace gfx
