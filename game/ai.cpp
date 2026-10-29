#include "game/ai.h"

#include "game/content.h"
#include "game/scratch.h"
#include "game/tuning.h"

namespace {

float steal_margin(Personality p) {
    switch (p) {
        case Personality::Stealer:   return tuning::AI_MARGIN_STEALER;
        case Personality::Opener:    return tuning::AI_MARGIN_OPENER;
        case Personality::Grudge:    return tuning::AI_MARGIN_GRUDGE;
        case Personality::Collector: return tuning::AI_MARGIN_COLLECTOR;
        case Personality::Lush:      return 0.0f;  // doesn't think it through
    }
    return 0.0f;
}

// What a guest thinks a gift is worth. Hidden values are a guess.
float perceived_value(const Party& party, int g) {
    if (content::GIFTS[party.gift(g).content_id].scratch_ticket) return scratch_expected_value();
    if (value_hidden(party, g)) return expected_gift_value();
    return static_cast<float>(party.gift(g).value);
}

}  // namespace

float expected_gift_value() {
    static const float average = [] {
        float sum = 0.0f;
        int n = 0;
        for (const content::Gift& g : content::GIFTS) {
            sum += g.scratch_ticket ? scratch_expected_value() : (g.min_value + g.max_value) * 0.5f;
            ++n;
        }
        return sum / n;
    }();
    return average;
}

bool value_hidden(const Party& party, int g) {
    return !party.over() && content::GIFTS[party.gift(g).content_id].category == GiftCategory::Wildcard;
}

int ai_choose(const Party& party, const AiProfile& profile, Rng& rng) {
    int me = party.actor();
    if (me < 0) return -1;

    // Score every gift we're allowed to take.
    int stealable[Party::MAX_SEATS];
    int count = 0;
    int best = -1;
    float best_score = 0.0f;
    for (int g = 0; g < party.gift_count(); ++g) {
        if (!party.can_steal(g)) continue;
        stealable[count++] = g;

        const Party::Gift& gift = party.gift(g);
        float score = perceived_value(party, g) + rng.range(0.0f, 3.0f);
        if (gift.holder == profile.player_seat) score += profile.player_target_bonus;
        if (profile.personality == Personality::Grudge && gift.holder == party.stolen_from_by(me)) {
            score += tuning::AI_GRUDGE_BONUS;
        }
        if (profile.personality == Personality::Collector &&
            content::GIFTS[gift.content_id].category == profile.favorite) {
            score += tuning::AI_FAVORITE_BONUS;
        }
        if (best < 0 || score > best_score) {
            best = g;
            best_score = score;
        }
    }
    if (count == 0) return -1;

    if (profile.personality == Personality::Lush) {
        return rng.chance(tuning::AI_LUSH_STEAL_CHANCE) ? stealable[rng.below(count)] : -1;
    }
    if (profile.personality == Personality::Opener && !rng.chance(tuning::AI_OPENER_STEAL_CHANCE)) {
        return -1;
    }

    // Compare against what we'd get otherwise: a random wrapped gift, or
    // (during the final swap) the gift already in hand.
    float baseline = party.final_swap()
                         ? perceived_value(party, party.gift_of(me)) + tuning::AI_FINAL_SWAP_MARGIN
                         : expected_gift_value();
    float margin = steal_margin(profile.personality) - tuning::AI_MARGIN_DROP_PER_PARTY * profile.party_index;
    return best_score - baseline > margin ? best : -1;
}
