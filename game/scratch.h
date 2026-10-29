#pragma once

#include "game/rng.h"

// A lottery scratch ticket: 6 spots, exactly one matching pair. The pair's
// value is the prize, picked from tuning::SCRATCH_PRIZES by rarity.
struct ScratchTicket {
    static constexpr int SPOTS = 6;
    static constexpr int COLUMNS = 3;

    int values[SPOTS] = {};
    float scratched[SPOTS] = {};  // 0 = covered, 1 = fully scratched off
    int prize = 0;

    void generate(Rng& rng);
    bool revealed(int spot) const { return scratched[spot] >= 1.0f; }
    bool won() const;  // both prize spots are uncovered
};

// Average payout of a ticket, from the prize table -- what guests think the
// scratchers are worth before anyone scratches them.
float scratch_expected_value();
