#include "game/game.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "game/ai.h"
#include "game/audio.h"
#include "game/tuning.h"
#include "platform/platform.h"

using namespace gfx;
using namespace platform;

namespace {

// --- Palette ----------------------------------------------------------------
constexpr Color COL_BG     = rgb(28, 22, 44);
constexpr Color COL_RUG    = rgb(74, 32, 40);
constexpr Color COL_HUD    = rgb(12, 10, 20);
constexpr Color COL_TEXT   = rgb(240, 236, 220);
constexpr Color COL_DIM    = rgb(130, 126, 140);
constexpr Color COL_GOLD   = rgb(250, 210, 60);
constexpr Color COL_SOBER  = rgb(90, 120, 200);
constexpr Color COL_SWEET  = rgb(60, 210, 100);
constexpr Color COL_DRUNK  = rgb(245, 150, 40);
constexpr Color COL_DANGER = rgb(230, 50, 50);
constexpr Color COL_GHOST  = rgb(210, 210, 235);
constexpr Color COL_ICE    = rgb(150, 220, 255);
constexpr Color COL_TREE   = rgb(30, 140, 70);
constexpr Color COL_TRUNK  = rgb(110, 70, 40);
constexpr Color COL_SKIN   = rgb(240, 200, 160);
constexpr Color COL_BEER   = rgb(240, 180, 40);
constexpr Color COL_SHY    = rgb(130, 130, 170);
constexpr Color COL_CHARM  = rgb(255, 120, 190);
constexpr Color COL_RUDE   = rgb(235, 80, 40);
constexpr Color COL_STRIP  = rgb(14, 10, 22);   // behind the side strings of lights
constexpr Color COL_WIRE   = rgb(24, 64, 34);
constexpr Color COL_SOCKET = rgb(40, 96, 52);

constexpr Color BULB_COLORS[] = {rgb(255, 60, 60), rgb(255, 200, 50), rgb(70, 130, 255), rgb(60, 220, 110),
                                 rgb(255, 110, 210)};
constexpr float STEAL_FLASH_SECONDS = 1.5f;
constexpr Color GIFT_COLORS[] = {
    rgb(220, 40, 60), rgb(40, 120, 220), rgb(240, 200, 60), rgb(160, 80, 200), rgb(40, 180, 120),
};

// --- Layout -----------------------------------------------------------------
constexpr int HUD_H = 34;  // tall enough for the needle gauges
constexpr float SEAT_RADIUS_Y = 68.0f;
constexpr int TICKER_Y = 224;
constexpr int ROOM_CX = 120;
constexpr int ROOM_CY = 124;

// Where wrapped gifts sit under the tree: dx, dy, w, h from the tree center.
constexpr int PILE[Party::MAX_SEATS][4] = {
    {-31, 17, 14, 11}, {-14, 21, 11, 9}, {3, 19, 12, 10}, {18, 16, 13, 12},
    {-24, 26, 10, 8},  {10, 27, 11, 8},  {-6, 28, 9, 7},  {24, 28, 9, 7},
};

Color zone_color(float buzz) {
    if (buzz < tuning::SWEET_SPOT_LOW) return COL_SOBER;
    if (buzz <= tuning::SWEET_SPOT_HIGH) return COL_SWEET;
    if (buzz < tuning::BLACKOUT) return COL_DRUNK;
    return COL_DANGER;
}

Color vibe_color(float vibe) {
    if (vibe < tuning::VIBE_WALLFLOWER) return COL_SHY;
    if (vibe > tuning::VIBE_OBNOXIOUS) return COL_RUDE;
    return COL_CHARM;
}

// --- Needle gauges ----------------------------------------------------------
// A half dial. Angles are degrees from straight up (-90 far left, +90 far
// right). The meter's good band [lo, hi] always spans -30..+30, so the goal
// is the same on every gauge: keep the needle pointing up.
constexpr float RAD = 3.14159265f / 180.0f;

float dial_angle(float v, float lo, float hi) {
    if (v < lo) return -90.0f + v / lo * 60.0f;
    if (v <= hi) return -30.0f + (v - lo) / (hi - lo) * 60.0f;
    return 30.0f + (v - hi) / (100.0f - hi) * 60.0f;
}

float dial_value(float angle, float lo, float hi) {
    if (angle < -30.0f) return (angle + 90.0f) / 60.0f * lo;
    if (angle <= 30.0f) return lo + (angle + 30.0f) / 60.0f * (hi - lo);
    return hi + (angle - 30.0f) / 60.0f * (100.0f - hi);
}

// The colored arc, plus white ticks at the zone edges.
void draw_dial(Canvas& c, int cx, int cy, int r, float lo, float hi, Color (*zone)(float),
               std::initializer_list<float> ticks) {
    const int band = 6;
    for (int y = cy - r; y <= cy; ++y) {
        for (int x = cx - r; x <= cx + r; ++x) {
            float dx = static_cast<float>(x - cx);
            float dy = static_cast<float>(cy - y);
            float d = std::sqrt(dx * dx + dy * dy);
            if (d > r + 0.5f || d < r - band) continue;
            float angle = std::atan2(dx, dy) / RAD;
            c.pixel(x, y, dim(zone(dial_value(angle, lo, hi))));
        }
    }
    for (float t : ticks) {
        float a = dial_angle(t, lo, hi) * RAD;
        c.line(cx + static_cast<int>(std::lround(std::sin(a) * (r - band - 2))),
               cy - static_cast<int>(std::lround(std::cos(a) * (r - band - 2))),
               cx + static_cast<int>(std::lround(std::sin(a) * r)),
               cy - static_cast<int>(std::lround(std::cos(a) * r)), COL_TEXT);
    }
}

void draw_needle(Canvas& c, int cx, int cy, int len, float angle_deg, Color col, bool dotted) {
    float a = angle_deg * RAD;
    int x1 = cx + static_cast<int>(std::lround(std::sin(a) * len));
    int y1 = cy - static_cast<int>(std::lround(std::cos(a) * len));
    c.line(cx, cy, x1, y1, col, dotted);
    if (!dotted) c.line(cx + 1, cy, x1 + 1, y1, col);  // 2px thick
}

void draw_gift(Canvas& c, int x, int y, int w, int h, Color box) {
    int ribbon = w / 8 < 1 ? 1 : w / 8;
    c.fill_rect(x, y, w, h, box);
    c.fill_rect(x + (w - ribbon) / 2, y, ribbon, h, COL_GOLD);
    c.fill_rect(x, y + (h - ribbon) / 2, w, ribbon, COL_GOLD);
    // bow
    c.fill_rect(x + w / 2 - ribbon * 2, y - ribbon * 2, ribbon * 2, ribbon * 2, COL_GOLD);
    c.fill_rect(x + w / 2 + ribbon / 2, y - ribbon * 2, ribbon * 2, ribbon * 2, COL_GOLD);
}

// 0xRRGGBB (how content.h writes colors) to a screen color.
Color hex(uint32_t rgb_hex) {
    return rgb(static_cast<uint8_t>(rgb_hex >> 16), static_cast<uint8_t>(rgb_hex >> 8),
               static_cast<uint8_t>(rgb_hex));
}

void to_ramp(const content::Ramp& r, Color out[4]) {
    for (int i = 0; i < 4; ++i) out[i] = hex(r.shade[i]);
}

// A 5x5 triangle with its top-left at (x, y), the narrow tip pointing
// (dx, dy) -- a joystick direction.
void draw_arrow(Canvas& c, int x, int y, int dx, int dy, Color col) {
    for (int i = 0; i < 3; ++i) {
        int len = 5 - i * 2;
        if (dy != 0) {
            c.fill_rect(x + i, dy < 0 ? y + 4 - i : y + i, len, 1, col);
        } else {
            c.fill_rect(dx < 0 ? x + 4 - i : x + i, y + i, 1, len, col);
        }
    }
}

bool blink(float t, float per_second) {
    return static_cast<int>(t * per_second * 2.0f) % 2 == 0;
}

// Draw text at scale 2 if it fits in max_width, otherwise at scale 1.
void text_fit(Canvas& c, int cx, int y, const char* s, Color col, int max_width) {
    if (text_width(s, 2) <= max_width) {
        c.text_centered(cx, y, s, col, 2);
    } else {
        c.text_centered(cx, y + 4, s, col);
    }
}

// Built-in drink art, by look, for drinks without a PNG yet. Top-center at
// (cx, top), about 16x20.
void draw_drink_look(Canvas& c, int cx, int top, const char* look) {
    auto is = [look](const char* name) { return std::strcmp(look, name) == 0; };
    const Color glass = dim(dim(COL_TEXT));
    if (is("shot")) {
        c.fill_rect(cx - 3, top + 10, 7, 10, glass);
        c.fill_rect(cx - 2, top + 13, 5, 6, rgb(230, 220, 190));
        c.fill_rect(cx - 3, top + 10, 7, 1, COL_TEXT);
    } else if (is("wine")) {
        c.fill_rect(cx - 5, top + 2, 11, 9, glass);              // bowl
        c.fill_rect(cx - 4, top + 6, 9, 4, rgb(150, 20, 50));    // wine
        c.fill_rect(cx, top + 11, 1, 6, glass);                  // stem
        c.fill_rect(cx - 3, top + 17, 7, 2, glass);              // foot
        c.fill_rect(cx - 5, top + 2, 11, 1, COL_TEXT);
    } else if (is("punch")) {
        c.fill_rect(cx - 6, top + 6, 13, 12, glass);
        c.fill_rect(cx - 5, top + 9, 11, 8, rgb(240, 70, 140));
        c.fill_rect(cx + 2, top + 1, 1, 8, COL_TEXT);  // straw
        c.fill_rect(cx - 6, top + 6, 13, 1, COL_TEXT);
    } else {
        Color liquid = is("water") ? COL_ICE : is("nog") ? rgb(245, 235, 200) : is("beer") ? COL_BEER : COL_DIM;
        c.fill_rect(cx - 5, top + 2, 11, 17, glass);
        c.fill_rect(cx - 4, top + 7, 9, 11, liquid);
        if (is("beer")) c.fill_rect(cx - 4, top + 5, 9, 2, COL_TEXT);  // foam
        c.fill_rect(cx - 5, top + 2, 11, 1, COL_TEXT);
    }
}

// Art named after something in the game: assets/<folder>/<name>.png, with the
// name in lowercase and spaces as underscores ("Sales Guy" -> guests/sales_guy).
const Sprite* named_sprite(const char* folder, const char* label) {
    char name[48];
    std::snprintf(name, sizeof(name), "%s/", folder);
    size_t n = std::strlen(name);
    for (const char* s = label; *s && n < sizeof(name) - 1; ++s) {
        char ch = *s;
        if (ch == ' ') ch = '_';
        else if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        name[n++] = ch;
    }
    name[n] = '\0';
    return find_sprite(name);
}

}  // namespace

Game::Game(uint32_t seed) : rng_(seed) {
    buzz_.reset();
    spr_room_ = find_sprite("room");
    spr_tree_ = find_sprite("tree");
    spr_gift_wrapped_ = find_sprite("gift_wrapped");
    spr_gift_open_ = find_sprite("gift_open");
    spr_couch_ = find_sprite("couch");
    spr_hand_ = find_sprite("hand");
    // A drink's own art (drinks/good_wine.png) wins over its look's (drinks/wine.png).
    for (int d = 0; d < DRINK_COUNT; ++d) {
        spr_drink_[d] = named_sprite("drinks", tuning::DRINKS[d].name);
        if (!spr_drink_[d]) spr_drink_[d] = named_sprite("drinks", tuning::DRINKS[d].look);
    }

    // People parts: assets/people/body_f4.png, hair/<style>.png, sweaters/<style>.png, hats/<hat>.png.
    spr_body_ = find_sprite("people/body");
    for (int i = 0; i < HAIR_COUNT; ++i) spr_hair_[i] = named_sprite("people/hair", content::HAIR_STYLES[i]);
    for (int i = 0; i < SWEATER_COUNT; ++i) spr_sweater_[i] = named_sprite("people/sweaters", content::SWEATERS[i]);
    for (int i = 0; i < HAT_COUNT; ++i) spr_hat_[i] = named_sprite("people/hats", content::HATS[i].name);

    load();
    apply_volume();
    audio::play_music("title");
}

// --- Flow ---------------------------------------------------------------------

void Game::set_state(State s) {
    if (state_ == State::Sleep && s != State::Sleep) platform::set_backlight(true);
    state_ = s;
    state_time_ = 0.0f;
    idle_time_ = 0.0f;

    // Each screen has its own loop in assets/music/ (silence if there's no WAV).
    switch (s) {
        case State::Title:      audio::play_music("title"); break;
        case State::Builder:    break;  // keep the title music going
        case State::Party:      audio::play_music("party"); break;
        case State::Paused:     break;  // keep the party music going
        case State::Scratch:    audio::play_music("scratch"); break;
        case State::Results:    audio::play_music("results"); break;
        case State::SeasonOver: audio::play_music("season_over"); break;
        case State::CutOff:     audio::play_music("cut_off"); break;
        case State::Initials:   break;  // keep the season-over or cut-off music
        case State::HighScores: break;
        case State::Idle:       audio::play_music("idle"); break;
        case State::Sleep:
            audio::stop_music();
            platform::set_backlight(false);
            break;
    }
}

void Game::update(const Input& raw, float dt) {
    // The volume buttons work on every screen and are never game input --
    // except that, like any button, they wake the screen from sleep.
    Input in = raw;
    if (state_ != State::Sleep) {
        update_volume(raw, dt);
        in.held &= ~(BTN_VOL_UP | BTN_VOL_DOWN);
        in.pressed &= ~(BTN_VOL_UP | BTN_VOL_DOWN);
    }

    state_time_ += dt;
    update_lights(dt);

    // Left alone on the title or builder, the screen goes to the snowy tree.
    if (state_ == State::Title || state_ == State::Builder) {
        idle_time_ = in.held ? 0.0f : idle_time_ + dt;
        if (idle_time_ >= tuning::IDLE_AFTER_SECONDS) {
            start_idle();
            return;
        }
    }

    switch (state_) {
        case State::Title:
            update_title(in, dt);
            break;
        case State::Builder:
            update_builder(in);
            break;
        case State::Party:
            if (in.pressed & BTN_Y) {
                set_state(State::Paused);
                audio::play("pause", 660, 40);
                break;
            }
            update_party(in, dt);
            break;
        case State::Paused:
            if (in.pressed & (BTN_Y | BTN_A)) {
                state_ = State::Party;  // keep party timers as they were
                audio::play("pause", 660, 40);
            }
            break;
        case State::Results:
            if (state_time_ > 1.0f && (in.pressed & BTN_A)) {
                if (party_index_ + 1 < tuning::PARTIES_PER_SEASON) {
                    ++party_index_;
                    start_party();
                } else {
                    set_state(State::SeasonOver);
                    audio::play("season_over", 1320, 300);
                }
            }
            break;
        case State::Scratch:
            update_scratch(in, dt);
            break;
        case State::SeasonOver:
        case State::CutOff:
            if (state_time_ > 1.0f && (in.pressed & BTN_A)) finish_run();
            break;
        case State::Initials:
            update_initials(in);
            break;
        case State::HighScores:
            if (state_time_ > 0.5f && (in.pressed & (BTN_A | BTN_PRESS))) {
                set_state(State::Title);
                audio::play("cursor", 1200, 15);
            }
            break;
        case State::Idle:
            // Any button goes back where you were (the press does nothing else).
            if (in.pressed) {
                set_state(idle_return_);
                audio::play_music("title");
                break;
            }
            snow_.update(dt, rng_);
            if (state_time_ >= tuning::IDLE_SECONDS) set_state(State::Sleep);
            break;
        case State::Sleep:
            if (in.pressed) set_state(State::Title);
            break;
    }
}

void Game::start_idle() {
    idle_return_ = state_ == State::Builder ? State::Builder : State::Title;
    snow_.generate(rng_);
    set_state(State::Idle);
}

void Game::update_title(const Input& in, float dt) {
    // Secret reset for the board: hold X + Y.
    bool combo = tuning::SCORE_RESET_HOLD_SECONDS > 0.0f && (in.held & BTN_X) && (in.held & BTN_Y);
    reset_hold_ = combo ? reset_hold_ + dt : 0.0f;
    if (combo && reset_hold_ >= tuning::SCORE_RESET_HOLD_SECONDS) {
        reset_hold_ = 0.0f;
        reset_high_scores();
        new_rank_ = -1;
        scores_were_reset_ = true;
        say("High scores reset.");
        set_state(State::HighScores);
        audio::play("pause", 330, 200);
        return;
    }

    if (in.pressed & (BTN_A | BTN_PRESS)) {
        builder_row_ = ROW_DONE;  // returning players can just press A again
        set_state(State::Builder);
        audio::play("cursor", 1200, 15);
    }
}

// --- High scores ----------------------------------------------------------------

int Game::score_rank(int score) const {
    if (score <= 0) return -1;  // no $0 runs on the board
    for (int i = 0; i < score_count_; ++i) {
        if (score > scores_[i].score) return i;  // ties go below the older score
    }
    return score_count_ < tuning::HIGH_SCORE_COUNT ? score_count_ : -1;
}

void Game::finish_run() {
    new_rank_ = -1;
    scores_were_reset_ = false;
    if (score_rank(run_score_) >= 0) {
        initials_slot_ = 0;
        say("New high score! $%d", run_score_);
        set_state(State::Initials);
        audio::play("high_score", 1320, 250);
    } else {
        set_state(State::HighScores);
    }
}

void Game::update_initials(const Input& in) {
    char& letter = initials_[initials_slot_];
    if (in.pressed & BTN_UP) {
        letter = letter >= 'Z' ? 'A' : static_cast<char>(letter + 1);
        audio::play("cursor", 1200, 15);
    }
    if (in.pressed & BTN_DOWN) {
        letter = letter <= 'A' ? 'Z' : static_cast<char>(letter - 1);
        audio::play("cursor", 1200, 15);
    }
    if ((in.pressed & (BTN_LEFT | BTN_B)) && initials_slot_ > 0) {
        --initials_slot_;
        audio::play("cursor", 1000, 15);
    }
    if ((in.pressed & BTN_RIGHT) && initials_slot_ < 2) {
        ++initials_slot_;
        audio::play("cursor", 1200, 15);
    }
    if (!(in.pressed & (BTN_A | BTN_PRESS))) return;

    if (initials_slot_ < 2) {  // A moves on to the next letter
        ++initials_slot_;
        audio::play("cursor", 1200, 15);
        return;
    }
    // Make room and write the new entry in.
    int rank = score_rank(run_score_);
    int last = score_count_ < tuning::HIGH_SCORE_COUNT ? score_count_ : tuning::HIGH_SCORE_COUNT - 1;
    for (int i = last; i > rank; --i) scores_[i] = scores_[i - 1];
    std::memcpy(scores_[rank].initials, initials_, sizeof(initials_));
    scores_[rank].score = run_score_;
    if (score_count_ < tuning::HIGH_SCORE_COUNT) ++score_count_;
    new_rank_ = rank;
    save();
    say("%s is number %d.", initials_, rank + 1);
    set_state(State::HighScores);
    audio::play("builder_surprise", 1500, 60);
}

void Game::use_default_scores() {
    score_count_ = 0;
    for (const content::HighScore& h : content::DEFAULT_HIGH_SCORES) {
        if (h.initials[0] == '\0' || score_count_ == tuning::HIGH_SCORE_COUNT) continue;
        // Insert in order, best first.
        int i = score_count_++;
        while (i > 0 && scores_[i - 1].score < h.score) {
            scores_[i] = scores_[i - 1];
            --i;
        }
        scores_[i] = h;
    }
}

void Game::reset_high_scores() {
    use_default_scores();
    save();
}

void Game::start_season() {
    party_index_ = 0;
    run_score_ = 0;
    start_party();
}

void Game::start_party() {
    seat_count_ = tuning::FIRST_PARTY_SEATS + party_index_;
    if (seat_count_ > GUEST_COUNT + 1) seat_count_ = GUEST_COUNT + 1;
    if (seat_count_ > Party::MAX_SEATS) seat_count_ = Party::MAX_SEATS;

    // Seats sit in an oval around the tree, with you at the bottom.
    for (int i = 0; i < seat_count_; ++i) {
        float angle = 1.5707963f + i * (6.2831853f / seat_count_);
        seats_[i] = {ROOM_CX + static_cast<int>(std::lround(std::cos(angle) * 84.0f)),
                     ROOM_CY + static_cast<int>(std::lround(std::sin(angle) * SEAT_RADIUS_Y))};
    }

    // Invite a random set of guests.
    int guests[GUEST_COUNT];
    for (int i = 0; i < GUEST_COUNT; ++i) guests[i] = i;
    for (int i = GUEST_COUNT - 1; i > 0; --i) {
        int j = rng_.below(i + 1);
        int tmp = guests[i];
        guests[i] = guests[j];
        guests[j] = tmp;
    }
    for (int s = 1; s < seat_count_; ++s) {
        guest_of_seat_[s] = guests[s - 1];
        favorite_[s] = static_cast<GiftCategory>(rng_.below(4));
    }
    // Guests can have a hand-drawn full sprite; you always use your built look.
    spr_person_[PLAYER] = nullptr;
    for (int s = 1; s < seat_count_; ++s) spr_person_[s] = named_sprite("guests", label_of(s));

    // Everyone brings a gift: draw them from the list without repeats.
    int pool[GIFT_COUNT];
    for (int i = 0; i < GIFT_COUNT; ++i) pool[i] = i;
    int ids[Party::MAX_SEATS];
    int values[Party::MAX_SEATS];
    for (int g = 0; g < seat_count_; ++g) {
        int j = g + rng_.below(GIFT_COUNT - g);
        int tmp = pool[g];
        pool[g] = pool[j];
        pool[j] = tmp;
        ids[g] = pool[g];
        const content::Gift& info = content::GIFTS[ids[g]];
        values[g] = info.min_value == info.max_value
                        ? info.min_value
                        : static_cast<int>(std::lround(rng_.range(static_cast<float>(info.min_value),
                                                                  static_cast<float>(info.max_value))));
    }
    party_.start(seat_count_, ids, values, rng_);

    buzz_.reset();
    cursor_ = TREE;
    drink_cooldown_ = 0.0f;
    tray_open_ = false;
    warn_tick_ = -1;
    vibe_ = tuning::VIBE_START;
    needle_buzz_ = 0.0f;
    needle_vibe_ = vibe_;
    hide_held_ = false;
    suspicion_ = 0.0f;
    banter_active_ = false;
    banter_timer_ = rng_.range(tuning::BANTER_EVERY_MIN, tuning::BANTER_EVERY_MAX);
    party_score_ = 0.0f;
    charm_score_ = 0.0f;
    steals_ = 0;
    scenes_ = 0;

    set_state(State::Party);
    say("Party %d of %d. Numbers drawn!", party_index_ + 1, tuning::PARTIES_PER_SEASON);
    audio::play("party_start", 880, 80);
    begin_turn(false);
}

// Hand control to whoever acts next.
void Game::begin_turn(bool announce) {
    phase_time_ = 0.0f;
    if (party_.over()) {
        finish_party();
        return;
    }
    int actor = party_.actor();
    if (actor == PLAYER) {
        phase_ = Phase::PlayerTurn;
        banter_active_ = false;  // banter never interrupts your own turn
        if (party_.final_swap()) {
            say("FINAL SWAP! Steal one, or pick the tree.");
        } else if (party_.forbidden() >= 0) {
            say("Robbed! Open or steal (not it back).");
        } else {
            say("YOUR TURN! Open a gift or steal one.");
        }
        audio::play("your_turn", 1320, 90);
        return;
    }
    phase_ = Phase::Thinking;
    think_seconds_ = tuning::AI_THINK_SECONDS + rng_.range(0.0f, tuning::AI_THINK_JITTER);
    if (party_.final_swap()) {
        say("%s gets the final swap...", label_of(actor));
    } else if (announce) {
        say("%s is up.", label_of(actor));
    }
}

void Game::finish_party() {
    // Leaving with the scratchers? Scratch one to find out what they're worth.
    int g = party_.gift_of(PLAYER);
    if (g >= 0 && gift_info(g).scratch_ticket) {
        start_scratch();
        return;
    }
    show_results(g >= 0 ? party_.gift(g).value : 0);
}

void Game::show_results(int gift_value) {
    Results r;
    r.gift = party_.gift_of(PLAYER);
    r.gift_value = gift_value;
    if (r.gift >= 0) {
        if (party_.frozen(r.gift) && gift_info(r.gift).category == GiftCategory::Coveted) {
            r.frozen_bonus = tuning::FROZEN_COVETED_BONUS;
        }
    }
    r.sweet = static_cast<int>(party_score_);
    r.charm = static_cast<int>(charm_score_);
    r.steals = steals_ * tuning::STEAL_BONUS;
    r.scenes = scenes_ * tuning::SCENE_PENALTY;
    r.total = r.gift_value + r.sweet + r.charm + r.steals + r.frozen_bonus - r.scenes;
    results_ = r;
    run_score_ += r.total;
    set_state(State::Results);
    audio::play("party_over", 990, 200);
}

// --- Lottery scratcher ----------------------------------------------------------

void Game::start_scratch() {
    ticket_.generate(rng_);
    scratch_cursor_ = 0;
    scratch_won_ = false;
    scratch_noise_ = 0.0f;
    set_state(State::Scratch);
    audio::play("scratch_start", 1320, 120);
}

void Game::update_scratch(const Input& in, float dt) {
    if (scratch_won_) {
        if (state_time_ - scratch_won_at_ > 0.8f && (in.pressed & (BTN_A | BTN_PRESS))) {
            show_results(ticket_.prize);
        }
        return;
    }

    // Move between spots: 3 across, 2 down.
    int col = scratch_cursor_ % ScratchTicket::COLUMNS;
    int row = scratch_cursor_ / ScratchTicket::COLUMNS;
    const int rows = ScratchTicket::SPOTS / ScratchTicket::COLUMNS;
    if ((in.pressed & BTN_LEFT) && col > 0) --col;
    if ((in.pressed & BTN_RIGHT) && col < ScratchTicket::COLUMNS - 1) ++col;
    if ((in.pressed & BTN_UP) && row > 0) --row;
    if ((in.pressed & BTN_DOWN) && row < rows - 1) ++row;
    scratch_cursor_ = row * ScratchTicket::COLUMNS + col;

    if (!(in.held & (BTN_A | BTN_PRESS)) || ticket_.revealed(scratch_cursor_)) return;

    // Scratching. Drunk, your hand slips onto another spot.
    if (rng_.chance(tuning::SCRATCH_DRUNK_SLIP * buzz_.drunkenness() * dt)) {
        scratch_cursor_ = rng_.below(ScratchTicket::SPOTS);
        return;
    }
    float& progress = ticket_.scratched[scratch_cursor_];
    progress += dt / tuning::SCRATCH_SECONDS;
    scratch_noise_ -= dt;
    if (scratch_noise_ <= 0.0f) {
        scratch_noise_ = 0.12f;
        audio::play("scratch", 2400, 10);
    }
    if (progress < 1.0f) return;

    progress = 1.0f;
    if (!ticket_.won()) {
        audio::play("scratch_reveal", 990, 40);
        return;
    }

    // The pair is out: that's the prize. Uncover the rest.
    for (float& s : ticket_.scratched) s = 1.0f;
    scratch_won_ = true;
    scratch_won_at_ = state_time_;
    audio::play("scratch_win", 1568, 400);
}

// --- Character builder ----------------------------------------------------------

void Game::update_builder(const Input& in) {
    if (in.pressed & BTN_UP) {
        builder_row_ = (builder_row_ + ROW_COUNT - 1) % ROW_COUNT;
        audio::play("cursor", 1200, 15);
    }
    if (in.pressed & BTN_DOWN) {
        builder_row_ = (builder_row_ + 1) % ROW_COUNT;
        audio::play("cursor", 1200, 15);
    }

    // Left/right cycles whatever the row controls.
    int step = (in.pressed & BTN_RIGHT) ? 1 : (in.pressed & BTN_LEFT) ? -1 : 0;
    if (step != 0) {
        auto cycle = [step](int& value, int count) { value = (value + step + count) % count; };
        switch (builder_row_) {
            case ROW_SKIN:       cycle(look_.skin, SKIN_COUNT); break;
            case ROW_HAIR:       cycle(look_.hair, HAIR_COUNT); break;
            case ROW_HAIR_COLOR: cycle(look_.hair_color, HAIR_COLOR_COUNT); break;
            case ROW_SWEATER:    cycle(look_.sweater, SWEATER_COUNT); break;
            case ROW_KNIT:       cycle(look_.sweater_color, KNIT_COUNT); break;
            case ROW_HAT:        cycle(look_.hat, HAT_COUNT); break;
            default:             break;
        }
        audio::play("builder_change", 900, 20);
    }

    if (in.pressed & (BTN_A | BTN_PRESS)) {
        if (builder_row_ == ROW_SURPRISE) {
            look_ = {rng_.below(SKIN_COUNT), rng_.below(HAIR_COUNT), rng_.below(HAIR_COLOR_COUNT),
                     rng_.below(SWEATER_COUNT), rng_.below(KNIT_COUNT), rng_.below(HAT_COUNT)};
            audio::play("builder_surprise", 1500, 60);
        } else if (builder_row_ == ROW_DONE) {
            save();
            start_season();
        } else {
            ++builder_row_;  // A on an option moves on to the next one
            audio::play("cursor", 1200, 15);
        }
    }
}

namespace {

// What survives power-off. Bump the magic number if the layout changes.
// (Changing HIGH_SCORE_COUNT changes the size, so old saves are ignored.)
struct SaveData {
    uint32_t magic;
    content::Look look;
    char initials[4];  // last initials entered
    int32_t score_count;
    content::HighScore scores[tuning::HIGH_SCORE_COUNT];
    int32_t volume;
};
constexpr uint32_t SAVE_MAGIC = 0x534E4C33;  // "SNL3"

// The second format: everything above but the volume.
struct SaveDataV2 {
    uint32_t magic;
    content::Look look;
    char initials[4];
    int32_t score_count;
    content::HighScore scores[tuning::HIGH_SCORE_COUNT];
};
constexpr uint32_t SAVE_MAGIC_V2 = 0x534E4C32;  // "SNL2"

// The first save format, from before high scores: just your look.
struct SaveDataV1 {
    uint32_t magic;
    content::Look look;
};
constexpr uint32_t SAVE_MAGIC_V1 = 0x534E4C31;  // "SNL1"

bool three_letters(const char* s) {
    for (int i = 0; i < 3; ++i) {
        if (s[i] < 'A' || s[i] > 'Z') return false;
    }
    return s[3] == '\0';
}

}  // namespace

void Game::save() const {
    SaveData save{};
    save.magic = SAVE_MAGIC;
    save.look = look_;
    std::memcpy(save.initials, initials_, sizeof(save.initials));
    save.score_count = score_count_;
    for (int i = 0; i < score_count_; ++i) save.scores[i] = scores_[i];
    save.volume = volume_;
    platform::save_data(&save, sizeof(save));
}

void Game::load() {
    use_default_scores();  // unless the save has a board

    SaveData save{};
    uint32_t got = platform::load_data(&save, sizeof(save));
    bool current = got == sizeof(SaveData) && save.magic == SAVE_MAGIC;
    bool v2 = got == sizeof(SaveDataV2) && save.magic == SAVE_MAGIC_V2;  // same layout, minus the volume
    bool v1 = got == sizeof(SaveDataV1) && save.magic == SAVE_MAGIC_V1;
    if (!current && !v2 && !v1) return;

    if (current && save.volume >= 0 && save.volume <= tuning::VOLUME_STEPS) volume_ = save.volume;
    if (current || v2) {
        if (three_letters(save.initials)) std::memcpy(initials_, save.initials, sizeof(initials_));
        bool ok = save.score_count >= 0 && save.score_count <= tuning::HIGH_SCORE_COUNT;
        for (int i = 0; ok && i < save.score_count; ++i) ok = save.scores[i].initials[3] == '\0';
        if (ok) {
            score_count_ = save.score_count;
            for (int i = 0; i < score_count_; ++i) scores_[i] = save.scores[i];
        }
    }

    const content::Look& l = save.look;
    auto ok = [](int v, int count) { return v >= 0 && v < count; };
    if (ok(l.skin, SKIN_COUNT) && ok(l.hair, HAIR_COUNT) && ok(l.hair_color, HAIR_COLOR_COUNT) &&
        ok(l.sweater, SWEATER_COUNT) && ok(l.sweater_color, KNIT_COUNT) && ok(l.hat, HAT_COUNT)) {
        look_ = l;
    }
}

const content::Look& Game::look_of(int seat) const {
    return seat == PLAYER ? look_ : content::GUESTS[guest_of_seat_[seat]].look;
}

// --- Party update ---------------------------------------------------------------

void Game::update_party(const Input& in, float dt) {
    ticker_age_ += dt;
    phase_time_ += dt;
    if (drink_cooldown_ > 0.0f) drink_cooldown_ -= dt;

    buzz_.update(dt);
    if (buzz_.cut_off()) {
        cut_off_line_ = content::CUT_OFF_LINES[rng_.below(
            sizeof(content::CUT_OFF_LINES) / sizeof(content::CUT_OFF_LINES[0]))];
        say("%s", cut_off_line_);  // not shown on screen, but lands in test logs
        set_state(State::CutOff);
        audio::play("cut_off", 110, 700);
        return;
    }

    // Warning beeps while over the line.
    if (buzz_.over_limit_seconds() > 0.0f) {
        int tick = static_cast<int>(buzz_.over_limit_seconds() * 2.0f);
        if (tick != warn_tick_) {
            warn_tick_ = tick;
            audio::play("warning", 220, 80);
            if (tick == 0) say("You should probably sit down...");
        }
    } else {
        warn_tick_ = -1;
    }

    if (buzz_.zone() == Zone::SweetSpot) {
        party_score_ += tuning::SWEET_SPOT_POINTS_PER_SECOND * dt;
    }

    update_vibe(dt);

    // Needles swing toward the real readings instead of snapping.
    float ease = tuning::NEEDLE_SPEED * dt;
    if (ease > 1.0f) ease = 1.0f;
    needle_buzz_ += (buzz_.level() - needle_buzz_) * ease;
    needle_vibe_ += (vibe_ - needle_vibe_) * ease;

    bool was_bantering = banter_active_;  // so an answer doesn't also move the cursor
    update_banter(in, dt);

    // You can look around and drink no matter whose turn it is -- except
    // mid-grab or mid-banter, when the joystick belongs to that.
    if (phase_ != Phase::Grab && !was_bantering && !banter_active_) {
        if (in.pressed & BTN_UP) move_cursor(0, -1);
        if (in.pressed & BTN_DOWN) move_cursor(0, 1);
        if (in.pressed & BTN_LEFT) move_cursor(-1, 0);
        if (in.pressed & BTN_RIGHT) move_cursor(1, 0);

        // Drunk: the cursor wanders off on its own.
        float drunk = buzz_.drunkenness();
        if (drunk > 0.0f && rng_.chance(tuning::CURSOR_DRIFT_PER_SECOND * drunk * dt)) {
            static constexpr int DIRS[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
            int d = rng_.below(4);
            move_cursor(DIRS[d][0], DIRS[d][1]);
        }
    }

    update_hiding(in, dt);
    if (tray_open_) update_tray(in, dt);
    else if (in.pressed & BTN_B) open_tray();

    bool act = (in.pressed & (BTN_A | BTN_PRESS)) != 0;
    switch (phase_) {
        case Phase::Thinking:
            if (banter_active_) think_seconds_ += dt;  // they wait for you to finish talking
            if (act) inspect();
            if (phase_time_ >= think_seconds_) ai_act();
            break;
        case Phase::PlayerTurn:
            if (act) {
                player_choose();
            } else if (phase_time_ >= player_turn_seconds()) {
                if (party_.final_swap()) {
                    party_.keep();
                    say("Time's up. You keep your gift.");
                    begin_turn(false);
                } else {
                    say("You froze up. Grabbing one off the pile.");
                    player_open();
                }
            }
            break;
        case Phase::Grab:
            update_grab(in, dt);
            break;
        case Phase::Reveal:
            if (phase_time_ >= tuning::REVEAL_SECONDS || (act && phase_time_ > 0.3f)) begin_turn(true);
            break;
    }
}

// --- Hiding your gift -----------------------------------------------------------

void Game::update_hiding(const Input& in, float dt) {
    bool was_caught = caught_hiding();
    bool has_gift = party_.gift_of(PLAYER) >= 0;
    if ((in.pressed & BTN_X) && !has_gift) say("%s", content::HIDE_NO_GIFT);

    // Nobody steals from you on your own turn, so there's nothing to hide from then.
    bool can_hide = has_gift && !tray_open_ && phase_ != Phase::PlayerTurn && phase_ != Phase::Grab;
    bool now = (in.held & BTN_X) && can_hide;
    if (now && !hide_held_) {
        say("%s", content::HIDE_START);
        audio::play("hide", 600, 30);
    }
    hide_held_ = now;

    if (hide_held_) {
        suspicion_ += dt;
        if (suspicion_ > tuning::HIDE_MAX_SUSPICION) suspicion_ = tuning::HIDE_MAX_SUSPICION;
    } else {
        suspicion_ -= tuning::HIDE_COOL_PER_SECOND * dt;
        if (suspicion_ < 0.0f) suspicion_ = 0.0f;
    }

    if (caught_hiding()) {
        if (!was_caught) {
            flop_vibe(1.0f, tuning::HIDE_RUDE_FLOP);
            say("%s", content::HIDE_CAUGHT);
            audio::play("caught_hiding", 250, 200);
        }
        if (hide_held_) flop_vibe(1.0f, tuning::HIDE_RUDE_PER_SECOND * dt);
    } else if (was_caught) {
        say("%s", content::HIDE_COOLED);
    }
}

bool Game::hiding() const {
    return hide_held_;
}

// --- Vibe and banter ------------------------------------------------------------

void Game::update_vibe(float dt) {
    switch (buzz_.zone()) {
        case Zone::Sober:     push_vibe(-tuning::VIBE_SOBER_DRIFT * dt); break;
        case Zone::SweetSpot: pull_vibe(tuning::VIBE_SWEET_PULL * dt); break;
        case Zone::Drunk:
        case Zone::Blackout:
            push_vibe(tuning::VIBE_DRUNK_DRIFT * (0.5f + buzz_.drunkenness()) * dt);
            break;
    }
    if (vibe() == Vibe::Charming) charm_score_ += tuning::CHARM_POINTS_PER_SECOND * dt;
}

void Game::push_vibe(float amount) {
    vibe_ += amount;
    if (vibe_ < 0.0f) vibe_ = 0.0f;
    if (vibe_ > 100.0f) vibe_ = 100.0f;
}

void Game::pull_vibe(float amount) {
    float gap = tuning::VIBE_CENTER - vibe_;
    if (gap > amount) gap = amount;
    if (gap < -amount) gap = -amount;
    vibe_ += gap;
}

// A bad outcome never helps: you keep however far off-center you were, land
// on the side that matches what you did (rude or shy), and get pushed further
// out. So a flopped roast while shy swings you to the rude end.
void Game::flop_vibe(float side, float amount) {
    float distance = std::fabs(vibe_ - tuning::VIBE_CENTER);
    vibe_ = tuning::VIBE_CENTER + side * (distance + amount);
    push_vibe(0.0f);  // clamp to 0..100
}

void Game::update_banter(const Input& in, float dt) {
    if (!banter_active_) {
        // Guests only start conversations while someone else is up.
        if (phase_ != Phase::Thinking && phase_ != Phase::Reveal) return;
        banter_timer_ -= dt;
        if (banter_timer_ <= 0.0f) start_banter();
        return;
    }

    banter_time_ += dt;
    if (in.pressed & BTN_UP) resolve_banter(Banter::Laugh);
    else if (in.pressed & BTN_LEFT) resolve_banter(Banter::Flatter);
    else if (in.pressed & BTN_RIGHT) resolve_banter(Banter::Roast);
    else if (in.pressed & BTN_DOWN) resolve_banter(Banter::Ignore);
    else if (banter_time_ >= tuning::BANTER_SECONDS) resolve_banter(Banter::Timeout);
}

void Game::start_banter() {
    banter_active_ = true;
    banter_time_ = 0.0f;
    banter_seat_ = 1 + rng_.below(seat_count_ - 1);
    banter_line_ = content::GUESTS[guest_of_seat_[banter_seat_]].chatter[rng_.below(3)];
    audio::play("banter_start", 1760, 30);
}

void Game::resolve_banter(Banter response) {
    banter_active_ = false;
    banter_timer_ = rng_.range(tuning::BANTER_EVERY_MIN, tuning::BANTER_EVERY_MAX);
    const content::Guest& guest = content::GUESTS[guest_of_seat_[banter_seat_]];

    if (response == Banter::Ignore) {
        flop_vibe(-1.0f, tuning::BANTER_IGNORE);
        say(content::BANTER_IGNORE, guest.label);
        return;
    }
    if (response == Banter::Timeout) {
        flop_vibe(-1.0f, tuning::BANTER_TIMEOUT);
        say(content::BANTER_TIMEOUT, guest.label);
        return;
    }

    // Drunk: whatever you meant to say comes out as a roast.
    bool garbled = false;
    if (response != Banter::Roast && rng_.chance(tuning::BANTER_GARBLE * buzz_.drunkenness())) {
        response = Banter::Roast;
        garbled = true;
    }

    float bonus = 0.0f;
    switch (buzz_.zone()) {
        case Zone::Sober:     bonus = -tuning::BANTER_SOBER_PENALTY; break;
        case Zone::SweetSpot: bonus = tuning::BANTER_SWEET_BONUS; break;
        case Zone::Drunk:
        case Zone::Blackout:  bonus = -tuning::BANTER_DRUNK_PENALTY; break;
    }
    const tuning::BanterOdds& odds = tuning::BANTER_ODDS[static_cast<int>(guest.personality)];
    float chance = response == Banter::Laugh     ? tuning::BANTER_LAUGH_ODDS
                   : response == Banter::Flatter ? odds.flatter
                                                 : odds.roast;
    bool landed = rng_.chance(chance + bonus);

    switch (response) {
        case Banter::Laugh:
            if (landed) pull_vibe(tuning::BANTER_PULL_LAUGH);
            else flop_vibe(-1.0f, tuning::BANTER_LAUGH_FLOP);
            say(landed ? content::BANTER_LAUGH_OK : content::BANTER_LAUGH_FLOP, guest.label);
            break;
        case Banter::Flatter:
            if (landed) pull_vibe(tuning::BANTER_PULL_FLATTER);
            else flop_vibe(-1.0f, tuning::BANTER_FLATTER_FLOP);
            say(landed ? content::BANTER_FLATTER_OK : content::BANTER_FLATTER_FLOP, guest.label);
            break;
        default:  // Roast
            if (landed) pull_vibe(tuning::BANTER_PULL_ROAST);
            else flop_vibe(1.0f, tuning::BANTER_ROAST_FLOP);
            if (garbled) {
                say(landed ? content::BANTER_GARBLE_OK : content::BANTER_GARBLE_FLOP, guest.label);
            } else {
                say(landed ? content::BANTER_ROAST_OK : content::BANTER_ROAST_FLOP, guest.label);
            }
            break;
    }
    if (landed) audio::play("banter_ok", 1320, 60);
    else audio::play("banter_flop", 200, 160);
}

void Game::ai_act() {
    int actor = party_.actor();
    AiProfile profile{content::GUESTS[guest_of_seat_[actor]].personality, favorite_[actor], party_index_,
                      player_target_bonus(), PLAYER};
    int g = ai_choose(party_, profile, rng_);

    if (party_.final_swap()) {
        if (g < 0) {
            party_.keep();
            say("%s keeps their gift.", label_of(actor));
        } else {
            int victim = party_.gift(g).holder;
            party_.steal(g);
            if (victim == PLAYER) {
                say("%s swaps you for your %s!", label_of(actor), gift_info(g).name);
            } else {
                say("%s swaps with %s: %s!", label_of(actor), label_of(victim), gift_info(g).name);
            }
            if (victim == PLAYER) audio::play("stolen_from_you", 300, 150);
            else audio::play("steal", 700, 150);
        }
        begin_turn(false);
        return;
    }

    if (g < 0) {
        int w = party_.pick_wrapped(rng_);
        party_.open(w);
        start_reveal(w, actor);
        return;
    }

    int victim = party_.gift(g).holder;
    party_.steal(g);
    say("%s steals the %s from %s!", label_of(actor), gift_info(g).name, who(victim));
    if (victim == PLAYER) audio::play("stolen_from_you", 300, 150);
    else audio::play("steal", 700, 150);
    begin_turn(false);
}

void Game::player_choose() {
    if (cursor_ == TREE) {
        if (party_.final_swap()) {
            party_.keep();
            say("You keep your gift.");
            begin_turn(false);
        } else {
            player_open();
        }
        return;
    }

    int g = party_.gift_of(cursor_);
    if (g < 0) {
        say("%s has nothing to steal.", label_of(cursor_));
    } else if (party_.frozen(g)) {
        say("The %s is frozen. Hands off.", gift_info(g).name);
    } else if (g == party_.forbidden()) {
        say("No stealing it right back!");
    } else {
        start_grab(g);
    }
}

void Game::player_open() {
    int w = party_.pick_wrapped(rng_);
    party_.open(w);
    start_reveal(w, PLAYER);
}

void Game::start_grab(int gift) {
    phase_ = Phase::Grab;
    banter_active_ = false;
    phase_time_ = 0.0f;
    grab_gift_ = gift;
    grab_pos_ = 0.0f;
    grab_dir_ = 1.0f;
    grab_center_ = rng_.range(0.35f, 0.8f);
    say("Grab it! Press A in the zone.");
    audio::play("grab_start", 990, 40);
}

void Game::update_grab(const Input& in, float dt) {
    float speed = tuning::GRAB_SWEEPS_PER_SECOND;
    if (buzz_.zone() == Zone::Sober) speed *= tuning::GRAB_SOBER_SPEEDUP;
    speed *= 1.0f + tuning::GRAB_DRUNK_WOBBLE * buzz_.drunkenness() * std::sin(phase_time_ * 7.0f);

    grab_pos_ += grab_dir_ * speed * dt;  // one sweep = one full pass across the bar
    if (grab_pos_ > 1.0f) {
        grab_pos_ = 1.0f;
        grab_dir_ = -1.0f;
    } else if (grab_pos_ < 0.0f) {
        grab_pos_ = 0.0f;
        grab_dir_ = 1.0f;
    }

    bool pressed = (in.pressed & (BTN_A | BTN_PRESS)) != 0;
    if (!pressed && phase_time_ < tuning::GRAB_SECONDS) return;

    bool hit = pressed && std::fabs(grab_pos_ - grab_zone_center()) <= grab_zone_width() * 0.5f;
    int g = grab_gift_;
    if (hit) {
        int victim = party_.gift(g).holder;
        bool final_swap = party_.final_swap();
        party_.steal(g);
        ++steals_;
        float rudeness = tuning::VIBE_STEAL_PUSH;
        if (content::GUESTS[guest_of_seat_[victim]].personality == Personality::Opener) {
            rudeness += tuning::VIBE_STEAL_NEW_HIRE;
        }
        flop_vibe(1.0f, rudeness);
        if (final_swap) {
            say("Swapped! The %s is yours.", gift_info(g).name);
        } else {
            say("You swipe the %s from %s!", gift_info(g).name, who(victim));
        }
        audio::play("grab_hit", 1500, 120);
        lights_flash_ = STEAL_FLASH_SECONDS;
        begin_turn(false);
        return;
    }

    ++scenes_;
    flop_vibe(1.0f, tuning::VIBE_SCENE_PUSH);
    audio::play("grab_miss", 150, 300);
    if (party_.final_swap()) {
        party_.keep();
        say("You fumble it. Everyone saw. Keep yours.");
        begin_turn(false);
    } else {
        say("You fumble it and make a scene!");
        player_open();
    }
}

void Game::start_reveal(int gift, int seat) {
    phase_ = Phase::Reveal;
    phase_time_ = 0.0f;
    reveal_gift_ = gift;
    reveal_seat_ = seat;
    // A cheer or a groan, depending on what came out of the box.
    uint16_t hz = seat == PLAYER ? 1320 : 990;
    if (party_.gift(gift).value < 0 && !value_hidden(party_, gift)) {
        audio::play("reveal_bad", hz, 60);
    } else {
        audio::play("reveal_good", hz, 60);
    }
}

// --- Drink tray -----------------------------------------------------------------

void Game::open_tray() {
    if (hide_held_) {
        say("%s", content::HIDE_HANDS_FULL);
        return;
    }
    if (drink_cooldown_ > 0.0f) {
        say("The tray hasn't come back yet.");
        return;
    }

    // Fill the slots. The drunker you are, the more the odds lean from each
    // drink's sober weight to its drunk weight.
    float t = buzz_.level() / tuning::BLACKOUT;
    if (t > 1.0f) t = 1.0f;
    float weights[DRINK_COUNT];
    float total = 0.0f;
    for (int d = 0; d < DRINK_COUNT; ++d) {
        const tuning::Drink& drink = tuning::DRINKS[d];
        weights[d] = drink.weight_sober + (drink.weight_drunk - drink.weight_sober) * t;
        total += weights[d];
    }
    for (int& slot : tray_) {
        float pick = rng_.range(0.0f, total);
        slot = DRINK_COUNT - 1;
        for (int d = 0; d < DRINK_COUNT; ++d) {
            if (pick < weights[d]) {
                slot = d;
                break;
            }
            pick -= weights[d];
        }
    }

    // There's always water on the tray (the first sobering drink in the list).
    int water = -1;
    for (int d = 0; d < DRINK_COUNT && water < 0; ++d) {
        if (tuning::DRINKS[d].max_buzz < 0.0f) water = d;
    }
    if (water >= 0) {
        bool has_water = false;
        for (int slot : tray_) has_water = has_water || slot == water;
        if (!has_water) tray_[rng_.below(tuning::TRAY_SLOTS)] = water;
    }

    // Clear-headed, you see the tray in order, weakest to strongest. Drunk, it's
    // whatever order it landed in. Sorting goes by look, so drinks that look
    // the same (good vs bad wine) land in random order and give nothing away.
    if (buzz_.zone() == Zone::Sober || buzz_.zone() == Zone::SweetSpot) {
        auto strength = [](int d) {
            float sum = 0.0f;
            int n = 0;
            for (const tuning::Drink& other : tuning::DRINKS) {
                if (std::strcmp(other.look, tuning::DRINKS[d].look) == 0) {
                    sum += other.min_buzz + other.max_buzz;
                    ++n;
                }
            }
            return sum / n;
        };
        for (int i = 1; i < tuning::TRAY_SLOTS; ++i) {
            for (int j = i; j > 0 && strength(tray_[j]) < strength(tray_[j - 1]); --j) {
                int tmp = tray_[j];
                tray_[j] = tray_[j - 1];
                tray_[j - 1] = tmp;
            }
        }
    }

    tray_open_ = true;
    tray_time_ = 0.0f;
    hand_pos_ = 0.0f;
    hand_dir_ = 1.0f;
    say("Drinks! Press D to grab.");
    audio::play("tray", 700, 40);
}

void Game::update_tray(const Input& in, float dt) {
    tray_time_ += dt;

    // Drunk: the hand moves faster and lurches.
    float drunk = buzz_.drunkenness();
    float speed = tuning::TRAY_SWEEPS_PER_SECOND * (1.0f + tuning::TRAY_DRUNK_SPEEDUP * drunk);
    speed *= 1.0f + tuning::TRAY_DRUNK_LURCH * drunk * std::sin(tray_time_ * 9.0f);
    hand_pos_ += hand_dir_ * speed * dt;
    if (hand_pos_ > 1.0f) {
        hand_pos_ = 1.0f;
        hand_dir_ = -1.0f;
    } else if (hand_pos_ < 0.0f) {
        hand_pos_ = 0.0f;
        hand_dir_ = 1.0f;
    }

    if (in.pressed & BTN_B) {
        take_drink(tray_[tray_slot_under_hand()], false);
    } else if (tray_time_ >= tuning::TRAY_SECONDS) {
        take_drink(tray_[tray_slot_under_hand()], true);  // hesitated: you get what you get
    }
}

int Game::tray_slot_under_hand() const {
    int slot = static_cast<int>(hand_pos_ * tuning::TRAY_SLOTS);
    return slot >= tuning::TRAY_SLOTS ? tuning::TRAY_SLOTS - 1 : slot;
}

void Game::take_drink(int d, bool fumbled) {
    tray_open_ = false;
    drink_cooldown_ = tuning::DRINK_COOLDOWN_SECONDS;
    const tuning::Drink& drink = tuning::DRINKS[d];

    float amount = rng_.range(drink.min_buzz, drink.max_buzz);
    if (amount > 0.0f) amount *= 1.0f + tuning::DRINK_STRENGTH_PER_PARTY * party_index_;  // water stays water
    if (!buzz_.drink(amount, drink.absorb_seconds)) {
        say("Both hands are full.");
        return;
    }
    if (fumbled) say("You hesitate and grab a %s.", drink.name);
    else say("You grab a %s.", drink.name);
    if (amount < 0.0f) audio::play("water", 880, 60);
    else audio::play("drink", 880, 60);
}

void Game::move_cursor(int dx, int dy) {
    // Jump to the nearest target in the pushed direction, preferring ones
    // that are straight ahead over ones off to the side.
    Point from = target_pos(cursor_);
    int best = -1;
    int best_score = 0;
    for (int t = 1; t <= seat_count_; ++t) {  // seat 0 is you; seat_count_ stands in for the tree
        int target = t == seat_count_ ? TREE : t;
        if (target == cursor_) continue;
        Point p = target_pos(target);
        int vx = p.x - from.x;
        int vy = p.y - from.y;
        int along = vx * dx + vy * dy;
        if (along <= 0) continue;
        int score = along + std::abs(vx * dy - vy * dx) * 2;
        if (best < 0 || score < best_score) {
            best = target;
            best_score = score;
        }
    }
    if (best >= 0) {
        cursor_ = best;
        audio::play("cursor", 1200, 15);
        inspect();
    }
}

// Describe whatever the cursor is on.
void Game::inspect() {
    if (cursor_ == TREE) {
        int n = party_.wrapped_count();
        say("%d wrapped gift%s under the tree.", n, n == 1 ? "" : "s");
        return;
    }
    int g = party_.gift_of(cursor_);
    if (g < 0) {
        say("%s: empty-handed.", label_of(cursor_));
        return;
    }
    char value[16];
    value_text(g, value, sizeof(value));
    if (party_.frozen(g)) {
        say("%s: %s %s FROZEN", label_of(cursor_), gift_info(g).name, value);
    } else {
        say("%s: %s %s (%d/%d)", label_of(cursor_), gift_info(g).name, value, party_.gift(g).steals,
            tuning::STEALS_TO_FREEZE);
    }
}

// --- Helpers --------------------------------------------------------------------

void Game::say(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(ticker_, sizeof(ticker_), fmt, args);
    va_end(args);
    ticker_age_ = 0.0f;
}

Game::Point Game::target_pos(int target) const {
    if (target == TREE) return {ROOM_CX, ROOM_CY};
    return seats_[target];
}

const char* Game::label_of(int seat) const {
    return seat == PLAYER ? "YOU" : content::GUESTS[guest_of_seat_[seat]].label;
}

const char* Game::who(int seat) const {
    return seat == PLAYER ? "you" : content::GUESTS[guest_of_seat_[seat]].label;
}

const content::Gift& Game::gift_info(int g) const {
    return content::GIFTS[party_.gift(g).content_id];
}

void Game::value_text(int g, char* buf, int size) const {
    if (value_hidden(party_, g)) {
        std::snprintf(buf, size, "$?");
    } else {
        std::snprintf(buf, size, "$%d", party_.gift(g).value);
    }
}

float Game::player_target_bonus() const {
    float bonus = scenes_ * tuning::SCENE_TARGET_BONUS;
    if (buzz_.zone() == Zone::Drunk || buzz_.zone() == Zone::Blackout) bonus += tuning::DRUNK_TARGET_BONUS;
    if (caught_hiding()) bonus += tuning::HIDE_OBVIOUS_BONUS;
    else if (hide_held_) bonus += tuning::HIDE_SNEAKY_BONUS;
    switch (vibe()) {
        case Vibe::Charming:   bonus += tuning::CHARM_TARGET_BONUS; break;
        case Vibe::Obnoxious:  bonus += tuning::OBNOXIOUS_TARGET_BONUS; break;
        case Vibe::Wallflower: break;
    }
    return bonus;
}

Game::Vibe Game::vibe() const {
    if (vibe_ < tuning::VIBE_WALLFLOWER) return Vibe::Wallflower;
    if (vibe_ > tuning::VIBE_OBNOXIOUS) return Vibe::Obnoxious;
    return Vibe::Charming;
}

float Game::player_turn_seconds() const {
    float t = tuning::PLAYER_TURN_SECONDS;
    if (vibe() == Vibe::Wallflower) t *= tuning::WALLFLOWER_TURN_FACTOR;
    return t;
}

float Game::grab_zone_center() const {
    return grab_center_ + tuning::GRAB_DRUNK_JITTER * buzz_.drunkenness() * std::sin(phase_time_ * 5.0f);
}

float Game::grab_zone_width() const {
    float width = tuning::GRAB_ZONE_SWEET;
    switch (buzz_.zone()) {
        case Zone::Sober:     width = tuning::GRAB_ZONE_SOBER; break;
        case Zone::SweetSpot: width = tuning::GRAB_ZONE_SWEET; break;
        case Zone::Drunk:     width = tuning::GRAB_ZONE_DRUNK; break;
        case Zone::Blackout:  width = tuning::GRAB_ZONE_BLACKOUT; break;
    }
    if (vibe() == Vibe::Charming) width *= tuning::GRAB_CHARM_FACTOR;
    return width;
}

// --- Render -------------------------------------------------------------------

void Game::render(Canvas& c) const {
    // The side strips first, then the game in the middle.
    c.set_area(0, gfx::STRIP_W);
    draw_strip(c, 0);
    c.set_area(gfx::SCREEN_W - gfx::STRIP_W, gfx::STRIP_W);
    draw_strip(c, 1);
    c.game_area();

    switch (state_) {
        case State::Title:
            draw_title(c);
            break;
        case State::Builder:
            draw_builder(c);
            break;
        case State::Party:
        case State::Paused: {
            draw_room(c);
            float sway = buzz_.drunkenness() * tuning::MAX_SWAY_PIXELS;
            if (buzz_.zone() == Zone::Blackout) sway *= 1.4f;
            if (sway > 0.0f) c.wave(HUD_H, TICKER_Y - 3, sway, state_time_ * 3.0f);
            if (phase_ == Phase::Reveal) draw_reveal(c);
            if (phase_ == Phase::Grab) draw_grab(c);
            if (tray_open_) draw_tray(c);
            if (banter_active_) draw_banter(c);
            draw_hud(c);
            draw_ticker(c);
            if (state_ == State::Paused) {
                c.fill_rect(60, 95, 120, 50, COL_HUD);
                c.rect(60, 95, 120, 50, COL_GOLD);
                c.text_centered(120, 104, "PAUSED", COL_GOLD, 2);
                c.text_centered(120, 128, "Y to resume", COL_DIM);
            }
            break;
        }
        case State::Scratch:
            draw_scratch(c);
            break;
        case State::Results:
            draw_results(c);
            break;
        case State::SeasonOver:
            draw_season_over(c);
            break;
        case State::CutOff:
            draw_cut_off(c);
            break;
        case State::Initials:
            draw_initials(c);
            break;
        case State::HighScores:
            draw_high_scores(c);
            break;
        case State::Idle:
            snow_.draw(c);
            break;
        case State::Sleep:
            c.clear(0);
            break;
    }

    if (volume_shown_ > 0.0f && state_ != State::Sleep) draw_volume(c);
}

// --- Volume ---------------------------------------------------------------------
//
// Two buttons step the volume up and down, on any screen. Holding one keeps
// stepping. A bar shows the level for a moment, each step ticks at the new
// volume, and the level is saved once the buttons are left alone.

void Game::update_volume(const Input& in, float dt) {
    const uint16_t both = BTN_VOL_UP | BTN_VOL_DOWN;
    int step = 0;
    if (in.pressed & both) {
        step = (in.pressed & BTN_VOL_UP) ? 1 : -1;
        volume_repeat_ = tuning::VOLUME_REPEAT_DELAY;
    } else if (in.held & both) {
        volume_repeat_ -= dt;
        if (volume_repeat_ <= 0.0f) {
            step = (in.held & BTN_VOL_UP) ? 1 : -1;
            volume_repeat_ = tuning::VOLUME_REPEAT_SECONDS;
        }
    }

    if (step) {
        int v = volume_ + step;
        if (v < 0) v = 0;
        if (v > tuning::VOLUME_STEPS) v = tuning::VOLUME_STEPS;
        if (v != volume_) {
            volume_ = v;
            apply_volume();
            volume_save_in_ = 2.0f;
        }
        volume_shown_ = tuning::VOLUME_SHOW_SECONDS;
        audio::play("volume", 1000, 30);  // hear the new level (silent at mute)
    }

    if (volume_shown_ > 0.0f) volume_shown_ -= dt;
    if (volume_save_in_ > 0.0f) {
        volume_save_in_ -= dt;
        if (volume_save_in_ <= 0.0f) save();
    }
}

// Volume steps feel even to the ear when the gain grows with the square.
void Game::apply_volume() const {
    float t = static_cast<float>(volume_) / tuning::VOLUME_STEPS;
    audio::set_master_volume(t * t);
}

// A small panel at the top: a speaker, then one segment per level.
void Game::draw_volume(Canvas& c) const {
    c.game_area();
    const int w = 10 + 12 + tuning::VOLUME_STEPS * 7 + 6;
    const int x = (W - w) / 2, y = 6;
    c.fill_rect(x, y, w, 20, COL_HUD);
    c.rect(x, y, w, 20, COL_DIM);

    // Speaker: a box and a cone.
    c.fill_rect(x + 6, y + 8, 3, 5, COL_TEXT);
    for (int i = 0; i < 4; ++i) c.fill_rect(x + 9 + i, y + 7 - i, 1, 7 + i * 2, COL_TEXT);

    int sx = x + 22;
    if (volume_ == 0) {
        c.text(sx, y + 7, "MUTE", COL_DANGER);
        return;
    }
    for (int i = 0; i < tuning::VOLUME_STEPS; ++i) {
        int h = 4 + i;  // segments climb like a ramp
        bool on = i < volume_;
        c.fill_rect(sx + i * 7, y + 16 - h, 5, h, on ? COL_GOLD : COL_STRIP);
    }
}

// --- Side strips: string lights ------------------------------------------------
//
// A strand of bulbs hangs down each side of the screen. Off the party they
// just twinkle. At the party they chase faster as your buzz climbs, the
// strands swing when you're drunk, everything flashes red past the blackout
// line, a gold chase runs up after you steal, and getting cut off kills them.

void Game::update_lights(float dt) {
    float speed = 0.6f;
    if (state_ == State::Party) {
        switch (buzz_.zone()) {
            case Zone::Sober:     speed = 0.7f; break;
            case Zone::SweetSpot: speed = 1.3f; break;
            case Zone::Drunk:     speed = 2.4f; break;
            case Zone::Blackout:  speed = 3.6f; break;
        }
    }
    lights_clock_ += dt * speed;
    if (lights_flash_ > 0.0f) lights_flash_ -= dt;
}

void Game::draw_strip(Canvas& c, int side) const {
    if (state_ == State::Sleep) {
        c.clear(0);
        return;
    }
    c.clear(COL_STRIP);

    bool party = state_ == State::Party || state_ == State::Paused;
    bool blackout = party && buzz_.over_limit_seconds() > 0.0f;
    bool dead = state_ == State::CutOff;
    float drunk = party ? buzz_.drunkenness() : 0.0f;

    // The strand hangs from the top, so it swings most at the bottom.
    float swing = drunk * 7.0f * std::sin(lights_clock_ * 1.1f + side * 2.0f);
    auto wire_x = [&](int y) {
        return gfx::STRIP_W / 2.0f + 4.0f * std::sin(y * 0.14f + side * 1.5f) + swing * y / gfx::H;
    };

    constexpr int BULBS = 10;
    struct Bulb {
        int x, y, dir;
        Color color;
        float glow;  // > 0.5 glows, > -0.3 is lit, otherwise dark
    } bulbs[BULBS];

    for (int i = 0; i < BULBS; ++i) {
        Bulb& b = bulbs[i];
        b.y = 12 + i * 23;
        b.x = static_cast<int>(wire_x(b.y));
        b.dir = (i + side) % 2 ? 1 : -1;  // bulbs point out to alternate sides
        b.color = BULB_COLORS[(i + side * 2) % 5];

        if (dead) {
            // One bulb sputters on; the rest are out.
            bool sputter = i == 3 + side * 3 &&
                           std::sin(state_time_ * 23.0f) + std::sin(state_time_ * 7.3f + side) > 1.1f;
            b.glow = sputter ? 0.0f : -1.0f;
        } else if (blackout) {
            b.color = COL_DANGER;
            b.glow = blink(state_time_, 3.0f) ? 1.0f : -1.0f;
        } else if (lights_flash_ > 0.0f) {
            b.color = COL_GOLD;
            int step = static_cast<int>((STEAL_FLASH_SECONDS - lights_flash_) * 18.0f);
            int from_bottom = BULBS - 1 - i;
            b.glow = ((step - from_bottom) % 3 + 3) % 3 == 0 ? 1.0f : -0.2f;
        } else {
            // A wave of brightness running down the strand.
            b.glow = std::sin(lights_clock_ * 3.0f - i * 0.9f + side * 0.5f);
        }
    }

    // Glows behind everything, then the wire, then the bulbs on top.
    for (const Bulb& b : bulbs) {
        if (b.glow > 0.5f) c.fill_circle(b.x + b.dir * 7, b.y, 6, gfx::dim(gfx::dim(b.color)));
    }
    for (int y = 0; y < gfx::H; ++y) c.fill_rect(static_cast<int>(wire_x(y)), y, 2, 1, COL_WIRE);
    for (const Bulb& b : bulbs) {
        bool lit = b.glow > -0.3f;
        Color body = lit ? b.color : gfx::dim(gfx::dim(b.color));
        int bx = b.x + b.dir * 7;
        c.fill_rect(b.dir > 0 ? b.x + 1 : b.x - 3, b.y - 1, 3, 3, COL_SOCKET);
        c.fill_circle(bx, b.y, 3, body);
        c.pixel(bx + b.dir * 4, b.y, body);  // the pointed tip
        if (lit) c.pixel(bx - b.dir, b.y - 2, rgb(255, 255, 255));
    }
}

void Game::draw_title(Canvas& c) const {
    // Every few seconds the title gives way to the high score board.
    int page = static_cast<int>(state_time_ / tuning::TITLE_SCORES_EVERY_SECONDS);
    if (page % 2 == 1 && reset_hold_ <= 0.0f) {
        draw_high_scores(c);
        return;
    }

    c.clear(COL_BG);

    // String lights chase along the top.
    int phase = static_cast<int>(state_time_ * 4.0f);
    for (int i = 0; i < 20; ++i) {
        c.fill_circle(6 + i * 12, 12, 2, GIFT_COLORS[(i + phase) % 5]);
    }

    c.text_centered(ROOM_CX, 34, content::TITLE_TOP, COL_GOLD, 4);
    c.text_centered(ROOM_CX, 68, content::TITLE_BOTTOM, COL_TEXT, 4);
    c.text_centered(ROOM_CX, 104, content::SUBTITLE, COL_DIM);

    draw_gift(c, 96, 134, 48, 36, GIFT_COLORS[0]);

    if (blink(state_time_, 1.0f)) c.text_centered(ROOM_CX, 186, "PRESS A", COL_TEXT, 2);
    c.text_centered(ROOM_CX, 216, "A:act B:drink X:hide Y:pause", COL_DIM);

    // Holding the reset combo: show it's working.
    if (reset_hold_ > 0.5f) {
        c.fill_rect(30, 92, 180, 52, COL_HUD);
        c.rect(30, 92, 180, 52, COL_DANGER);
        c.text_centered(ROOM_CX, 100, "Keep holding to reset", COL_TEXT);
        c.text_centered(ROOM_CX, 112, "the high scores", COL_TEXT);
        int fill = static_cast<int>(160 * reset_hold_ / tuning::SCORE_RESET_HOLD_SECONDS);
        c.rect(40, 128, 160, 8, COL_DIM);
        c.fill_rect(40, 128, fill > 160 ? 160 : fill, 8, COL_DANGER);
    }
}

void Game::draw_initials(Canvas& c) const {
    c.clear(COL_BG);
    int phase = static_cast<int>(state_time_ * 4.0f);
    for (int i = 0; i < 20; ++i) {
        c.fill_circle(6 + i * 12, 12, 2, GIFT_COLORS[(i + phase) % 5]);
    }
    c.text_centered(ROOM_CX, 30, "NEW HIGH SCORE!", COL_GOLD, 2);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "$%d", run_score_);
    c.text_centered(ROOM_CX, 54, buf, COL_TEXT, 3);
    std::snprintf(buf, sizeof(buf), "Number %d on the board", score_rank(run_score_) + 1);
    c.text_centered(ROOM_CX, 84, buf, COL_DIM);

    // Three big letters; the one being edited has arrows and an underline.
    for (int i = 0; i < 3; ++i) {
        int cx = ROOM_CX + (i - 1) * 40;
        char letter[2] = {initials_[i], '\0'};
        bool active = i == initials_slot_;
        c.text_centered(cx, 122, letter, active ? COL_GOLD : COL_TEXT, 4);
        if (active) {
            draw_arrow(c, cx - 3, 110, 0, -1, COL_GOLD);
            draw_arrow(c, cx - 3, 160, 0, 1, COL_GOLD);
            if (blink(state_time_, 2.0f)) c.fill_rect(cx - 12, 154, 23, 2, COL_GOLD);
        }
    }

    c.text_centered(ROOM_CX, 184, "Up/Down: letter", COL_DIM);
    c.text_centered(ROOM_CX, 198, initials_slot_ < 2 ? "A: next   B: back" : "A: done   B: back", COL_DIM);
}

void Game::draw_high_scores(Canvas& c) const {
    c.clear(COL_BG);
    int phase = static_cast<int>(state_time_ * 4.0f);
    for (int i = 0; i < 20; ++i) {
        c.fill_circle(6 + i * 12, 12, 2, GIFT_COLORS[(i + phase) % 5]);
    }
    c.text_centered(ROOM_CX, 24, "HIGH SCORES", COL_GOLD, 2);

    char buf[16];
    for (int i = 0; i < tuning::HIGH_SCORE_COUNT; ++i) {
        int y = 46 + i * 16;
        bool mine = i == new_rank_;
        Color col = mine ? (blink(state_time_, 2.0f) ? COL_GOLD : COL_TEXT) : (i < score_count_ ? COL_TEXT : COL_DIM);
        std::snprintf(buf, sizeof(buf), "%d.", i + 1);
        c.text(56 - text_width(buf, 2), y, buf, col, 2);
        if (i < score_count_) {
            c.text(70, y, scores_[i].initials, col, 2);
            std::snprintf(buf, sizeof(buf), "$%d", scores_[i].score);
            c.text(200 - text_width(buf, 2), y, buf, col, 2);
        } else {
            c.text(70, y, "---", col, 2);
        }
    }

    if (scores_were_reset_ && state_ == State::HighScores) {
        c.text_centered(ROOM_CX, 208, "Board reset", COL_DANGER);
    }
    if (state_time_ > 0.5f && blink(state_time_, 1.0f)) c.text_centered(ROOM_CX, 222, "PRESS A", COL_TEXT);
}

void Game::draw_room(Canvas& c) const {
    c.clear(COL_BG);

    // The room behind everyone (or a plain rug without art).
    if (spr_room_) {
        c.sprite(spr_room_, 0, HUD_H);
    } else {
        c.fill_rect(34, 62, 172, 128, COL_RUG);
        c.rect(38, 66, 164, 120, dim(COL_GOLD));
    }

    // String lights along the wall.
    int phase = static_cast<int>(state_time_ * 2.0f);
    for (int i = 0; i < 20; ++i) {
        c.fill_circle(6 + i * 12, HUD_H + 2, 1, GIFT_COLORS[(i + phase) % 5]);
    }

    draw_tree(c, ROOM_CX, ROOM_CY);
    for (int s = 0; s < seat_count_; ++s) draw_guest(c, s);

    // Selection cursor.
    Color cursor = blink(state_time_, 2.0f) ? COL_GOLD : COL_TEXT;
    if (cursor_ == TREE) {
        c.rect(ROOM_CX - 34, ROOM_CY - 37, 69, 75, cursor);
    } else {
        Point p = seats_[cursor_];
        c.rect(p.x - 15, p.y - 23, 31, 45, cursor);
    }
}

void Game::draw_tree(Canvas& c, int cx, int cy) const {
    if (spr_tree_) {
        // 64x64, centered on the tree's spot; frames cycle for twinkling.
        c.sprite(spr_tree_, cx - 32, cy - 38, static_cast<int>(state_time_ * 2.0f));
    } else {
        c.fill_rect(cx - 4, cy + 15, 9, 6, COL_TRUNK);
        for (int t = 0; t < 4; ++t) {
            int w = 14 + t * 12;
            c.fill_rect(cx - w / 2, cy - 26 + t * 10, w, 11, COL_TREE);
        }
        // Ornaments twinkle.
        static constexpr int ORNAMENTS[][2] = {{-4, -20}, {5, -11}, {-10, -4}, {12, 2}, {-18, 8}, {2, 6}, {20, 11}};
        int i = 0;
        int phase = static_cast<int>(state_time_ * 3.0f);
        for (const auto& o : ORNAMENTS) {
            c.fill_rect(cx + o[0], cy + o[1], 2, 2, GIFT_COLORS[(i++ + phase) % 5]);
        }
        // Star.
        c.fill_rect(cx - 1, cy - 34, 3, 7, COL_GOLD);
        c.fill_rect(cx - 3, cy - 32, 7, 3, COL_GOLD);
    }

    // The unopened pile, one box per wrapped gift.
    int slot = 0;
    for (int g = 0; g < party_.gift_count(); ++g) {
        if (party_.gift(g).holder >= 0) continue;
        const int* p = PILE[slot++];
        int wrap = party_.gift(g).wrap;
        if (spr_gift_wrapped_) {
            c.sprite(spr_gift_wrapped_, cx + p[0], cy + p[1] - 3, wrap);  // frame = wrapping paper
        } else {
            draw_gift(c, cx + p[0], cy + p[1], p[2], p[3], GIFT_COLORS[wrap % 5]);
        }
    }
}

void Game::draw_person(Canvas& c, const content::Look& look, int x, int y, int frame, int scale) const {
    Color skin[4], hair[4], knit[4];
    to_ramp(content::SKIN_TONES[look.skin % SKIN_COUNT], skin);
    to_ramp(content::HAIR_COLORS[look.hair_color % HAIR_COLOR_COUNT], hair);
    to_ramp(content::SWEATER_COLORS[look.sweater_color % KNIT_COUNT], knit);

    if (!spr_body_) {
        // No people art yet: a simple head and sweater.
        c.fill_rect(x + 4 * scale, y + 18 * scale, 17 * scale, 12 * scale, knit[1]);
        c.fill_circle(x + 12 * scale, y + 10 * scale, 7 * scale, skin[1]);
        return;
    }
    // Body (with this frame's face), then sweater, hair, and hat on top.
    // Hats share the sweater's colors. Some hats tuck the hair: its second
    // frame, if it has one.
    const Sprite* hair_spr = spr_hair_[look.hair % HAIR_COUNT];
    const content::HatStyle& hat = content::HATS[look.hat % HAT_COUNT];
    int hair_frame = hat.tucks_hair && hair_spr && hair_spr->frames > 1 ? 1 : 0;
    c.sprite(spr_body_, x, y, frame, false, skin, scale);
    c.sprite(spr_sweater_[look.sweater % SWEATER_COUNT], x, y, 0, false, knit, scale);
    c.sprite(hair_spr, x, y, hair_frame, false, hair, scale);
    c.sprite(spr_hat_[look.hat % HAT_COUNT], x, y, 0, false, knit, scale);
}

void Game::draw_builder(Canvas& c) const {
    c.clear(COL_BG);
    int phase = static_cast<int>(state_time_ * 4.0f);
    for (int i = 0; i < 20; ++i) c.fill_circle(6 + i * 12, 4, 1, GIFT_COLORS[(i + phase) % 5]);
    c.text_centered(ROOM_CX, 10, "WHO ARE YOU?", COL_GOLD, 2);

    // Big preview that cycles through your expressions.
    c.fill_rect(6, 34, 84, 112, COL_RUG);
    c.rect(6, 34, 84, 112, dim(COL_GOLD));
    draw_person(c, look_, 12, 42, static_cast<int>(state_time_ * 1.25f) % 4, 3);

    // Option rows.
    static constexpr const char* LABELS[ROW_COUNT] = {"SKIN", "HAIR", "COLOR", "SWEATER", "COLOR", "HAT", "", ""};
    const int label_x = 104, value_cx = 192, row_h = 22, top = 36;
    for (int row = 0; row < ROW_COUNT; ++row) {
        const int y = top + row * row_h;
        const bool on = row == builder_row_;
        if (on) draw_arrow(c, label_x - 9, y + 1, 1, 0, COL_GOLD);

        auto swatches = [&](const content::Ramp* ramps, int count, int selected) {
            const int x0 = value_cx - (count * 14 - 4) / 2;
            for (int i = 0; i < count; ++i) {
                c.fill_rect(x0 + i * 14, y - 1, 10, 10, hex(ramps[i].shade[1]));
                if (i == selected) c.rect(x0 + i * 14 - 2, y - 3, 14, 14, on ? COL_GOLD : COL_TEXT);
            }
        };
        auto choice = [&](const char* name) {
            c.text_centered(value_cx, y, name, on ? COL_TEXT : COL_DIM);
            if (on) {
                draw_arrow(c, value_cx - 44, y + 1, -1, 0, COL_GOLD);
                draw_arrow(c, value_cx + 40, y + 1, 1, 0, COL_GOLD);
            }
        };

        switch (row) {
            case ROW_SKIN:       swatches(content::SKIN_TONES, SKIN_COUNT, look_.skin); break;
            case ROW_HAIR:       choice(content::HAIR_STYLES[look_.hair]); break;
            case ROW_HAIR_COLOR: swatches(content::HAIR_COLORS, HAIR_COLOR_COUNT, look_.hair_color); break;
            case ROW_SWEATER:    choice(content::SWEATERS[look_.sweater]); break;
            case ROW_KNIT:       swatches(content::SWEATER_COLORS, KNIT_COUNT, look_.sweater_color); break;
            case ROW_HAT:        choice(content::HATS[look_.hat].name); break;
            case ROW_SURPRISE:
                c.text(label_x, y, "SURPRISE ME", on ? COL_CHARM : COL_DIM);
                break;
            case ROW_DONE:
                if (on) c.fill_rect(label_x - 3, y - 3, 122, 13, dim(dim(COL_GOLD)));
                c.text(label_x, y, "LET'S PARTY!", on ? COL_GOLD : COL_DIM);
                break;
            default: break;
        }
        if (LABELS[row][0]) c.text(label_x, y, LABELS[row], on ? COL_GOLD : COL_DIM);
    }

    c.text_centered(ROOM_CX, 224, "Up/Down: pick  Left/Right: change", COL_DIM);
}

void Game::draw_guest(Canvas& c, int seat) const {
    Point p = seats_[seat];
    bool is_player = seat == PLAYER;
    bool acting = party_.actor() == seat;

    // 24x32 frames: 0 idle, 1 talking, 2 thinking, 3 drunk.
    int frame = 0;
    if (banter_active_ && seat == banter_seat_) frame = 1;
    else if (acting && phase_ == Phase::Thinking) frame = 2;
    else if (is_player && buzz_.drunkenness() > 0.0f) frame = 3;

    if (const Sprite* art = spr_person_[seat]) {
        c.sprite(art, p.x - 12, p.y - 10, frame);  // a hand-drawn guest
    } else {
        draw_person(c, look_of(seat), p.x - 12, p.y - 10, frame);
    }

    // You're holding a drink while one is still soaking in (unless both hands are on your gift).
    if (is_player && buzz_.holding_drink() && !hide_held_) {
        c.fill_rect(p.x + 10, p.y + 9, 5, 7, COL_BEER);
        c.fill_rect(p.x + 10, p.y + 8, 5, 2, COL_TEXT);
    }

    // The gift they're holding, with its value. Frozen gifts are iced over.
    int g = party_.gift_of(seat);
    if (g >= 0 && is_player && hide_held_ && !caught_hiding()) {
        // Sneakily tucked behind you: just a corner peeking out.
        Color box = dim(GIFT_COLORS[party_.gift(g).wrap % 5]);
        c.fill_rect(p.x - 14, p.y + 12, 5, 6, box);
        c.fill_rect(p.x - 12, p.y + 12, 1, 6, dim(COL_GOLD));
    } else if (g >= 0) {
        char value[16];
        value_text(g, value, sizeof(value));
        bool frozen = party_.frozen(g);
        int width = 12 + text_width(value);
        int x0 = p.x - width / 2;
        int y0 = p.y - 20;
        if (spr_gift_open_) {
            c.sprite(spr_gift_open_, x0 - 1, y0, party_.gift(g).wrap);  // frame = wrapping paper
        } else {
            Color box = GIFT_COLORS[party_.gift(g).wrap % 5];
            c.fill_rect(x0, y0 + 2, 10, 6, box);
            c.fill_rect(x0 - 1, y0, 12, 2, box);  // lid
            c.fill_rect(x0 + 4, y0, 2, 8, COL_GOLD);
        }
        if (frozen) c.rect(x0 - 2, y0 - 1, 14, 10, COL_ICE);
        if (is_player && caught_hiding() && blink(state_time_, 3.0f)) c.rect(x0 - 2, y0 - 1, 14, 10, COL_DANGER);
        Color vc = frozen ? COL_ICE : (party_.gift(g).value < 0 && !value_hidden(party_, g) ? COL_DANGER : COL_GOLD);
        c.text(x0 + 13, y0 + 1, value, vc);
    }

    // Whose move is it?
    if (acting && phase_ == Phase::Thinking) {
        int dots = static_cast<int>(phase_time_ * 3.0f) % 4;
        for (int i = 0; i < dots; ++i) c.fill_rect(p.x + 10 + i * 3, p.y - 4, 2, 2, COL_TEXT);
    }

    // Speech bubble on whoever is talking to you.
    if (banter_active_ && seat == banter_seat_) {
        c.fill_rect(p.x - 18, p.y - 13, 9, 7, COL_TEXT);
        c.fill_rect(p.x - 10, p.y - 7, 2, 2, COL_TEXT);
        c.fill_rect(p.x - 16, p.y - 11, 1, 1, COL_HUD);
        c.fill_rect(p.x - 14, p.y - 11, 1, 1, COL_HUD);
        c.fill_rect(p.x - 12, p.y - 11, 1, 1, COL_HUD);
    }

    const char* label = label_of(seat);
    Color label_color = is_player ? COL_GOLD : COL_TEXT;
    if (acting && (phase_ == Phase::Thinking || phase_ == Phase::PlayerTurn)) {
        label_color = blink(phase_time_, 3.0f) ? COL_GOLD : COL_SWEET;
    }
    c.text_centered(p.x, p.y + 23, label, label_color);

    // Your turn timer, next to your name.
    if (is_player && phase_ == Phase::PlayerTurn) {
        int left = static_cast<int>(std::ceil(player_turn_seconds() - phase_time_));
        char buf[12];
        std::snprintf(buf, sizeof(buf), "%d", left < 0 ? 0 : left);
        c.text(p.x + 14, p.y + 23, buf, left <= 5 ? COL_DANGER : COL_GOLD);
    }
}

void Game::draw_tray(Canvas& c) const {
    const int x = 8, y = HUD_H + 4, w = 224, h = 42;
    c.fill_rect(x, y, w, h, COL_HUD);
    c.rect(x, y, w, h, COL_GOLD);

    const int inner_x = x + 8, inner_w = w - 16;
    const int under = tray_slot_under_hand();
    for (int i = 0; i < tuning::TRAY_SLOTS; ++i) {
        const int d = tray_[i];
        const int sx = inner_x + i * inner_w / tuning::TRAY_SLOTS;
        const int sw = inner_w / tuning::TRAY_SLOTS;
        const int cx = sx + sw / 2;

        if (i == under) c.fill_rect(sx + 1, y + 13, sw - 2, 22, dim(dim(COL_GOLD)));

        // No names, just what it looks like: same-looking drinks are a gamble.
        if (spr_drink_[d]) {
            c.sprite(spr_drink_[d], cx - spr_drink_[d]->w / 2, y + 14);
        } else {
            draw_drink_look(c, cx, y + 14, tuning::DRINKS[d].look);
        }
    }

    // Your hand, pointing down at whatever it's over.
    int hx = inner_x + static_cast<int>(hand_pos_ * (inner_w - 1));
    if (spr_hand_) {
        c.sprite(spr_hand_, hx - spr_hand_->w / 2, y + 2);
    } else {
        c.fill_rect(hx - 4, y + 2, 9, 6, COL_SKIN);  // back of the hand
        for (int f = 0; f < 4; ++f) c.fill_rect(hx - 4 + f * 2 + (f > 1), y + 8, 1, 3, COL_SKIN);
    }

    // Time left before you grab whatever's under your hand.
    float left = 1.0f - tray_time_ / tuning::TRAY_SECONDS;
    if (left < 0.0f) left = 0.0f;
    c.fill_rect(x + 6, y + h - 5, static_cast<int>((w - 12) * left), 2, COL_GOLD);
}

void Game::draw_banter(Canvas& c) const {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%s: \"%s\"", label_of(banter_seat_), banter_line_);
    float left = 1.0f - banter_time_ / tuning::BANTER_SECONDS;
    if (left < 0.0f) left = 0.0f;

    // The answers sit around a D-pad, on the side you push.
    const int x = 8, y = 152, w = 224, h = 70;  // reaches the ticker, covering your seat
    c.fill_rect(x, y, w, h, COL_HUD);
    c.rect(x, y, w, h, COL_CHARM);
    c.text_wrapped(ROOM_CX, y + 4, buf, COL_TEXT, w - 8);

    const int cx = ROOM_CX, cy = y + 41;
    const Color pad = dim(dim(COL_TEXT));
    c.fill_rect(cx - 3, cy - 9, 7, 19, pad);
    c.fill_rect(cx - 9, cy - 3, 19, 7, pad);
    draw_arrow(c, cx - 2, cy - 8, 0, -1, COL_TEXT);
    draw_arrow(c, cx - 2, cy + 4, 0, 1, COL_TEXT);
    draw_arrow(c, cx - 8, cy - 2, -1, 0, COL_TEXT);
    draw_arrow(c, cx + 4, cy - 2, 1, 0, COL_TEXT);

    // Colors hint at the risk: safe, sweet, spicy, and checking out.
    c.text_centered(cx, cy - 19, "Laugh", COL_SWEET);
    c.text_centered(cx, cy + 12, "Ignore", COL_DIM);
    c.text(cx - 13 - text_width("Flatter"), cy - 3, "Flatter", COL_CHARM);
    c.text(cx + 14, cy - 3, "Roast", COL_DANGER);

    // Time left to answer.
    c.fill_rect(x + 6, y + h - 4, static_cast<int>((w - 12) * left), 2, COL_CHARM);
}

void Game::draw_hud(Canvas& c) const {
    c.fill_rect(0, 0, W, HUD_H, COL_HUD);
    char buf[32];
    const int r = 29;
    const int cy = HUD_H - 3;

    // BUZZ, top left. The dotted ghost needle shows where you're headed as
    // drinks soak in; the real needle trembles when you're drunk.
    const int bx = 34;
    draw_dial(c, bx, cy, r, tuning::SWEET_SPOT_LOW, tuning::SWEET_SPOT_HIGH, zone_color,
              {tuning::SWEET_SPOT_LOW, tuning::SWEET_SPOT_HIGH, tuning::BLACKOUT});
    c.text_centered(bx, cy - 9, "BUZZ", COL_DIM);
    float heading = buzz_.level() + buzz_.pending();
    if (heading < 0.0f) heading = 0.0f;
    if (heading > 100.0f) heading = 100.0f;
    if (std::fabs(buzz_.pending()) > 1.0f) {
        draw_needle(c, bx, cy, r - 3, dial_angle(heading, tuning::SWEET_SPOT_LOW, tuning::SWEET_SPOT_HIGH),
                    COL_GHOST, true);
    }
    float tremble = std::sin(state_time_ * 11.0f) * buzz_.drunkenness() * tuning::NEEDLE_DRUNK_TREMBLE;
    draw_needle(c, bx, cy, r - 3,
                dial_angle(needle_buzz_, tuning::SWEET_SPOT_LOW, tuning::SWEET_SPOT_HIGH) + tremble,
                zone_color(buzz_.level()), false);
    c.fill_circle(bx, cy, 2, COL_TEXT);

    // VIBE, top right.
    const int vx = W - 34;
    draw_dial(c, vx, cy, r, tuning::VIBE_WALLFLOWER, tuning::VIBE_OBNOXIOUS, vibe_color,
              {tuning::VIBE_WALLFLOWER, tuning::VIBE_OBNOXIOUS});
    c.text_centered(vx, cy - 9, "VIBE", COL_DIM);
    draw_needle(c, vx, cy, r - 3, dial_angle(needle_vibe_, tuning::VIBE_WALLFLOWER, tuning::VIBE_OBNOXIOUS),
                vibe_color(vibe_), false);
    c.fill_circle(vx, cy, 2, COL_TEXT);

    // Middle column: zone names, score, hide gauge.
    const int left = 68, right = W - 68;
    switch (buzz_.zone()) {
        case Zone::Sober:     c.text(left, 3, "SOBER", COL_SOBER); break;
        case Zone::SweetSpot: c.text(left, 3, "SWEET", COL_SWEET); break;
        case Zone::Drunk:     c.text(left, 3, "DRUNK", COL_DRUNK); break;
        case Zone::Blackout: {
            float t = tuning::BLACKOUT_SECONDS - buzz_.over_limit_seconds();
            std::snprintf(buf, sizeof(buf), "CUT %.1f", t < 0.0f ? 0.0f : t);
            if (blink(state_time_, 4.0f)) c.text(left, 3, buf, COL_DANGER);
            break;
        }
    }
    const char* vibe_word = vibe() == Vibe::Wallflower ? "SHY" : vibe() == Vibe::Obnoxious ? "RUDE" : "CHARM";
    c.text(right - text_width(vibe_word), 3, vibe_word, vibe_color(vibe_));

    std::snprintf(buf, sizeof(buf), "$%d",
                  run_score_ + static_cast<int>(party_score_) + static_cast<int>(charm_score_));
    c.text_centered(ROOM_CX, 14, buf, COL_GOLD);

    const int sw = 30;
    const int sx = ROOM_CX - (sw + 27) / 2 + 27;
    Color hide_label = caught_hiding() ? COL_DANGER : (hide_held_ ? COL_SWEET : COL_DIM);
    c.text(sx - 27, 25, "HIDE", hide_label);
    c.fill_rect(sx, 26, sw, 5, dim(dim(COL_TEXT)));
    int fill = static_cast<int>(suspicion_ / tuning::HIDE_MAX_SUSPICION * sw);
    c.fill_rect(sx, 26, fill, 5, caught_hiding() ? COL_DANGER : COL_SWEET);
    c.fill_rect(sx + static_cast<int>(tuning::HIDE_OBVIOUS_SECONDS / tuning::HIDE_MAX_SUSPICION * sw), 24, 1, 9,
                COL_TEXT);
}

void Game::draw_ticker(Canvas& c) const {
    c.fill_rect(0, TICKER_Y - 2, W, H - TICKER_Y + 2, COL_HUD);
    int x = 6;
    int w = text_width(ticker_);
    int room = W - 12;
    if (w > room) {
        // Long lines hold for a second, then scroll to show the end.
        float scroll = (ticker_age_ - 1.0f) * 40.0f;
        if (scroll < 0.0f) scroll = 0.0f;
        if (scroll > w - room) scroll = static_cast<float>(w - room);
        x -= static_cast<int>(scroll);
    }
    c.text(x, TICKER_Y + 5, ticker_, COL_TEXT);
    c.fill_rect(0, TICKER_Y - 2, 4, H - TICKER_Y + 2, COL_HUD);
    c.fill_rect(W - 4, TICKER_Y - 2, 4, H - TICKER_Y + 2, COL_HUD);
}

void Game::draw_reveal(Canvas& c) const {
    const int x = 16, y = 64, w = 208, h = 86;  // room for a two-line caption; clears the banter panel
    c.fill_rect(x, y, w, h, COL_HUD);
    c.rect(x, y, w, h, GIFT_COLORS[party_.gift(reveal_gift_).wrap % 5]);

    char buf[48];
    std::snprintf(buf, sizeof(buf), "%s unwraps...", label_of(reveal_seat_));
    c.text_centered(ROOM_CX, y + 6, buf, COL_DIM);

    const content::Gift& info = gift_info(reveal_gift_);
    text_fit(c, ROOM_CX, y + 20, info.name, COL_TEXT, w - 12);

    value_text(reveal_gift_, buf, sizeof(buf));
    bool bad = party_.gift(reveal_gift_).value < 0 && !value_hidden(party_, reveal_gift_);
    c.text_centered(ROOM_CX, y + 40, buf, bad ? COL_DANGER : COL_GOLD, 2);
    c.text_wrapped(ROOM_CX, y + 60, info.caption, COL_DIM, w - 12);
}

void Game::draw_grab(Canvas& c) const {
    const int x = 16, y = 88, w = 208, h = 60;
    c.fill_rect(x, y, w, h, COL_HUD);
    c.rect(x, y, w, h, COL_GOLD);

    char buf[48];
    std::snprintf(buf, sizeof(buf), "STEAL: %s", gift_info(grab_gift_).name);
    c.text_centered(ROOM_CX, y + 6, buf, COL_GOLD);

    // The bar, the target zone, and the sweeping marker.
    const int bx = x + 14, by = y + 22, bw = w - 28, bh = 14;
    c.fill_rect(bx, by, bw, bh, dim(dim(COL_TEXT)));
    float center = grab_zone_center();
    float half = grab_zone_width() * 0.5f;
    int z0 = bx + static_cast<int>((center - half) * bw);
    int z1 = bx + static_cast<int>((center + half) * bw);
    if (z0 < bx) z0 = bx;
    if (z1 > bx + bw) z1 = bx + bw;
    c.fill_rect(z0, by, z1 - z0, bh, zone_color(buzz_.level()));
    int mx = bx + static_cast<int>(grab_pos_ * (bw - 1));
    c.fill_rect(mx - 1, by - 3, 3, bh + 6, COL_TEXT);

    c.text_centered(ROOM_CX, y + 44, "Press A in the zone!", COL_DIM);
}

void Game::draw_scratch(Canvas& c) const {
    c.clear(COL_BG);
    c.text_centered(ROOM_CX, 10, "SCRATCH TIME", COL_GOLD, 2);
    c.text_centered(ROOM_CX, 32, "Find the matching pair!", COL_DIM);

    // The ticket.
    const Color paper = rgb(250, 240, 205);
    const Color ink = rgb(70, 30, 30);
    const Color coating = rgb(170, 172, 185);
    c.fill_rect(20, 46, 200, 142, paper);
    c.rect(20, 46, 200, 142, COL_DANGER);
    c.rect(22, 48, 196, 138, COL_DANGER);

    const int cell = 52, gap = 10;
    const int x0 = ROOM_CX - (ScratchTicket::COLUMNS * cell + (ScratchTicket::COLUMNS - 1) * gap) / 2;
    const int y0 = 60;
    for (int i = 0; i < ScratchTicket::SPOTS; ++i) {
        const int x = x0 + (i % ScratchTicket::COLUMNS) * (cell + gap);
        const int y = y0 + (i / ScratchTicket::COLUMNS) * (cell + gap);
        const bool pair = scratch_won_ && ticket_.values[i] == ticket_.prize;

        // What's underneath.
        c.fill_rect(x, y, cell, cell, pair ? COL_GOLD : paper);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "$%d", ticket_.values[i]);
        c.text_centered(x + cell / 2, y + cell / 2 - 7, buf, pair ? COL_DANGER : ink, 2);

        // The silver coating, speckling away as you scratch.
        float progress = ticket_.scratched[i];
        if (progress < 1.0f) {
            for (int py = 0; py < cell; ++py) {
                for (int px = 0; px < cell; ++px) {
                    uint32_t h = static_cast<uint32_t>((px * 37 + py * 91 + i * 13) * 2654435761u) >> 24;
                    if (h / 255.0f >= progress) {
                        c.pixel(x + px, y + py, ((px + py) & 3) ? coating : dim(coating));
                    }
                }
            }
        }
        c.rect(x - 1, y - 1, cell + 2, cell + 2, ink);
    }

    // Which spot your hand is on.
    if (!scratch_won_) {
        const int x = x0 + (scratch_cursor_ % ScratchTicket::COLUMNS) * (cell + gap);
        const int y = y0 + (scratch_cursor_ / ScratchTicket::COLUMNS) * (cell + gap);
        Color cursor = blink(state_time_, 2.0f) ? COL_GOLD : COL_DANGER;
        c.rect(x - 3, y - 3, cell + 6, cell + 6, cursor);
        c.rect(x - 4, y - 4, cell + 8, cell + 8, cursor);
        c.text_centered(ROOM_CX, 200, "Hold A to scratch", COL_TEXT);
    } else {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "WINNER! $%d", ticket_.prize);
        c.text_centered(ROOM_CX, 194, buf, COL_GOLD, 2);
        if (state_time_ - scratch_won_at_ > 0.8f && blink(state_time_, 1.0f)) {
            c.text_centered(ROOM_CX, 216, "A: continue", COL_TEXT);
        }
    }
}

void Game::draw_results(Canvas& c) const {
    c.clear(COL_BG);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "PARTY %d OVER", party_index_ + 1);
    c.text_centered(ROOM_CX, 10, buf, COL_GOLD, 2);

    c.text_centered(ROOM_CX, 34, "You leave with:", COL_DIM);
    if (results_.gift >= 0) {
        const content::Gift& info = gift_info(results_.gift);
        text_fit(c, ROOM_CX, 46, info.name, COL_TEXT, 228);
        c.text_wrapped(ROOM_CX, 64, info.caption, COL_DIM, W - 12);
    }

    // The score breakdown, one line each.
    int y = 86;
    auto line = [&](const char* label, int amount, Color col) {
        char num[16];
        std::snprintf(num, sizeof(num), amount < 0 ? "-$%d" : "$%d", amount < 0 ? -amount : amount);
        c.text(30, y, label, COL_TEXT);
        c.text(210 - text_width(num), y, num, col);
        y += 12;
    };
    line("Gift value", results_.gift_value, results_.gift_value < 0 ? COL_DANGER : COL_GOLD);
    line("Sweet spot", results_.sweet, COL_SWEET);
    line("Charm", results_.charm, COL_CHARM);
    std::snprintf(buf, sizeof(buf), "Steals x%d", steals_);
    line(buf, results_.steals, COL_GOLD);
    if (results_.frozen_bonus > 0) line("Frozen prize", results_.frozen_bonus, COL_ICE);
    if (scenes_ > 0) {
        std::snprintf(buf, sizeof(buf), "Scenes x%d", scenes_);
        line(buf, -results_.scenes, COL_DANGER);
    }
    c.fill_rect(30, y, 180, 1, COL_DIM);
    y += 5;
    line("Party total", results_.total, COL_GOLD);
    line("Run total", run_score_, COL_GOLD);

    if (state_time_ > 1.0f && blink(state_time_, 1.0f)) {
        bool last = party_index_ + 1 >= tuning::PARTIES_PER_SEASON;
        c.text_centered(ROOM_CX, 212, last ? "A: FINISH" : "A: NEXT PARTY", COL_TEXT, 2);
    }
}

void Game::draw_season_over(Canvas& c) const {
    c.clear(COL_BG);
    int phase = static_cast<int>(state_time_ * 4.0f);
    for (int i = 0; i < 20; ++i) {
        c.fill_circle(6 + i * 12, 12, 2, GIFT_COLORS[(i + phase) % 5]);
    }
    c.text_centered(ROOM_CX, 40, "SEASON", COL_GOLD, 3);
    c.text_centered(ROOM_CX, 66, "COMPLETE", COL_GOLD, 3);
    c.text_centered(ROOM_CX, 104, "You survived the holidays.", COL_DIM);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "$%d", run_score_);
    c.text_centered(ROOM_CX, 124, "FINAL SCORE", COL_TEXT);
    c.text_centered(ROOM_CX, 138, buf, COL_GOLD, 3);

    if (state_time_ > 1.0f && blink(state_time_, 1.0f)) {
        c.text_centered(ROOM_CX, 200, "PRESS A", COL_TEXT, 2);
    }
}

void Game::draw_cut_off(Canvas& c) const {
    c.clear(rgb(40, 8, 12));
    c.text_centered(ROOM_CX, 40, "CUT OFF!", COL_DANGER, 4);
    c.text_centered(ROOM_CX, 82, "Sent to the couch.", COL_TEXT);
    c.text_wrapped(ROOM_CX, 96, cut_off_line_, COL_DIM, W - 16);

    // The couch.
    if (spr_couch_) {
        c.sprite(spr_couch_, ROOM_CX - spr_couch_->w / 2, 128);
    } else {
        Color couch = rgb(120, 60, 50);
        c.fill_rect(70, 130, 100, 18, couch);
        c.fill_rect(64, 140, 112, 18, rgb(90, 42, 36));
        c.fill_rect(64, 132, 10, 26, couch);
        c.fill_rect(166, 132, 10, 26, couch);
    }

    c.text_centered(ROOM_CX, 168, "PARTY SCORE FORFEIT", COL_DRUNK);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "RUN SCORE $%d", run_score_);
    c.text_centered(ROOM_CX, 182, buf, COL_TEXT);

    if (state_time_ > 1.0f && blink(state_time_, 1.0f)) {
        c.text_centered(ROOM_CX, 204, "PRESS A", COL_TEXT, 2);
    }
}
