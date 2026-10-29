#pragma once

#include "game/party.h"
#include "game/rng.h"
#include "game/types.h"

// Everything an AI guest considers when it's their move.
struct AiProfile {
    Personality personality;
    GiftCategory favorite;     // the Collector's obsession this party
    int party_index;           // 0 = first party; guests get greedier later
    float player_target_bonus; // extra desire for the human's gift (charm / mess)
    int player_seat;
};

// The current actor's move: a gift to steal, or -1 to open one
// (or to keep theirs, during the final swap).
int ai_choose(const Party& party, const AiProfile& profile, Rng& rng);

// Average value of a gift from the whole list -- what a guest expects to get
// by opening one.
float expected_gift_value();

// True while a gift's value is a secret (Wildcards, until the party ends).
bool value_hidden(const Party& party, int g);
