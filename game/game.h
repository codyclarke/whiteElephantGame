#pragma once
#include <cstdint>

#include "game/buzz.h"
#include "game/content.h"
#include "game/gfx.h"
#include "game/party.h"
#include "game/rng.h"
#include "game/scratch.h"
#include "game/snow.h"
#include "game/tuning.h"

// What the buttons are doing this tick: held = down right now,
// pressed = went down this tick (use this for one-shot actions).
struct Input {
    uint16_t held;
    uint16_t pressed;
};

// The whole game. The platform's main loop calls update() at a fixed rate
// and render() whenever it wants a frame -- a bit like Godot's
// _physics_process() and _draw().
class Game {
public:
    explicit Game(uint32_t seed);

    void update(const Input& in, float dt);
    void render(gfx::Canvas& c) const;

    // Current message line, for test logs.
    const char* ticker() const { return ticker_; }

    // Screen off: the platform can drop into low power until a button.
    bool sleeping() const { return state_ == State::Sleep; }

    // Put the high score board back to content::DEFAULT_HIGH_SCORES and save.
    void reset_high_scores();

    // Test mode: jump straight to the scratcher mini-game / character builder /
    // entering initials for a new high score.
    void debug_start_scratch() { start_scratch(); }
    void debug_start_builder() { set_state(State::Builder); }
    void debug_start_initials(int score) { run_score_ = score; finish_run(); }
    void debug_start_idle() { start_idle(); }

private:
    enum class State { Title, Builder, Party, Paused, CutOff, Scratch, Results, SeasonOver, Initials, HighScores,
                       Idle, Sleep };

    // Rows on the character builder screen, top to bottom.
    enum BuilderRow { ROW_SKIN, ROW_HAIR, ROW_HAIR_COLOR, ROW_SWEATER, ROW_KNIT, ROW_HAT, ROW_SURPRISE, ROW_DONE,
                      ROW_COUNT };

    static constexpr int SKIN_COUNT = sizeof(content::SKIN_TONES) / sizeof(content::SKIN_TONES[0]);
    static constexpr int HAIR_COUNT = sizeof(content::HAIR_STYLES) / sizeof(content::HAIR_STYLES[0]);
    static constexpr int HAIR_COLOR_COUNT = sizeof(content::HAIR_COLORS) / sizeof(content::HAIR_COLORS[0]);
    static constexpr int SWEATER_COUNT = sizeof(content::SWEATERS) / sizeof(content::SWEATERS[0]);
    static constexpr int KNIT_COUNT = sizeof(content::SWEATER_COLORS) / sizeof(content::SWEATER_COLORS[0]);
    static constexpr int HAT_COUNT = sizeof(content::HATS) / sizeof(content::HATS[0]);

    // What's happening within a party.
    enum class Phase {
        Thinking,    // an AI guest is about to act
        PlayerTurn,  // waiting for you to open or steal
        Grab,        // you're attempting a steal (timing mini-game)
        Reveal,      // someone just unwrapped a gift
    };

    static constexpr int GUEST_COUNT = sizeof(content::GUESTS) / sizeof(content::GUESTS[0]);
    static constexpr int GIFT_COUNT = sizeof(content::GIFTS) / sizeof(content::GIFTS[0]);
    static constexpr int PLAYER = 0;                   // you always sit in seat 0
    static constexpr int TREE = Party::MAX_SEATS;      // cursor target id for the gift pile

    struct Point {
        int x;
        int y;
    };

    enum class Vibe { Wallflower, Charming, Obnoxious };
    enum class Banter { Laugh, Flatter, Roast, Ignore, Timeout };

    struct Results {
        int gift = -1;  // index into party_ gifts
        int gift_value = 0;
        int sweet = 0;
        int charm = 0;
        int steals = 0;
        int frozen_bonus = 0;
        int scenes = 0;
        int total = 0;
    };

    // Flow
    void set_state(State s);
    void start_season();
    void start_party();
    void begin_turn(bool announce);
    void finish_party();
    void show_results(int gift_value);
    void start_scratch();
    void update_scratch(const Input& in, float dt);
    void update_builder(const Input& in);
    void save() const;  // your look and the high scores
    void load();
    void use_default_scores();  // in memory only; reset_high_scores() also saves
    void finish_run();  // after the season or a cut-off: high score or not
    int score_rank(int score) const;  // where it would land on the board, or -1
    void update_title(const Input& in, float dt);
    void update_initials(const Input& in);
    void start_idle();

    // Party update
    void update_party(const Input& in, float dt);
    void ai_act();
    void player_choose();
    void player_open();
    void start_grab(int gift);
    void update_grab(const Input& in, float dt);
    void start_reveal(int gift, int seat);
    void open_tray();
    void update_tray(const Input& in, float dt);
    void take_drink(int drink, bool fumbled);
    int tray_slot_under_hand() const;
    void move_cursor(int dx, int dy);
    void inspect();
    void update_hiding(const Input& in, float dt);
    bool hiding() const;  // holding X right now, with a gift to hide
    bool caught_hiding() const { return suspicion_ >= tuning::HIDE_OBVIOUS_SECONDS; }
    void update_vibe(float dt);
    void push_vibe(float amount);  // + toward obnoxious, - toward wallflower
    void pull_vibe(float amount);  // toward the middle
    void flop_vibe(float side, float amount);  // bad outcome: +1 rude side, -1 shy side
    void update_banter(const Input& in, float dt);
    void start_banter();
    void resolve_banter(Banter response);

    // Helpers
    void say(const char* fmt, ...);
    Point target_pos(int target) const;
    const char* label_of(int seat) const;  // "YOU" or the guest's name
    const char* who(int seat) const;       // "you" or the guest's name, for sentences
    const content::Gift& gift_info(int g) const;
    void value_text(int g, char* buf, int size) const;
    float player_target_bonus() const;
    Vibe vibe() const;
    float player_turn_seconds() const;
    float grab_zone_center() const;
    float grab_zone_width() const;

    // Drawing
    void draw_title(gfx::Canvas& c) const;
    void draw_room(gfx::Canvas& c) const;
    void draw_tree(gfx::Canvas& c, int cx, int cy) const;
    void draw_guest(gfx::Canvas& c, int seat) const;
    void draw_hud(gfx::Canvas& c) const;
    void draw_ticker(gfx::Canvas& c) const;
    void draw_reveal(gfx::Canvas& c) const;
    void draw_grab(gfx::Canvas& c) const;
    void draw_banter(gfx::Canvas& c) const;
    void draw_tray(gfx::Canvas& c) const;
    void draw_scratch(gfx::Canvas& c) const;
    void draw_builder(gfx::Canvas& c) const;
    // A person from parts, 24x32 (times scale) with its top-left at x, y.
    void draw_person(gfx::Canvas& c, const content::Look& look, int x, int y, int frame, int scale = 1) const;
    const content::Look& look_of(int seat) const;
    void draw_results(gfx::Canvas& c) const;
    void draw_season_over(gfx::Canvas& c) const;
    void draw_cut_off(gfx::Canvas& c) const;
    void draw_initials(gfx::Canvas& c) const;
    void draw_high_scores(gfx::Canvas& c) const;
    void update_lights(float dt);
    void draw_strip(gfx::Canvas& c, int side) const;  // string lights; side 0 = left, 1 = right

    State state_ = State::Title;
    float state_time_ = 0.0f;  // seconds since entering the current state

    Rng rng_;
    Buzz buzz_;
    Party party_;

    // Seating for the current party.
    int seat_count_ = 0;
    Point seats_[Party::MAX_SEATS] = {};
    int guest_of_seat_[Party::MAX_SEATS] = {};  // index into content::GUESTS (unused for seat 0)
    GiftCategory favorite_[Party::MAX_SEATS] = {};
    int cursor_ = TREE;

    Phase phase_ = Phase::Thinking;
    float phase_time_ = 0.0f;
    float think_seconds_ = 0.0f;
    int reveal_gift_ = -1;
    int reveal_seat_ = -1;

    // Grab mini-game
    int grab_gift_ = -1;
    float grab_pos_ = 0.0f;  // marker position, 0..1
    float grab_dir_ = 1.0f;
    float grab_center_ = 0.5f;

    float drink_cooldown_ = 0.0f;

    // Drink tray
    static constexpr int DRINK_COUNT = sizeof(tuning::DRINKS) / sizeof(tuning::DRINKS[0]);
    bool tray_open_ = false;
    float tray_time_ = 0.0f;
    int tray_[tuning::TRAY_SLOTS] = {};  // index into tuning::DRINKS per slot
    float hand_pos_ = 0.0f;              // 0..1 across the tray
    float hand_dir_ = 1.0f;
    int warn_tick_ = -1;

    // Needle positions on the gauge HUD, easing toward the real readings.
    float needle_buzz_ = 0.0f;
    float needle_vibe_ = 0.0f;

    // Hiding your gift
    bool hide_held_ = false;
    float suspicion_ = 0.0f;  // seconds of hiding, drains when you let go

    // Vibe and banter
    float vibe_ = 0.0f;
    bool banter_active_ = false;
    int banter_seat_ = -1;
    const char* banter_line_ = "";
    float banter_time_ = 0.0f;   // seconds since the current prompt appeared
    float banter_timer_ = 0.0f;  // seconds until the next prompt

    // Your character, and where you are on the builder screen.
    content::Look look_ = content::DEFAULT_LOOK;
    int builder_row_ = ROW_DONE;

    // Lottery scratcher mini-game
    ScratchTicket ticket_;
    int scratch_cursor_ = 0;
    bool scratch_won_ = false;
    float scratch_won_at_ = 0.0f;  // state_time_ when the pair turned up
    float scratch_noise_ = 0.0f;   // timer for the scratching sound

    // Scoring
    int party_index_ = 0;
    float party_score_ = 0.0f;  // sweet-spot points this party
    float charm_score_ = 0.0f;  // charming-vibe points this party
    int steals_ = 0;
    int scenes_ = 0;
    int run_score_ = 0;         // points banked from finished parties
    Results results_;
    const char* cut_off_line_ = "";

    // High scores, best first. Saved with your look.
    content::HighScore scores_[tuning::HIGH_SCORE_COUNT] = {};
    int score_count_ = 0;
    char initials_[4] = "AAA";  // remembered from your last entry
    int initials_slot_ = 0;     // letter being edited
    int new_rank_ = -1;         // your new entry, highlighted on the board
    float reset_hold_ = 0.0f;   // seconds X + Y held on the title screen
    bool scores_were_reset_ = false;

    // Idle screen: the snowy tree, then sleep.
    float idle_time_ = 0.0f;              // seconds with no buttons on the title or builder
    State idle_return_ = State::Title;    // where a button press goes back to
    SnowScene snow_;

    // String lights in the side strips.
    float lights_clock_ = 0.0f;  // runs faster the drunker you are
    float lights_flash_ = 0.0f;  // seconds left of the gold chase after your steal

    char ticker_[96] = {};
    float ticker_age_ = 0.0f;

    // Art from assets/ (nullptr = draw the built-in shapes instead).
    const Sprite* spr_room_ = nullptr;
    const Sprite* spr_tree_ = nullptr;
    const Sprite* spr_gift_wrapped_ = nullptr;
    const Sprite* spr_gift_open_ = nullptr;
    const Sprite* spr_couch_ = nullptr;
    const Sprite* spr_person_[Party::MAX_SEATS] = {};  // full-sprite overrides, per seat (guests only)
    const Sprite* spr_body_ = nullptr;                 // people parts
    const Sprite* spr_hair_[HAIR_COUNT] = {};
    const Sprite* spr_sweater_[SWEATER_COUNT] = {};
    const Sprite* spr_hat_[HAT_COUNT] = {};
    const Sprite* spr_drink_[DRINK_COUNT] = {};        // per drink, on the tray
    const Sprite* spr_hand_ = nullptr;
};
