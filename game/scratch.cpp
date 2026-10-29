#include "game/scratch.h"

#include "game/tuning.h"

namespace {

constexpr int PRIZE_COUNT = sizeof(tuning::SCRATCH_PRIZES) / sizeof(tuning::SCRATCH_PRIZES[0]);
static_assert(PRIZE_COUNT >= ScratchTicket::SPOTS - 1,
              "SCRATCH_PRIZES needs at least 5 prizes: 1 pair + 4 different near-misses");

}  // namespace

void ScratchTicket::generate(Rng& rng) {
    // Pick the prize by rarity.
    int total = 0;
    for (const tuning::ScratchPrize& p : tuning::SCRATCH_PRIZES) total += p.weight;
    int pick = rng.below(total);
    int winner = 0;
    for (int i = 0; i < PRIZE_COUNT; ++i) {
        if (pick < tuning::SCRATCH_PRIZES[i].weight) {
            winner = i;
            break;
        }
        pick -= tuning::SCRATCH_PRIZES[i].weight;
    }
    prize = tuning::SCRATCH_PRIZES[winner].value;

    // Near-misses: four other prizes, each shown once, so there's only one pair.
    int others[PRIZE_COUNT];
    int other_count = 0;
    for (int i = 0; i < PRIZE_COUNT; ++i) {
        if (i != winner) others[other_count++] = i;
    }
    for (int i = other_count - 1; i > 0; --i) {
        int j = rng.below(i + 1);
        int tmp = others[i];
        others[i] = others[j];
        others[j] = tmp;
    }

    values[0] = prize;
    values[1] = prize;
    for (int i = 2; i < SPOTS; ++i) values[i] = tuning::SCRATCH_PRIZES[others[i - 2]].value;
    for (int i = SPOTS - 1; i > 0; --i) {
        int j = rng.below(i + 1);
        int tmp = values[i];
        values[i] = values[j];
        values[j] = tmp;
    }

    for (float& s : scratched) s = 0.0f;
}

bool ScratchTicket::won() const {
    int found = 0;
    for (int i = 0; i < SPOTS; ++i) {
        if (revealed(i) && values[i] == prize) ++found;
    }
    return found >= 2;
}

float scratch_expected_value() {
    float sum = 0.0f;
    int total = 0;
    for (const tuning::ScratchPrize& p : tuning::SCRATCH_PRIZES) {
        sum += static_cast<float>(p.value) * p.weight;
        total += p.weight;
    }
    return sum / total;
}
