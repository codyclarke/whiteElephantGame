#include "game/snow.h"

#include <cmath>

#include "game/tuning.h"

using gfx::Canvas;
using gfx::Color;
using gfx::rgb;

namespace {

constexpr Color SNOW_TOP = rgb(248, 250, 255);
constexpr Color SNOW_MID = rgb(222, 230, 248);
constexpr Color SNOW_DEEP = rgb(196, 208, 236);

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
float approach(float rate, float dt) { return rate * dt > 1.0f ? 1.0f : rate * dt; }

Color mix(Color a, Color b, float t) {
    auto ch = [t](int x, int y) { return static_cast<int>(x + (y - x) * t); };
    int r = ch((a >> 11) & 31, (b >> 11) & 31);
    int g = ch((a >> 5) & 63, (b >> 5) & 63);
    int bl = ch(a & 31, b & 31);
    return static_cast<Color>((r << 11) | (g << 5) | bl);
}

// Repeatable per-cell noise, for needle texture.
uint32_t hash(int x, int y) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// Tree greens: highlight, light, mid, shadow.
constexpr Color TREE_GREENS[][4] = {
    {rgb(70, 150, 80), rgb(40, 115, 60), rgb(26, 84, 46), rgb(14, 52, 32)},    // fir
    {rgb(90, 150, 130), rgb(58, 118, 104), rgb(38, 86, 80), rgb(22, 54, 54)},  // blue spruce
    {rgb(110, 150, 60), rgb(76, 116, 40), rgb(52, 84, 30), rgb(30, 52, 20)},   // scraggly pine
};

constexpr Color LIGHT_SETS[][5] = {
    {rgb(255, 60, 60), rgb(255, 210, 60), rgb(80, 140, 255), rgb(255, 120, 220), rgb(255, 150, 40)},  // classic
    {rgb(255, 230, 160), rgb(255, 220, 140), rgb(255, 240, 190), rgb(255, 225, 150), rgb(255, 235, 175)},  // warm white
    {rgb(150, 210, 255), rgb(220, 240, 255), rgb(120, 180, 255), rgb(200, 230, 255), rgb(170, 220, 255)},  // icy
    {rgb(255, 50, 50), rgb(255, 210, 80), rgb(255, 50, 50), rgb(255, 210, 80), rgb(255, 255, 255)},    // red and gold
};

constexpr Color ORNAMENT_COLORS[] = {rgb(230, 40, 50), rgb(250, 200, 50), rgb(200, 210, 230),
                                     rgb(60, 110, 230), rgb(170, 70, 200)};

// Toppers, bottom row sitting on the tree's peak. Y gold, W white, S skin,
// R red, O dark red, . empty.
constexpr const char* TOPPERS[][5] = {
    {"..Y..", ".YYY.", "YYWYY", ".YYY.", "Y...Y"},  // star
    {".YYY.", "..S..", "WWWWW", ".WWW.", "WWWWW"},  // angel
    {"RR.RR", "RRORR", ".R.R.", ".R.R.", "....."},  // bow
};

Color topper_color(char ch) {
    switch (ch) {
        case 'Y': return rgb(255, 210, 60);
        case 'W': return rgb(245, 245, 255);
        case 'S': return rgb(240, 200, 160);
        case 'R': return rgb(220, 30, 40);
        case 'O': return rgb(150, 15, 25);
        default:  return 0;
    }
}

}  // namespace

// --- Building a scene -----------------------------------------------------------

void SnowScene::generate(Rng& rng) {
    // Start clean. (Field by field rather than *this = SnowScene(), which
    // would need a scene-sized temporary on the handheld's small stack.)
    for (int y = 0; y < GH; ++y) {
        for (int x = 0; x < GW; ++x) {
            grid_[y][x] = EMPTY;
            tree_color_[y][x] = 0;
        }
    }
    for (auto& row : cover_)
        for (uint8_t& c : row) c = 0;
    for (Particle& p : parts_) p.alive = false;
    light_count_ = ornament_count_ = star_count_ = 0;
    time_ = spawn_acc_ = back_acc_ = front_acc_ = branch_timer_ = gust_ = gust_left_ = 0.0f;
    shooting_t_ = -1.0f;
    tick_ = sheds_ = 0;

    // Sky: dark at the top, lighter toward the horizon, with a random tint.
    uint8_t tr = static_cast<uint8_t>(rng.below(8)), tb = static_cast<uint8_t>(rng.below(14));
    Color top = rgb(6 + tr, 8 + tr / 2, 26 + tb);
    Color horizon = rgb(30 + tr * 2, 36 + tr, 70 + tb * 2);
    for (int y = 0; y < GH; ++y) sky_[y] = mix(top, horizon, clampf(y / 105.0f, 0.0f, 1.0f));

    // Ground: gentle random hills, frozen dirt until the snow covers it.
    float f1 = rng.range(0.03f, 0.08f), p1 = rng.range(0.0f, 6.3f);
    float f2 = rng.range(0.1f, 0.2f), p2 = rng.range(0.0f, 6.3f);
    for (int x = 0; x < GW; ++x) {
        int top_y = 108 + static_cast<int>(std::lround(1.6f * std::sin(x * f1 + p1) + 0.8f * std::sin(x * f2 + p2)));
        ground_top_[x] = static_cast<uint8_t>(top_y);
        for (int y = top_y; y < GH; ++y) {
            grid_[y][x] = GROUND;
            // A little lighter toward the viewer.
            Color dirt = mix(rgb(22, 24, 38), rgb(40, 40, 56), static_cast<float>(y - top_y) / (GH - top_y));
            tree_color_[y][x] = (hash(x, y) & 3) ? dirt : gfx::dim(dirt);
        }
    }
    // Soft patches for the field, stretched sideways like ground seen at an
    // angle: blend random values on a coarse grid.
    uint32_t salt = rng.next();
    for (int y = 0; y < FIELD_ROWS; ++y) {
        for (int x = 0; x < GW; ++x) {
            const int SX = 7, SY = 3;
            int gx = x / SX, gy = y / SY;
            float fx = static_cast<float>(x % SX) / SX, fy = static_cast<float>(y % SY) / SY;
            auto v = [salt](int a, int b) { return static_cast<float>(hash(a + static_cast<int>(salt & 1023), b) & 255); };
            float upper = v(gx, gy) + (v(gx + 1, gy) - v(gx, gy)) * fx;
            float lower = v(gx, gy + 1) + (v(gx + 1, gy + 1) - v(gx, gy + 1)) * fx;
            patch_[y][x] = static_cast<uint8_t>(upper + (lower - upper) * fy);
        }
    }

    build_tree(rng);
    decorate(rng);

    // Moon and stars.
    moon_r_ = 7 + rng.below(5);
    moon_x_ = 24 + rng.below(192);
    moon_y_ = 18 + rng.below(40);
    moon_shadow_ = rng.range(0.7f, 2.4f) * (rng.chance(0.5f) ? 1.0f : -1.0f);  // never a new moon
    star_count_ = 40 + rng.below(MAX_STARS - 40);
    for (int i = 0; i < star_count_; ++i) {
        Star& s = stars_[i];
        s.x = static_cast<uint8_t>(rng.below(gfx::W));
        s.y = static_cast<uint8_t>(rng.below(150));
        s.big = rng.chance(0.12f) ? 1 : 0;
        int pick = rng.below(4);
        s.color = pick == 0 ? rgb(255, 240, 200) : (pick == 1 ? rgb(200, 220, 255) : rgb(235, 235, 245));
        s.rate = rng.range(0.6f, 3.0f);
        s.phase = rng.range(0.0f, 6.3f);
    }

    intensity_ = rng.range(0.2f, 0.5f);
    intensity_target_ = intensity_;
    intensity_timer_ = rng.range(4.0f, 10.0f);
    base_wind_ = rng.range(-6.0f, 6.0f);
    wind_ = base_wind_;
    gust_timer_ = rng.range(6.0f, 16.0f);
    shooting_timer_ = rng.range(10.0f, 35.0f);
}

void SnowScene::build_tree(Rng& rng) {
    tree_cx_ = GW / 2 + rng.below(17) - 8;
    int ground = 0;
    while (ground < GH && grid_[ground][tree_cx_] == EMPTY) ++ground;

    // Trunk.
    int trunk_w = 3 + rng.below(2);
    int trunk_h = 4 + rng.below(3);
    for (int y = ground - trunk_h; y < ground + 1; ++y) {
        for (int x = tree_cx_ - trunk_w / 2; x < tree_cx_ - trunk_w / 2 + trunk_w; ++x) {
            grid_[y][x] = TRUNK;
            tree_color_[y][x] = (x == tree_cx_ - trunk_w / 2) ? rgb(110, 72, 44) : rgb(78, 50, 32);
        }
    }

    // Foliage: overlapping tiers, each a lopsided triangle. The slopes are
    // kept shallow enough (over a cell sideways per cell down) that snow can
    // cling to them, and each tier pokes out past the one above as a ledge.
    const Color* green = TREE_GREENS[rng.below(3)];
    int tiers = 4 + rng.below(MAX_TIERS - 3);
    int bottom = ground - trunk_h;
    int height = 44 + rng.below(15);
    int top = bottom - height;
    float base_w = rng.range(19.0f, 26.0f);  // half-width of the bottom tier
    float slope = rng.range(1.15f, 1.5f);    // cells out per cell down
    float top_w = rng.range(6.0f, 9.0f);     // half-width of the top tier
    int overlap = 3 + rng.below(3);

    static int8_t owner[GH][GW];  // which tier each tree cell came from
    for (auto& row : owner)
        for (int8_t& o : row) o = -1;

    int peak_x = tree_cx_;
    for (int t = 0; t < tiers; ++t) {
        int yb = top + height * (t + 1) / tiers;
        int yt = t == 0 ? top : top + height * t / tiers - overlap;
        float w = top_w + (base_w - top_w) * t / (tiers - 1);
        float wl = w * rng.range(0.85f, 1.15f);
        float wr = w * rng.range(0.85f, 1.15f);
        float s = t == 0 ? w / (yb - yt + 1) : slope;  // the top tier comes to a point
        int cx = tree_cx_ + rng.below(3) - 1;
        if (t == 0) peak_x = cx;
        for (int y = yt; y <= yb; ++y) {
            float up = static_cast<float>(yb - y);
            int hl = static_cast<int>(std::lround(wl - s * up * wl / w));
            int hr = static_cast<int>(std::lround(wr - s * up * wr / w));
            // Narrower than that, it's just the dark middle showing between tiers.
            bool core = hl < 1 || hr < 1;
            if (hl < 1) hl = 1;
            if (hr < 1) hr = 1;
            for (int x = cx - hl; x <= cx + hr; ++x) {
                if (x < 0 || x >= GW || y < 0) continue;
                if (owner[y][x] < 0) owner[y][x] = static_cast<int8_t>(t);
                grid_[y][x] = TREE;
                // Lit from the left: highlight on the left edge, shadow on the right.
                float side = static_cast<float>(x - cx) / (hr + hl + 1) * 2.0f;
                int shade = side < -0.6f ? 1 : (side < 0.3f ? 2 : 3);
                uint32_t n = hash(x, y);
                if ((n & 7) == 0 && shade > 0) --shade;
                if ((n & 15) == 1 && shade < 3) ++shade;
                if (y == yb && (n & 1)) shade = 3;  // ragged dark underside
                if (x == cx - hl && side < 0.0f) shade = 0;
                if (core) shade = 3;
                tree_color_[y][x] = green[shade];
            }
        }
    }

    // Topper on the peak.
    const char* const* pattern = TOPPERS[rng.below(3)];
    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 5; ++col) {
            char ch = pattern[row][col];
            int x = peak_x - 2 + col, y = top - 5 + row;
            if (ch == '.' || x < 0 || x >= GW || y < 0) continue;
            grid_[y][x] = TOPPER;
            tree_color_[y][x] = topper_color(ch);
            owner[y][x] = 0;
        }
    }

    // Each column's top surface and which branch (tier and side) it belongs to.
    int count[MAX_BRANCHES] = {};
    for (int x = 0; x < GW; ++x) {
        int y = 0;
        while (y < GH && grid_[y][x] == EMPTY) ++y;
        surface_y_[x] = static_cast<uint8_t>(y);
        surface_branch_[x] = -1;
        if (y < GH && (grid_[y][x] == TREE || grid_[y][x] == TOPPER) && owner[y][x] >= 0) {
            int b = owner[y][x] * 2 + (x >= tree_cx_ ? 1 : 0);
            surface_branch_[x] = static_cast<int8_t>(b);
            ++count[b];
        }
    }
    for (int b = 0; b < MAX_BRANCHES; ++b) capacity_[b] = count[b] * rng.range(2.0f, 3.4f) + 2.0f;
}

void SnowScene::decorate(Rng& rng) {
    const Color* set = LIGHT_SETS[rng.below(4)];
    bool steady = rng.chance(0.3f);  // some trees just glow instead of twinkling

    auto far_from = [](int x, int y, const auto* list, int n, int gap) {
        for (int i = 0; i < n; ++i) {
            int dx = list[i].x - x, dy = list[i].y - y;
            if (dx < gap && dx > -gap && dy < gap && dy > -gap) return false;
        }
        return true;
    };

    int want = 24 + rng.below(MAX_LIGHTS - 24);
    for (int tries = 0; tries < 800 && light_count_ < want; ++tries) {
        int x = rng.below(GW), y = rng.below(GH);
        if (grid_[y][x] != TREE || !far_from(x, y, lights_, light_count_, 4)) continue;
        Light& l = lights_[light_count_++];
        l.x = static_cast<uint8_t>(x);
        l.y = static_cast<uint8_t>(y);
        l.color = set[rng.below(5)];
        l.rate = steady ? rng.range(0.2f, 0.5f) : rng.range(0.6f, 2.6f);
        l.phase = rng.range(0.0f, 6.3f);
    }

    want = 6 + rng.below(MAX_ORNAMENTS - 6);
    for (int tries = 0; tries < 800 && ornament_count_ < want; ++tries) {
        int x = rng.below(GW), y = rng.below(GH);
        if (grid_[y][x] != TREE || !far_from(x, y, lights_, light_count_, 2) ||
            !far_from(x, y, ornaments_, ornament_count_, 5)) {
            continue;
        }
        Ornament& o = ornaments_[ornament_count_++];
        o.x = static_cast<uint8_t>(x);
        o.y = static_cast<uint8_t>(y);
        o.color = ORNAMENT_COLORS[rng.below(5)];
    }
}

// --- Simulation -----------------------------------------------------------------

uint8_t SnowScene::at(int x, int y) const {
    if (x < 0 || x >= GW || y >= GH) return GROUND;
    if (y < 0) return EMPTY;
    return grid_[y][x];
}

// Turn a landed flake into settled snow at (x, y), or the first gap above it.
bool SnowScene::settle(int x, int y) {
    if (x < 0 || x >= GW) return false;
    for (; y >= 0; --y) {
        if (grid_[y][x] == EMPTY) {
            grid_[y][x] = SNOW;
            return true;
        }
    }
    return false;
}

void SnowScene::spawn(Kind kind, float x, float y, float vx, float vy, float fall, Rng& rng) {
    for (Particle& p : parts_) {
        if (p.alive) continue;
        p = {x, y, vx, vy, fall, rng.range(0.0f, 6.3f), 0.0f, kind, true};
        if (kind == Kind::Flake) p.life = rng.range(0.6f, 1.0f);  // how much the wind moves it
        if (kind == Kind::Puff) p.life = rng.range(0.4f, 0.8f);
        if (kind == Kind::Back) p.life = rng.range(60.0f, 220.0f);  // how far down it gets
        if (kind == Kind::Clump) p.life = 0.3f;  // drops through its own branch first
        if (kind == Kind::Front) p.life = rng.range(0.0f, 1.0f);  // how far into the field it lands
        return;
    }
}

// A near flake lands on the field. Nearer rows are bigger on screen, so a
// flake there dusts a wider patch.
void SnowScene::dust(int x, int y) {
    int top = ground_top_[x];
    float near = static_cast<float>(y - top) / (GH - 1 - top);
    int reach = near > 0.7f ? 2 : (near > 0.3f ? 1 : 0);
    auto add = [this](int cx, int cy, int amount) {
        if (cx < 0 || cx >= GW || cy < FIELD_TOP || cy >= GH || grid_[cy][cx] != GROUND) return;
        uint8_t& c = cover_[cy - FIELD_TOP][cx];
        c = static_cast<uint8_t>(c + amount > 255 ? 255 : c + amount);
    };
    add(x, y, 200);
    for (int d = 1; d <= reach; ++d) {
        add(x - d, y, d == 1 ? 140 : 90);
        add(x + d, y, d == 1 ? 140 : 90);
    }
    if (near > 0.8f) add(x, y + 1, 70);
}

void SnowScene::update(float dt, Rng& rng) {
    time_ += dt;
    ++tick_;
    update_weather(dt, rng);

    // New flakes come in across the top, upwind so the gusts don't leave gaps.
    spawn_acc_ += intensity_ * tuning::SNOW_FLAKES_PER_SECOND * dt;
    for (; spawn_acc_ >= 1.0f; spawn_acc_ -= 1.0f) {
        float fall = rng.range(14.0f, 24.0f);
        spawn(Kind::Flake, rng.range(-40.0f, 280.0f) - wind_ * 3.0f, -4.0f, wind_ * 0.8f, fall, fall, rng);
    }
    back_acc_ += intensity_ * tuning::SNOW_FLAKES_PER_SECOND * 1.2f * dt;
    for (; back_acc_ >= 1.0f; back_acc_ -= 1.0f) {
        float fall = rng.range(7.0f, 12.0f);
        spawn(Kind::Back, rng.range(-40.0f, 280.0f) - wind_ * 3.0f, -2.0f, wind_ * 0.5f, fall, fall, rng);
    }
    // Near flakes pass in front of the tree, fall faster, and land on the field.
    front_acc_ += intensity_ * tuning::SNOW_FLAKES_PER_SECOND * 0.8f * dt;
    for (; front_acc_ >= 1.0f; front_acc_ -= 1.0f) {
        float fall = rng.range(28.0f, 40.0f);
        spawn(Kind::Front, rng.range(-40.0f, 280.0f) - wind_ * 3.0f, -4.0f, wind_, fall, fall, rng);
    }

    update_particles(dt, rng);
    update_settled(dt, rng);
    soften_field();

    branch_timer_ -= dt;
    if (branch_timer_ <= 0.0f) {
        branch_timer_ = 0.25f;
        check_branches(rng);
    }

    // The occasional shooting star.
    if (shooting_t_ >= 0.0f) {
        shooting_t_ += dt;
        if (shooting_t_ > 0.8f) shooting_t_ = -1.0f;
    } else if ((shooting_timer_ -= dt) <= 0.0f) {
        shooting_timer_ = rng.range(15.0f, 40.0f);
        shooting_t_ = 0.0f;
        shooting_x_ = rng.range(30.0f, 210.0f);
        shooting_y_ = rng.range(8.0f, 50.0f);
        shooting_dx_ = rng.chance(0.5f) ? 1.0f : -1.0f;
    }
}

void SnowScene::update_weather(float dt, Rng& rng) {
    // Snowfall drifts between flurries and heavy snow.
    if ((intensity_timer_ -= dt) <= 0.0f) {
        intensity_target_ = rng.range(0.1f, 1.0f);
        intensity_timer_ = rng.range(8.0f, 20.0f);
    }
    intensity_ += (intensity_target_ - intensity_) * approach(0.3f, dt);

    // A light breeze that wanders, plus the odd gust.
    base_wind_ = clampf(base_wind_ + rng.range(-1.0f, 1.0f) * 4.0f * dt, -8.0f, 8.0f);
    if (gust_left_ > 0.0f) {
        gust_left_ -= dt;
    } else if ((gust_timer_ -= dt) <= 0.0f) {
        gust_ = (rng.chance(0.5f) ? -1.0f : 1.0f) * rng.range(0.5f, 1.0f) * tuning::SNOW_GUST_MAX;
        gust_left_ = rng.range(1.5f, 4.0f);
        gust_timer_ = rng.range(8.0f, 20.0f);
    }
    float target = base_wind_ + (gust_left_ > 0.0f ? gust_ : 0.0f);
    wind_ += (target - wind_) * approach(1.2f, dt);
}

void SnowScene::update_particles(float dt, Rng& rng) {
    for (Particle& p : parts_) {
        if (!p.alive) continue;

        if (p.kind == Kind::Puff) {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.vx *= 1.0f - approach(3.0f, dt);
            p.life -= dt;
            if (p.life <= 0.0f) p.alive = false;
            continue;
        }

        float px = p.x, py = p.y;
        float wind_target = p.kind == Kind::Flake   ? wind_ * p.life
                            : p.kind == Kind::Back  ? wind_ * 0.5f
                            : p.kind == Kind::Front ? wind_
                                                    : wind_ * 0.3f;
        p.vx += (wind_target - p.vx) * approach(p.kind == Kind::Clump ? 0.8f : 1.5f, dt);
        p.vy += (p.fall - p.vy) * approach(3.0f, dt);
        float flutter = p.kind == Kind::Clump ? 0.0f : std::sin(time_ * 2.0f + p.phase) * 6.0f;
        p.x += (p.vx + flutter) * dt;
        p.y += p.vy * dt;

        if (p.y >= gfx::H || p.x < -80.0f || p.x > gfx::W + 80.0f) {
            p.alive = false;
            continue;
        }
        if (p.kind == Kind::Back) {  // far away: never lands, just fades out
            if (p.y > p.life) p.alive = false;
            continue;
        }
        if (p.kind == Kind::Front) {  // close up: misses everything until it reaches its spot on the field
            int fx = static_cast<int>(std::floor(p.x / CELL));
            if (fx < 0 || fx >= GW) continue;
            int top = ground_top_[fx];
            int row = top + static_cast<int>(p.life * (GH - top));
            if (row > GH - 1) row = GH - 1;
            if (p.y >= row * CELL + 1) {
                dust(fx, row);
                p.alive = false;
            }
            continue;
        }
        if (p.kind == Kind::Clump && p.life > 0.0f) {
            p.life -= dt;
            continue;
        }

        int cx = static_cast<int>(std::floor(p.x / CELL));
        int cy = static_cast<int>(std::floor(p.y / CELL));
        if (cy < 0 || cx < 0 || cx >= GW || grid_[cy][cx] == EMPTY) continue;

        // Landed: settle where it just was, or on top of whatever it hit.
        int pcx = static_cast<int>(std::floor(px / CELL));
        int pcy = static_cast<int>(std::floor(py / CELL));
        bool was_clear = pcx >= 0 && pcx < GW && pcy >= 0 && pcy < GH && grid_[pcy][pcx] == EMPTY;
        if (was_clear) {
            grid_[pcy][pcx] = SNOW;
        } else if (p.kind == Kind::Clump) {
            continue;  // still falling through the needles
        } else {
            settle(cx, cy - 1);
        }
        p.alive = false;
    }
    (void)rng;
}

// Settled snow acts like sticky sand.
void SnowScene::update_settled(float dt, Rng& rng) {
    bool rightward = (tick_ & 1) != 0;
    float drift = std::fabs(wind_) > 15.0f ? (std::fabs(wind_) - 15.0f) * 0.0008f : 0.0f;
    int downwind = wind_ > 0.0f ? 1 : -1;
    (void)dt;

    for (int y = GH - 2; y >= 0; --y) {
        for (int k = 0; k < GW; ++k) {
            int x = rightward ? k : GW - 1 - k;
            if (grid_[y][x] != SNOW) continue;  // also skips snow that already moved

            // Fall into a gap.
            if (at(x, y + 1) == EMPTY) {
                grid_[y][x] = EMPTY;
                grid_[y + 1][x] = SNOW | MOVED;
                continue;
            }
            // Slide off an edge -- but snow clings, so only off a real drop,
            // with a rare slump down a single step.
            int d = (rng.next() & 1) ? 1 : -1;
            bool moved = false;
            for (int attempt = 0; attempt < 2 && !moved; ++attempt, d = -d) {
                if (at(x + d, y) != EMPTY || at(x + d, y + 1) != EMPTY) continue;
                if (at(x + d, y + 2) == EMPTY || rng.chance(0.004f)) {
                    grid_[y][x] = EMPTY;
                    grid_[y + 1][x + d] = SNOW | MOVED;
                    moved = true;
                }
            }
            // Strong wind skates surface snow downwind.
            if (!moved && drift > 0.0f && at(x, y - 1) == EMPTY && at(x + downwind, y) == EMPTY &&
                rng.chance(drift)) {
                grid_[y][x] = EMPTY;
                grid_[y][x + downwind] = SNOW | MOVED;
            }
        }
    }
    for (auto& row : grid_)
        for (uint8_t& c : row) c &= static_cast<uint8_t>(~MOVED);
}

// Snow on the field spreads a little each pass, so landings blend into soft
// patches instead of dots. One row per tick keeps it cheap.
void SnowScene::soften_field() {
    uint8_t* row = cover_[tick_ % FIELD_ROWS];
    int left = row[0];
    for (int x = 0; x < GW; ++x) {
        int right = x + 1 < GW ? row[x + 1] : row[x];
        int here = row[x];
        row[x] = static_cast<uint8_t>((left + here * 2 + right + 2) / 4);
        left = here;
    }
}

// Weigh the snow on each branch; an overloaded one dumps it.
void SnowScene::check_branches(Rng& rng) {
    float load[MAX_BRANCHES] = {};
    for (int x = 0; x < GW; ++x) {
        int b = surface_branch_[x];
        if (b < 0) continue;
        for (int y = surface_y_[x] - 1; y >= 0 && grid_[y][x] == SNOW; --y) load[b] += 1.0f;
    }

    float gusting = gust_left_ > 0.0f ? 0.75f : 1.0f;  // gusts shake branches loose
    for (int b = 0; b < MAX_BRANCHES; ++b) {
        if (load[b] <= capacity_[b] * gusting) continue;

        int outward = (b & 1) ? 1 : -1;
        int columns = 0;
        for (int x = 0; x < GW; ++x) {
            if (surface_branch_[x] != b) continue;
            ++columns;
            for (int y = surface_y_[x] - 1; y >= 0 && grid_[y][x] == SNOW; --y) {
                grid_[y][x] = EMPTY;
                float px = x * CELL + 1.0f, py = y * CELL + 1.0f;
                spawn(Kind::Clump, px, py, outward * rng.range(6.0f, 18.0f) + wind_ * 0.3f, rng.range(0.0f, 10.0f),
                      rng.range(45.0f, 55.0f), rng);
                if (rng.chance(0.5f)) {
                    spawn(Kind::Puff, px, py, rng.range(-14.0f, 14.0f), rng.range(-10.0f, 2.0f), 0.0f, rng);
                }
            }
        }
        capacity_[b] = columns * rng.range(2.0f, 3.4f) + 2.0f;
        ++sheds_;
        return;  // one branch at a time; the clump may set off the next one down
    }
}

// --- Drawing --------------------------------------------------------------------

Color SnowScene::cell_color(int x, int y) const {
    if (grid_[y][x] == GROUND && y >= FIELD_TOP) {
        // Some patches whiten later than others, so the field goes patchy
        // first; a few cells of grit keep the patch edges from looking smooth.
        int patch = patch_[y - FIELD_TOP][x];
        int v = cover_[y - FIELD_TOP][x] - patch * 2 / 5 - static_cast<int>(hash(x, y) % 16);
        if (v < 15) return tree_color_[y][x];
        if (v < 45) return mix(tree_color_[y][x], SNOW_DEEP, 0.55f);
        if (v < 85) return SNOW_DEEP;
        // Settled: bluer in the distance, brighter up close, shadowed in the
        // low spots.
        float near = static_cast<float>(y - ground_top_[x]) / (GH - 1 - ground_top_[x]);
        if (near < 0.2f || patch > 190) return SNOW_MID;
        return SNOW_TOP;
    }
    if (grid_[y][x] != SNOW) return tree_color_[y][x];
    if (at(x, y - 1) == EMPTY) return SNOW_TOP;
    if (at(x, y - 2) == EMPTY) return SNOW_MID;
    return SNOW_DEEP;
}

void SnowScene::draw(Canvas& c) const {
    for (int y = 0; y < GH; ++y) c.fill_rect(0, y * CELL, gfx::W, CELL, sky_[y]);

    // Stars twinkle.
    for (int i = 0; i < star_count_; ++i) {
        const Star& s = stars_[i];
        float b = std::sin(time_ * s.rate + s.phase);
        Color col = b > 0.6f ? s.color : (b < -0.6f ? gfx::dim(s.color) : mix(gfx::dim(s.color), s.color, 0.5f));
        c.pixel(s.x, s.y, col);
        if (s.big && b > 0.6f) {
            Color glow = gfx::dim(s.color);
            c.pixel(s.x - 1, s.y, glow);
            c.pixel(s.x + 1, s.y, glow);
            c.pixel(s.x, s.y - 1, glow);
            c.pixel(s.x, s.y + 1, glow);
        }
    }

    // Shooting star: a bright head with a fading tail.
    if (shooting_t_ >= 0.0f) {
        float t = shooting_t_ / 0.8f;
        int hx = static_cast<int>(shooting_x_ + shooting_dx_ * 120.0f * t);
        int hy = static_cast<int>(shooting_y_ + 45.0f * t);
        for (int i = 0; i < 16; ++i) {
            int x = hx - static_cast<int>(shooting_dx_ * i * 1.6f), y = hy - i * 6 / 10;
            c.pixel(x, y, i < 3 ? rgb(255, 255, 240) : (i < 8 ? rgb(170, 180, 210) : rgb(90, 96, 130)));
        }
    }

    // Moon, with a soft glow and its phase.
    Color sky = sky_[moon_y_ / CELL];
    c.fill_circle(moon_x_, moon_y_, moon_r_ + 3, mix(sky, rgb(120, 130, 160), 0.25f));
    float sx = moon_x_ + moon_shadow_ * moon_r_;
    for (int dy = -moon_r_; dy <= moon_r_; ++dy) {
        for (int dx = -moon_r_; dx <= moon_r_; ++dx) {
            if (dx * dx + dy * dy > moon_r_ * moon_r_) continue;
            float ox = moon_x_ + dx - sx;
            bool shadow = ox * ox + dy * dy <= moon_r_ * moon_r_;
            Color col = shadow ? mix(sky, rgb(60, 66, 90), 0.4f)
                               : ((hash(dx + 40, dy + 40) % 9 == 0) ? rgb(220, 218, 190) : rgb(250, 246, 220));
            c.pixel(moon_x_ + dx, moon_y_ + dy, col);
        }
    }

    // Far-off flakes go behind everything.
    for (const Particle& p : parts_) {
        if (p.alive && p.kind == Kind::Back) c.pixel(static_cast<int>(p.x), static_cast<int>(p.y), rgb(120, 130, 160));
    }

    // The world: ground, tree, topper and settled snow.
    for (int y = 0; y < GH; ++y) {
        for (int x = 0; x < GW; ++x) {
            if (grid_[y][x] != EMPTY) c.fill_rect(x * CELL, y * CELL, CELL, CELL, cell_color(x, y));
        }
    }

    // Ornaments, with a glint.
    for (int i = 0; i < ornament_count_; ++i) {
        const Ornament& o = ornaments_[i];
        int x = o.x * CELL, y = o.y * CELL;
        c.fill_rect(x, y, 2, 2, o.color);
        c.pixel(x, y, rgb(255, 255, 255));
    }

    // Lights twinkle and glow.
    for (int i = 0; i < light_count_; ++i) {
        const Light& l = lights_[i];
        int x = l.x * CELL, y = l.y * CELL;
        float b = std::sin(time_ * l.rate + l.phase);
        if (b > -0.2f) {
            c.fill_rect(x, y, 2, 2, l.color);
            if (b > 0.5f) {
                Color glow = gfx::dim(l.color);
                c.fill_rect(x - 1, y, 1, 2, glow);
                c.fill_rect(x + 2, y, 1, 2, glow);
                c.fill_rect(x, y - 1, 2, 1, glow);
                c.fill_rect(x, y + 2, 2, 1, glow);
            }
        } else {
            c.fill_rect(x, y, 2, 2, gfx::dim(gfx::dim(l.color)));
        }
    }

    // Falling snow in front.
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        const Particle& p = parts_[i];
        if (!p.alive || p.kind == Kind::Back) continue;
        int x = static_cast<int>(p.x), y = static_cast<int>(p.y);
        if (p.kind == Kind::Puff) {
            c.pixel(x, y, p.life > 0.3f ? SNOW_MID : SNOW_DEEP);
        } else if (p.kind == Kind::Clump || p.kind == Kind::Front || i % 3 == 0) {
            c.fill_rect(x, y, 2, 2, SNOW_TOP);
        } else {
            c.pixel(x, y, SNOW_TOP);
        }
    }
}

// --- For tests ------------------------------------------------------------------

int SnowScene::snow_cells() const {
    int n = 0;
    for (const auto& row : grid_)
        for (uint8_t c : row) n += c == SNOW;
    return n;
}

int SnowScene::snow_on_tree() const {
    int n = 0;
    for (int y = 1; y < GH; ++y) {
        for (int x = 0; x < GW; ++x) {
            if (grid_[y - 1][x] == SNOW && (grid_[y][x] == TREE || grid_[y][x] == TOPPER)) ++n;
        }
    }
    return n;
}

float SnowScene::field_cover() const {
    int total = 0, cells = 0;
    for (int y = FIELD_TOP; y < GH; ++y) {
        for (int x = 0; x < GW; ++x) {
            if (grid_[y][x] != GROUND) continue;
            total += cover_[y - FIELD_TOP][x];
            ++cells;
        }
    }
    return cells ? static_cast<float>(total) / (cells * 255.0f) : 0.0f;
}

int SnowScene::particles() const {
    int n = 0;
    for (const Particle& p : parts_) n += p.alive;
    return n;
}
