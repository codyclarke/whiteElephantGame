#pragma once
#include <cstdint>

#include "game/gfx.h"
#include "game/rng.h"

// The idle screen: a Christmas tree outside at night, slowly collecting snow.
//
// The world is a grid of 2x2-pixel cells. Flakes fall as free particles; when
// one lands it becomes a settled snow cell, and settled snow behaves like
// sand: it drops into gaps and slides off anything steeper than it can cling
// to, so drifts build on the branches, the topper and along the far edge of
// the ground. The ground itself is a field seen from above: nearer flakes
// fall in front of the tree and dust it at different depths, so it goes from
// bare dirt to patchy to white. Branches
// that get too heavy shed their snow as a clump. Wind gusts push the falling
// snow and drift the surface, and the snowfall comes and goes.
//
// generate() rolls a new scene: tree shape, lights, ornaments, topper, moon
// and stars are all random.
class SnowScene {
public:
    static constexpr int CELL = 2;
    static constexpr int GW = gfx::W / CELL;  // grid columns
    static constexpr int GH = gfx::H / CELL;  // grid rows

    void generate(Rng& rng);
    void update(float dt, Rng& rng);
    void draw(gfx::Canvas& c) const;

    // For tests.
    int snow_cells() const;
    int snow_on_tree() const;  // settled snow resting on the tree or topper
    int sheds() const { return sheds_; }
    int particles() const;
    float field_cover() const;  // 0 = bare ground, 1 = all white

private:
    enum Cell : uint8_t { EMPTY, GROUND, TREE, TRUNK, TOPPER, SNOW };
    static constexpr uint8_t MOVED = 0x80;  // set on snow that already moved this tick

    enum class Kind : uint8_t { Flake, Clump, Back, Front, Puff };
    struct Particle {
        float x, y;    // pixels
        float vx, vy;  // pixels per second
        float fall;    // speed it eases toward
        float phase;   // for the side-to-side flutter
        float life;    // puffs fade out
        Kind kind;
        bool alive;
    };
    static constexpr int MAX_PARTICLES = 1200;

    // The bottom rows hold the ground field (the ground never starts higher).
    static constexpr int FIELD_ROWS = 16;
    static constexpr int FIELD_TOP = GH - FIELD_ROWS;

    // A branch is one side of one tier; it sheds once its snow passes capacity.
    static constexpr int MAX_TIERS = 6;
    static constexpr int MAX_BRANCHES = MAX_TIERS * 2;

    static constexpr int MAX_LIGHTS = 48;
    static constexpr int MAX_ORNAMENTS = 16;
    static constexpr int MAX_STARS = 90;

    struct Light {
        uint8_t x, y;  // cell
        gfx::Color color;
        float rate, phase;
    };
    struct Ornament {
        uint8_t x, y;
        gfx::Color color;
    };
    struct Star {
        uint8_t x, y;  // pixel
        uint8_t big;
        gfx::Color color;
        float rate, phase;
    };

    uint8_t at(int x, int y) const;  // out of bounds counts as solid
    bool settle(int x, int y);
    void spawn(Kind kind, float x, float y, float vx, float vy, float fall, Rng& rng);
    void dust(int x, int y);
    void update_particles(float dt, Rng& rng);
    void update_settled(float dt, Rng& rng);
    void soften_field();
    void update_weather(float dt, Rng& rng);
    void check_branches(Rng& rng);
    void build_tree(Rng& rng);
    void decorate(Rng& rng);
    gfx::Color cell_color(int x, int y) const;

    uint8_t grid_[GH][GW] = {};
    gfx::Color tree_color_[GH][GW] = {};  // shading baked in at generate()
    gfx::Color sky_[GH] = {};

    // Per column: the top of the tree, which branch it belongs to (-1 = none).
    int8_t surface_branch_[GW] = {};
    uint8_t surface_y_[GW] = {};
    float capacity_[MAX_BRANCHES] = {};
    int tree_cx_ = GW / 2;
    uint8_t ground_top_[GW] = {};          // first ground row in each column
    uint8_t cover_[FIELD_ROWS][GW] = {};   // snow dusted on the field, 0..255
    uint8_t patch_[FIELD_ROWS][GW] = {};   // smooth noise: which spots whiten last

    Particle parts_[MAX_PARTICLES] = {};

    Light lights_[MAX_LIGHTS] = {};
    int light_count_ = 0;
    Ornament ornaments_[MAX_ORNAMENTS] = {};
    int ornament_count_ = 0;
    Star stars_[MAX_STARS] = {};
    int star_count_ = 0;

    int moon_x_ = 0, moon_y_ = 0, moon_r_ = 0;
    float moon_shadow_ = 0.0f;  // offset of the shadow circle, in moon radii

    float time_ = 0.0f;
    float intensity_ = 0.5f;  // 0..1 snowfall
    float intensity_target_ = 0.5f;
    float intensity_timer_ = 0.0f;
    float wind_ = 0.0f;  // pixels per second, + is to the right
    float base_wind_ = 0.0f;
    float gust_ = 0.0f;
    float gust_timer_ = 0.0f;
    float gust_left_ = 0.0f;
    float spawn_acc_ = 0.0f;
    float back_acc_ = 0.0f;
    float front_acc_ = 0.0f;
    float branch_timer_ = 0.0f;
    float shooting_timer_ = 0.0f;
    float shooting_t_ = -1.0f;  // < 0 = no shooting star right now
    float shooting_x_ = 0.0f, shooting_y_ = 0.0f, shooting_dx_ = 0.0f;
    int tick_ = 0;
    int sheds_ = 0;
};
