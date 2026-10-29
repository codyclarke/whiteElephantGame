#pragma once
#include <cstdint>

// Art from PNGs in assets/, converted at build time by tools/png2sprite.py.
// Frames sit side by side in the image: frame f starts at x = f * w.
struct Sprite {
    const char* name;  // path under assets/ without ".png" or "_fN", e.g. "guests/manager"
    int w;             // width of ONE frame
    int h;
    int frames;
    const uint16_t* px;  // RGB565, row-major, (w * frames) pixels per row
};

// The sprite drawn from assets/<name>.png, or nullptr if there's no such PNG
// (the game then falls back to its built-in shapes).
const Sprite* find_sprite(const char* name);
