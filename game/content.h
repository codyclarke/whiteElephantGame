#pragma once

#include <cstdint>

#include "game/types.h"

// All the jokey text in the game. No code in here -- edit freely.
// Keep ticker lines under ~38 characters so they fit without scrolling
// (longer ones scroll, which is fine but slower to read).
namespace content {

inline constexpr const char* TITLE_TOP    = "YULE";
inline constexpr const char* TITLE_BOTTOM = "MASTER";
inline constexpr const char* SUBTITLE     = "A White Elephant Party";

// --- High scores ------------------------------------------------------------
// The board a fresh handheld starts with, and what resetting the high scores
// puts back (see README). Any order: the game sorts it. Initials are up to 3
// capital letters. For a blank board, leave a single {"", 0} line.
// The board holds HIGH_SCORE_COUNT entries (tuning.h); extras are dropped.
struct HighScore {
    char initials[4];
    int score;
};

inline constexpr HighScore DEFAULT_HIGH_SCORES[] = {
    {"SNT", 300},
    {"ELF", 240},
    {"RDF", 180},
    {"FRT", 120},
    {"BAD", 1},
};

// --- People: built from parts ------------------------------------------------
// Everyone (you and the guests) is a body + sweater + hair + hat, each drawn
// in four grays and recolored with a ramp. Art lives in assets/people/.
// Each list below must stay in the same order as its enum.

// A color ramp: highlight, light, mid, shadow, as 0xRRGGBB.
struct Ramp {
    const char* name;
    uint32_t shade[4];
};

enum SkinTone { SKIN_PORCELAIN, SKIN_PEACH, SKIN_TAN, SKIN_DEEP };
inline constexpr Ramp SKIN_TONES[] = {
    {"Porcelain", {0xFFF0E6, 0xF2D3BE, 0xD9AE95, 0xA87E68}},
    {"Peach",     {0xE6B8A2, 0xC59782, 0x9E7563, 0x74513F}},
    {"Tan",       {0xD9A877, 0xB88050, 0x8F5F38, 0x6A4226}},
    {"Deep",      {0xA86E4C, 0x7E4E33, 0x5C3622, 0x3E2416}},
};

// Bald has no art: you see the body's head.
enum HairStyle { HAIR_CLASSIC, HAIR_LONG, HAIR_LONG_WAVY, HAIR_CURLY, HAIR_BUN, HAIR_SPIKY, HAIR_BALD, HAIR_MOHAWK };
inline constexpr const char* HAIR_STYLES[] = {"Classic", "Long", "Long Wavy", "Curly", "Bun", "Spiky", "Bald", "Mohawk"};

enum HairColor { HAIR_GINGER, HAIR_BLONDE, HAIR_BROWN, HAIR_BLACK };
inline constexpr Ramp HAIR_COLORS[] = {
    {"Ginger", {0xE07A35, 0xAD4E1A, 0x84380F, 0x5A2408}},
    {"Blonde", {0xFFF0A0, 0xE8C860, 0xC09A38, 0x8A6A20}},
    {"Brown",  {0xA87850, 0x7A5232, 0x56361E, 0x3A2212}},
    {"Black",  {0x6A6A80, 0x3A3A48, 0x24242E, 0x141418}},
};

enum SweaterStyle { SWEATER_PLAIN, SWEATER_STRIPES, SWEATER_SNOWFLAKE, SWEATER_REINDEER, SWEATER_TREE };
inline constexpr const char* SWEATERS[] = {"Plain", "Stripes", "Snowflake", "Reindeer", "Tree"};

enum KnitColor { KNIT_GREEN, KNIT_RED, KNIT_NAVY, KNIT_CREAM, KNIT_PURPLE };
inline constexpr Ramp SWEATER_COLORS[] = {
    {"Green",  {0x5FA020, 0x376D03, 0x245000, 0x143200}},
    {"Red",    {0xE04848, 0xB02020, 0x801010, 0x500808}},
    {"Navy",   {0x5070C0, 0x30489A, 0x1E3070, 0x101C48}},
    {"Cream",  {0xFFF8E8, 0xE8DCC0, 0xC0B090, 0x908060}},
    {"Purple", {0xA070D0, 0x7040A8, 0x502C80, 0x301A50}},
};

// Hats share the sweater's color (so a beanie matches); fixed colors stay put.
// tucks_hair = true swaps the hair to its "under a hat" version: the second
// frame of the hair PNG (e.g. hair/spiky_f2.png). Hair with only one frame
// just uses its main version.
struct HatStyle {
    const char* name;
    bool tucks_hair;
};

enum Hat { HAT_NONE, HAT_SANTA, HAT_ANTLERS, HAT_BOW, HAT_BEANIE };
inline constexpr HatStyle HATS[] = {
    {"None",      false},
    {"Santa Hat", true},
    {"Antlers",   false},
    {"Bow",       false},
    {"Beanie",    true},
};

struct Look {
    int skin;
    int hair;
    int hair_color;
    int sweater;
    int sweater_color;
    int hat;
};

// What the character builder starts on before you've saved a look.
inline constexpr Look DEFAULT_LOOK = {SKIN_PEACH, HAIR_CLASSIC, HAIR_GINGER, SWEATER_PLAIN, KNIT_GREEN, HAT_NONE};

// --- Guests -------------------------------------------------------------------

struct Guest {
    const char* label;        // name under their seat (max ~9 characters)
    Personality personality;  // how they play
    const char* chatter[3];   // banter openers: what they say to you
    Look look;                // drawn from parts; a full PNG at
                              // assets/guests/<label>_f4.png overrides it
};

// Office archetypes (see DESIGN.md). Real party guests go here later.
inline constexpr Guest GUESTS[] = {
    {"Manager",   Personality::Stealer,
                  {"Let's take this offline.",
                   "Circling back to the gifts.",
                   "Love the energy, team."},
                  {SKIN_PORCELAIN, HAIR_CLASSIC, HAIR_BROWN, SWEATER_PLAIN, KNIT_NAVY, HAT_NONE}},
    {"New Hire",  Personality::Opener,
                  {"Is it okay if I sit here?",
                   "Sorry. Sorry! Sorry.",
                   "Do we... bring our own?"},
                  {SKIN_TAN, HAIR_CURLY, HAIR_BLACK, SWEATER_SNOWFLAKE, KNIT_CREAM, HAT_ANTLERS}},
    {"Sales Guy", Personality::Lush,
                  {"Who wants shots? Anyone?",
                   "Let me tell you about my boat.",
                   "Crushing it tonight."},
                  {SKIN_PEACH, HAIR_SPIKY, HAIR_BLONDE, SWEATER_STRIPES, KNIT_RED, HAT_SANTA}},
    {"HR Rep",    Personality::Grudge,
                  {"Let's keep it appropriate.",
                   "I'm writing that down.",
                   "Hydrate, people."},
                  {SKIN_DEEP, HAIR_BUN, HAIR_BLACK, SWEATER_PLAIN, KNIT_PURPLE, HAT_NONE}},
    {"Intern",    Personality::Collector,
                  {"Is the food free? It's free?",
                   "I'm not getting paid for this.",
                   "Can I take leftovers?"},
                  {SKIN_PORCELAIN, HAIR_LONG, HAIR_GINGER, SWEATER_REINDEER, KNIT_GREEN, HAT_BEANIE}},
};

struct Gift {
    const char* name;      // keep under ~19 characters
    int min_value;         // dollars; negative = worse than nothing
    int max_value;         // same as min unless it's a gamble (Wildcard)
    GiftCategory category; // Coveted gifts draw steals; Wildcard values stay hidden until the end
    const char* caption;   // reveal line; up to ~66 characters (wraps onto two lines)
    bool scratch_ticket = false;  // end the party holding it and you play the scratcher
                                  // mini-game; its value comes from SCRATCH_PRIZES in tuning.h
};

inline constexpr Gift GIFTS[] = {
    // Coveted
    {"$50 Gift Card",       50, 50, GiftCategory::Coveted,   "Finally, a real gift."},
    {"BT Speaker",          40, 40, GiftCategory::Coveted,   "Your playlist, now everyone's."},
    {"Fancy Bourbon",       45, 45, GiftCategory::Coveted,   "Aged longer than your laptop."},
    {"PTO Voucher",         35, 35, GiftCategory::Coveted,   "Forged. Still counts."},
    {"Air Fryer",           40, 40, GiftCategory::Coveted,   "It fries. It changes lives."},
    {"FunkoPop and a Joint",40, 40, GiftCategory::Coveted,   "Limited edition. The figure and the strain!"},
    // Practical
    {"Heated Desk Mat",     20, 20, GiftCategory::Practical, "Warm hands, cold hearts, can't lose."},
    {"Nice Mug",            12, 12, GiftCategory::Practical, "World's Best PeePaw!"},
    {"Good Socks",          15, 15, GiftCategory::Practical, "Good for hardwood sliding"},
    {"Phone Battery Bank",  10, 10, GiftCategory::Practical, "Always forget to bring it"},
    {"House Plant",          14, 14, GiftCategory::Practical, "Dead by February."},
    // Gag
    {"Live Laugh Leverage", -5, -5, GiftCategory::Gag,       "Synergy, but make it decor."},
    {"Used Fidget Spinner", -3, -3, GiftCategory::Gag,       "Still warm. Don't ask."},
    {"Q4 Panic Candle",     -2, -2, GiftCategory::Gag,       "Smells like missed targets."},
    {"Regifted Fruitcake",  -8, -8, GiftCategory::Gag,       "The tag says 'To Dave'."},
    {"Singing Fish",        -4, -4, GiftCategory::Gag,       "It knows one song. Forever."},
    {"Mystery Lanyard",     -1, -1, GiftCategory::Gag,       "Holds nothing. Means nothing."},
    {"Hang In There Poster",-3, -3, GiftCategory::Gag,       "The cat did not hang in there."},
    // Wildcard
    {"Lottery Scratchers",   0, 0,  GiftCategory::Wildcard,  "Could be nothing. Could be...", true},
    {"Mystery Envelope",   -10, 50, GiftCategory::Wildcard,  "Feels like cash. Or a coupon."},
};

// Banter results in the ticker. %s is the guest's name -- keep exactly one.
inline constexpr const char* BANTER_LAUGH_OK     = "%s appreciates the laugh.";
inline constexpr const char* BANTER_LAUGH_FLOP   = "%s: \"That wasn't a joke.\"";
inline constexpr const char* BANTER_FLATTER_OK   = "%s blushes. You're a delight.";
inline constexpr const char* BANTER_FLATTER_FLOP = "%s: \"...Thanks?\" Awkward.";
inline constexpr const char* BANTER_ROAST_OK     = "%s loses it. You're hilarious.";
inline constexpr const char* BANTER_ROAST_FLOP   = "%s is NOT amused.";
inline constexpr const char* BANTER_IGNORE       = "You stare at your drink. %s moves on.";
inline constexpr const char* BANTER_TIMEOUT      = "You zone out. %s gives up.";
inline constexpr const char* BANTER_GARBLE_OK    = "You meant to be nice. %s loved it anyway.";
inline constexpr const char* BANTER_GARBLE_FLOP  = "You meant to be nice. %s is horrified.";

// Hiding your gift (hold X).
inline constexpr const char* HIDE_START      = "You slide your gift behind you.";
inline constexpr const char* HIDE_CAUGHT     = "Everyone sees you sitting on your gift.";
inline constexpr const char* HIDE_COOLED     = "People stop staring at your lap.";
inline constexpr const char* HIDE_NO_GIFT    = "Nothing to hide yet.";
inline constexpr const char* HIDE_HANDS_FULL = "Hands full. You're guarding a gift.";

// Shown under "Sent to the couch." Up to ~110 characters; long lines wrap
// onto as many as 3 lines.
inline constexpr const char* CUT_OFF_LINES[] = {
    "You broke a family heirloom and said \"Did I do that?\"",
    "Your Uber is 2 minutes away.",
    "You puked on the host.",
    "You tried to drink the Christmas tree water.",
};

}  // namespace content
