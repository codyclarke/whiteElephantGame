#pragma once

// Every gameplay number lives here. Change a value, rebuild, play.
// Buzz is measured on a 0-100 scale.
namespace tuning {

// --- Buzz meter ------------------------------------------------------------
inline constexpr float SWEET_SPOT_LOW   = 30.0f;  // below this you're "sober"
inline constexpr float SWEET_SPOT_HIGH  = 65.0f;  // above this you're "drunk"
inline constexpr float BLACKOUT         = 90.0f;  // the danger line
inline constexpr float BLACKOUT_SECONDS = 3.0f;   // time above the line before you're cut off
inline constexpr float BUZZ_DECAY_PER_SECOND = 2.5f;  // how fast buzz wears off

// --- Drinks ----------------------------------------------------------------
// A drink doesn't hit right away: its buzz is "pending" and soaks in over
// absorb_seconds. Each drink rolls a random strength between min and max,
// so you never know exactly how hard one will hit (mystery punch is wildest).
// A negative strength sobers you up.
//
// The two weights are how often a drink lands on the tray: weight_sober when
// you're at 0 buzz, weight_drunk at the blackout line, and a blend between.
// The first sobering drink in the list (water) is always on the tray at
// least once, no matter how drunk you are.
//
// The tray shows no names, only what each drink looks like. Drinks with the
// same look can't be told apart -- good wine and bad wine are both "wine",
// so grabbing one is a gamble. The look is also the art file:
// assets/drinks/<look>.png.
struct Drink {
    const char* name;  // revealed once you've grabbed it
    const char* look;
    float min_buzz;
    float max_buzz;
    float absorb_seconds;
    int weight_sober;
    int weight_drunk;
};

inline constexpr Drink DRINKS[] = {
    // name            look      min    max   absorb sober drunk
    {"Light Beer",    "beer",    5.0f, 12.0f, 5.0f,  4,    2},
    {"Good Wine",     "good_wine",   10.0f, 22.0f, 4.0f,  3,    2},
    {"Bad Wine",      "bad_wine",   10.0f, 35.0f, 3.0f,  4,    5},
    {"Eggnog",        "nog",     8.0f, 20.0f, 6.0f,  1,    1},
    {"Shot",          "shot",   18.0f, 34.0f, 3.0f,  2,    4},
    {"Mystery Punch", "punch",  10.0f, 40.0f, 5.0f,  2,    3},
    {"Water",         "water", -14.0f, -6.0f, 4.0f,  3,    0},
};

inline constexpr float DRINK_COOLDOWN_SECONDS = 1.5f;  // before you can call the tray again

// --- Drink tray (press D) --------------------------------------------------
// Your hand sweeps across the tray; press D to grab what it's over. Wait too
// long and you grab whatever's under it. Sober or in the sweet spot, the
// tray is sorted weakest to strongest; drunk, it's shuffled.
inline constexpr int   TRAY_SLOTS = 5;
inline constexpr float TRAY_SECONDS = 3.0f;            // then you grab whatever's under your hand
inline constexpr float TRAY_SWEEPS_PER_SECOND = 0.8f;  // hand speed (one sweep = across the tray)
inline constexpr float TRAY_DRUNK_SPEEDUP = 1.0f;      // at the blackout line the hand moves 2x
inline constexpr float TRAY_DRUNK_LURCH = 0.6f;        // and its speed lurches this much

// --- Hiding your gift (hold X) ---------------------------------------------
// Holding X builds "suspicion". Below the line you're sneaky and guests skip
// your gift. Past it, everyone can tell: your gift draws steals (even after you
// let go, until suspicion cools) and you come off as rude.
inline constexpr float HIDE_SNEAKY_BONUS = -20.0f;     // desire for your gift while sneakily hidden
inline constexpr float HIDE_OBVIOUS_SECONDS = 3.0f;    // suspicion past this = caught
inline constexpr float HIDE_OBVIOUS_BONUS = 15.0f;     // desire for your gift while caught
inline constexpr float HIDE_COOL_PER_SECOND = 0.5f;    // suspicion drains this fast after you let go
inline constexpr float HIDE_MAX_SUSPICION = 6.0f;      // suspicion stops building here
inline constexpr float HIDE_RUDE_FLOP = 8.0f;          // vibe rude flop the moment you're caught
inline constexpr float HIDE_RUDE_PER_SECOND = 4.0f;    // more rude flop each second you keep it up

// --- Drunk effects ---------------------------------------------------------
inline constexpr float MAX_SWAY_PIXELS = 5.0f;           // screen wobble at the blackout line
inline constexpr float CURSOR_DRIFT_PER_SECOND = 0.35f;  // chance/sec the cursor wanders at full drunk

// --- Parties ---------------------------------------------------------------
inline constexpr int   PARTIES_PER_SEASON = 3;
inline constexpr int   FIRST_PARTY_SEATS = 4;               // you + 3 guests; +1 guest each party
inline constexpr float DRINK_STRENGTH_PER_PARTY = 0.15f;    // party 2 drinks are 15% stronger, etc.
inline constexpr int   STEALS_TO_FREEZE = 3;                // a gift locks after this many steals

// --- Pacing ----------------------------------------------------------------
inline constexpr float AI_THINK_SECONDS = 2.0f;     // how long a guest "thinks" before acting
inline constexpr float AI_THINK_JITTER = 1.0f;      // + up to this much at random  
inline constexpr float REVEAL_SECONDS = 2.8f;       // how long an unwrapped gift is shown (A skips)
inline constexpr float PLAYER_TURN_SECONDS = 15.0f; // dither too long and you grab one off the pile

// --- AI guests -------------------------------------------------------------
// A guest steals when the best gift beats an average wrapped gift by more
// than their margin. Lower margin = greedier.
inline constexpr float AI_MARGIN_STEALER   = 0.0f;
inline constexpr float AI_MARGIN_OPENER    = 20.0f;
inline constexpr float AI_MARGIN_GRUDGE    = 8.0f;
inline constexpr float AI_MARGIN_COLLECTOR = 12.0f;
inline constexpr float AI_MARGIN_DROP_PER_PARTY = 4.0f;  // everyone gets greedier as the season goes on
inline constexpr float AI_OPENER_STEAL_CHANCE = 0.4f;    // even when tempted, the New Hire often chickens out
inline constexpr float AI_LUSH_STEAL_CHANCE = 0.5f;      // Sales Guy flips a coin, then grabs anything
inline constexpr float AI_GRUDGE_BONUS = 25.0f;          // HR really wants it back from whoever took theirs
inline constexpr float AI_FAVORITE_BONUS = 25.0f;        // the Intern's favorite category this party
inline constexpr float AI_FINAL_SWAP_MARGIN = 5.0f;      // final swap only if it's this much better

// How much guests want YOUR gift, added to how much they want it anyway.
inline constexpr float CHARM_TARGET_BONUS = -8.0f;       // vibe is Charming: they leave you be
inline constexpr float OBNOXIOUS_TARGET_BONUS = 10.0f;   // vibe is Obnoxious: payback
inline constexpr float DRUNK_TARGET_BONUS = 8.0f;        // you're a mess: easy mark
inline constexpr float SCENE_TARGET_BONUS = 5.0f;        // per scene you've made this party

// --- Vibe meter --------------------------------------------------------------
// 0 = wallflower, 100 = obnoxious. Charming is the middle band.
// Good outcomes pull you toward the center. Bad outcomes ("flops") never help:
// you keep how far off-center you were, land on the side that matches what you
// did (rude or shy), and get pushed further by the flop amount. A flopped
// roast while shy swings you to the rude end.
// Keep WALLFLOWER and OBNOXIOUS the same distance from CENTER so a swing
// lands you in the matching zone on the other side.
inline constexpr float VIBE_START = 40.0f;
inline constexpr float VIBE_WALLFLOWER = 30.0f;     // below this you're a wallflower
inline constexpr float VIBE_OBNOXIOUS = 70.0f;      // above this you're obnoxious
inline constexpr float VIBE_CENTER = 50.0f;         // where good outcomes pull you
inline constexpr float VIBE_SOBER_DRIFT = 1.2f;     // per second toward wallflower while sober
inline constexpr float VIBE_SWEET_PULL = 1.0f;      // per second toward center in the buzz sweet spot
inline constexpr float VIBE_DRUNK_DRIFT = 2.0f;     // per second toward obnoxious while drunk (scales up)
inline constexpr float VIBE_STEAL_PUSH = 8.0f;      // every steal is a rude flop
inline constexpr float VIBE_STEAL_NEW_HIRE = 6.0f;  // extra for robbing the New Hire
inline constexpr float VIBE_SCENE_PUSH = 15.0f;     // rude flop
inline constexpr float WALLFLOWER_TURN_FACTOR = 0.6f;  // your turn timer shrinks to 60%
inline constexpr float GRAB_CHARM_FACTOR = 1.2f;       // steal zone 20% wider when charming

// --- Banter ------------------------------------------------------------------
inline constexpr float BANTER_EVERY_MIN = 5.0f;   // seconds between banter prompts...
inline constexpr float BANTER_EVERY_MAX = 9.0f;   // ...picked at random in this range
inline constexpr float BANTER_SECONDS = 2.5f;     // time to answer
inline constexpr float BANTER_LAUGH_ODDS = 0.9f;
inline constexpr float BANTER_PULL_LAUGH = 6.0f;     // pull toward the middle when it lands
inline constexpr float BANTER_PULL_FLATTER = 10.0f;
inline constexpr float BANTER_PULL_ROAST = 18.0f;
inline constexpr float BANTER_LAUGH_FLOP = 4.0f;     // shy flop
inline constexpr float BANTER_FLATTER_FLOP = 8.0f;   // shy flop (awkward)
inline constexpr float BANTER_ROAST_FLOP = 16.0f;    // rude flop
inline constexpr float BANTER_IGNORE = 6.0f;         // shy flop
inline constexpr float BANTER_TIMEOUT = 4.0f;        // shy flop

inline constexpr float BANTER_SWEET_BONUS = 0.15f;   // added to the odds in the buzz sweet spot
inline constexpr float BANTER_SOBER_PENALTY = 0.10f; // subtracted while sober
inline constexpr float BANTER_DRUNK_PENALTY = 0.05f; // subtracted while drunk
inline constexpr float BANTER_GARBLE = 0.6f;         // chance at full drunk your answer comes out as a roast

// Odds a flatter or roast lands, by personality (same order as Personality in types.h).
struct BanterOdds {
    float flatter;
    float roast;
};
inline constexpr BanterOdds BANTER_ODDS[] = {
    {0.85f, 0.35f},  // Stealer (Manager): loves flattery
    {0.90f, 0.20f},  // Opener (New Hire): fragile
    {0.50f, 0.90f},  // Lush (Sales Guy): can take it, dishes it out
    {0.60f, 0.10f},  // Grudge (HR Rep): will document it
    {0.60f, 0.70f},  // Collector (Intern): just happy to be here
};

// --- Steal grab mini-game --------------------------------------------------
// A marker sweeps a bar; press A while it's inside the zone. Sizes are a
// fraction of the bar.
inline constexpr float GRAB_SWEEPS_PER_SECOND = 1.0f;
inline constexpr float GRAB_SECONDS = 4.0f;          // miss if you never press
inline constexpr float GRAB_ZONE_SOBER = 0.14f;      // awkward hesitation
inline constexpr float GRAB_ZONE_SWEET = 0.30f;      // smooth operator
inline constexpr float GRAB_ZONE_DRUNK = 0.18f;
inline constexpr float GRAB_ZONE_BLACKOUT = 0.10f;
inline constexpr float GRAB_SOBER_SPEEDUP = 1.4f;    // nerves: the marker moves faster when sober
inline constexpr float GRAB_DRUNK_WOBBLE = 0.7f;     // marker speed lurches when drunk
inline constexpr float GRAB_DRUNK_JITTER = 0.12f;    // the zone itself drifts when drunk

// --- Lottery scratchers ----------------------------------------------------
// End a party holding the scratch tickets and you scratch one to find out what
// it's worth. Every ticket wins: it has exactly one matching pair, and that's
// the prize. The other spots show other prizes from this table as near-misses,
// so keep at least 5 prizes, each with a different value.
// weight = how often that prize comes up, relative to the others.
struct ScratchPrize {
    int value;
    int weight;
};
inline constexpr ScratchPrize SCRATCH_PRIZES[] = {
    // value  weight
    {  2,     30},
    {  5,     25},
    { 10,     18},
    { 20,     12},
    { 40,      8},
    { 75,      4},
    {150,      2},
    {500,      1},  // jackpot
};
inline constexpr float SCRATCH_SECONDS = 0.5f;        // holding A to clear one spot
inline constexpr float SCRATCH_DRUNK_SLIP = 1.5f;     // chance/sec at full drunk your hand slips to another spot mid-scratch

// --- HUD needle gauges -----------------------------------------------------
inline constexpr float NEEDLE_SPEED = 8.0f;           // how quickly needles swing to their reading
inline constexpr float NEEDLE_DRUNK_TREMBLE = 6.0f;   // degrees of buzz-needle shake at full drunk

// --- Sound -----------------------------------------------------------------
inline constexpr float MUSIC_VOLUME = 0.45f;  // 0..1; music sits under the effects
inline constexpr float SFX_VOLUME = 0.8f;     // 0..1; WAV sound effects
inline constexpr float BEEP_VOLUME = 0.08f;   // 0..1; fallback beeps for events with no WAV yet

// --- Scoring ---------------------------------------------------------------
inline constexpr float SWEET_SPOT_POINTS_PER_SECOND = 0.5f;
inline constexpr float CHARM_POINTS_PER_SECOND = 0.5f;  // while your vibe is Charming
inline constexpr int   STEAL_BONUS = 10;           // per successful steal
inline constexpr int   FROZEN_COVETED_BONUS = 15;  // leave holding a frozen Coveted gift
inline constexpr int   SCENE_PENALTY = 10;         // per fumbled steal

// --- High scores -------------------------------------------------------------
inline constexpr int   HIGH_SCORE_COUNT = 10;            // entries on the board
inline constexpr float TITLE_SCORES_EVERY_SECONDS = 8.0f; // title and board take turns
// Hold X + Y on the title screen this long to reset the board to
// DEFAULT_HIGH_SCORES (content.h). 0 turns the combo off, e.g. for the party.
inline constexpr float SCORE_RESET_HOLD_SECONDS = 3.0f;

// --- Idle screen (snowy tree) and sleep --------------------------------------
inline constexpr float IDLE_AFTER_SECONDS = 30.0f;     // no buttons on the title or builder
inline constexpr float IDLE_SECONDS = 120.0f;          // then the screen turns off
inline constexpr float SNOW_FLAKES_PER_SECOND = 30.0f; // at the heaviest snowfall
inline constexpr float SNOW_GUST_MAX = 40.0f;          // strongest gust, pixels per second

}  // namespace tuning
