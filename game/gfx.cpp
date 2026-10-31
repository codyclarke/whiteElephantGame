#include "game/gfx.h"

#include <cmath>
#include <cstring>

#include "game/font5x7.h"

namespace gfx {

int text_width(const char* s, int scale) {
    int len = static_cast<int>(std::strlen(s));
    return len == 0 ? 0 : (len * 6 - 1) * scale;
}

void Canvas::clear(Color c) {
    fill_rect(0, 0, width_, SCREEN_H, c);
}

void Canvas::pixel(int x, int y, Color c) {
    if (x >= 0 && x < width_ && y >= 0 && y < SCREEN_H) px[y * SCREEN_W + left_ + x] = c;
}

void Canvas::fill_rect(int x, int y, int w, int h, Color c) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > width_ ? width_ : x + w;
    int y1 = y + h > SCREEN_H ? SCREEN_H : y + h;
    for (int yy = y0; yy < y1; ++yy) {
        Color* row = &px[yy * SCREEN_W + left_];
        for (int xx = x0; xx < x1; ++xx) row[xx] = c;
    }
}

void Canvas::rect(int x, int y, int w, int h, Color c) {
    fill_rect(x, y, w, 1, c);
    fill_rect(x, y + h - 1, w, 1, c);
    fill_rect(x, y, 1, h, c);
    fill_rect(x + w - 1, y, 1, h, c);
}

void Canvas::fill_circle(int cx, int cy, int r, Color c) {
    for (int dy = -r; dy <= r; ++dy) {
        int dx = static_cast<int>(std::sqrt(static_cast<float>(r * r + r - dy * dy)));
        fill_rect(cx - dx, cy + dy, dx * 2 + 1, 1, c);
    }
}

void Canvas::line(int x0, int y0, int x1, int y1, Color c, bool dotted) {
    // Bresenham: step along the longer axis, nudging the other as error builds.
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (int step = 0;; ++step) {
        if (!dotted || step % 2 == 0) pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = err * 2;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void Canvas::sprite(const Sprite* s, int x, int y, int frame, bool flip, const Color* ramp, int scale) {
    if (!s || s->frames <= 0) return;
    frame %= s->frames;
    if (frame < 0) frame += s->frames;
    const int stride = s->w * s->frames;
    for (int row = 0; row < s->h; ++row) {
        const uint16_t* src = s->px + row * stride + frame * s->w;
        for (int col = 0; col < s->w; ++col) {
            Color c = src[flip ? s->w - 1 - col : col];
            if (c == TRANSPARENT_KEY) continue;
            if (ramp) {
                for (int k = 0; k < 4; ++k) {
                    if (c == RAMP_KEYS[k]) {
                        c = ramp[k];
                        break;
                    }
                }
            }
            if (scale == 1) {
                pixel(x + col, y + row, c);
            } else {
                fill_rect(x + col * scale, y + row * scale, scale, scale, c);
            }
        }
    }
}

int Canvas::text(int x, int y, const char* s, Color c, int scale) {
    int cx = x;
    for (; *s; ++s) {
        unsigned ch = static_cast<unsigned char>(*s);
        if (ch < 32 || ch > 126) ch = '?';
        const uint8_t* glyph = FONT5X7[ch - 32];
        for (int col = 0; col < 5; ++col) {
            for (int row = 0; row < 7; ++row) {
                if ((glyph[col] >> row) & 1) {
                    fill_rect(cx + col * scale, y + row * scale, scale, scale, c);
                }
            }
        }
        cx += 6 * scale;
    }
    return cx - x;
}

void Canvas::text_centered(int cx, int y, const char* s, Color c, int scale) {
    text(cx - text_width(s, scale) / 2, y, s, c, scale);
}

int Canvas::text_wrapped(int cx, int y, const char* s, Color c, int max_width, int scale) {
    const int max_chars = (max_width + scale) / (6 * scale);
    const int line_height = 10 * scale;
    char line[64];
    int lines = 0;
    while (*s) {
        while (*s == ' ') ++s;
        if (!*s) break;

        // Take as many whole words as fit; split a word only if it alone is too long.
        int len = static_cast<int>(std::strlen(s));
        int take = len;
        if (len > max_chars) {
            take = max_chars;
            while (take > 0 && s[take] != ' ') --take;
            if (take == 0) take = max_chars;
        }
        if (take > static_cast<int>(sizeof(line)) - 1) take = sizeof(line) - 1;
        if (take < 1) take = 1;  // too narrow for even one letter: still make progress
        std::memcpy(line, s, take);
        line[take] = '\0';
        int end = take;
        while (end > 0 && line[end - 1] == ' ') line[--end] = '\0';

        text_centered(cx, y + lines * line_height, line, c, scale);
        ++lines;
        s += take;
    }
    return lines;
}

void Canvas::wave(int y0, int y1, float amplitude, float phase) {
    Color row[SCREEN_W];
    for (int y = y0; y <= y1 && y < SCREEN_H; ++y) {
        int shift = static_cast<int>(std::lround(amplitude * std::sin(phase + y * 0.07f)));
        if (shift == 0) continue;
        Color* line = &px[y * SCREEN_W + left_];
        std::memcpy(row, line, width_ * sizeof(Color));
        for (int x = 0; x < width_; ++x) {
            int sx = x - shift;
            if (sx < 0) sx = 0;
            if (sx >= width_) sx = width_ - 1;
            line[x] = row[sx];
        }
    }
}

}  // namespace gfx
