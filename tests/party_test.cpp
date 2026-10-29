// Plays thousands of AI-only and random parties and checks the white
// elephant rules hold every time. Run: build/Release/party_test.exe

#include <cstdio>

#include "game/ai.h"
#include "game/content.h"
#include "game/party.h"
#include "game/tuning.h"

namespace {

int failures = 0;
unsigned current_seed = 0;

#define CHECK(cond)                                                                        \
    do {                                                                                   \
        if (!(cond)) {                                                                     \
            if (failures < 20) {                                                           \
                std::printf("FAIL line %d (seed %u): %s\n", __LINE__, current_seed, #cond); \
            }                                                                              \
            ++failures;                                                                    \
        }                                                                                  \
    } while (0)

constexpr int GIFT_COUNT = sizeof(content::GIFTS) / sizeof(content::GIFTS[0]);

struct Stats {
    long parties = 0;
    long steals = 0;
    long final_swaps = 0;
};

// Plays one party. use_ai = real AI decisions; otherwise random legal moves.
void play_party(Rng& rng, bool use_ai, Stats& stats) {
    int seats = 1 + rng.below(Party::MAX_SEATS);  // 1..8
    int ids[Party::MAX_SEATS];
    int values[Party::MAX_SEATS];
    for (int g = 0; g < seats; ++g) {
        ids[g] = rng.below(GIFT_COUNT);
        values[g] = rng.below(80) - 15;
    }

    Party p;
    p.start(seats, ids, values, rng);
    CHECK(!p.over());
    CHECK(p.wrapped_count() == seats);

    int opens = 0;
    int actions = 0;
    while (!p.over() && actions < 1000) {
        ++actions;
        int actor = p.actor();
        CHECK(actor >= 0 && actor < seats);
        if (p.final_swap()) {
            CHECK(actor == p.first_seat());
            CHECK(p.wrapped_count() == 0);
        } else {
            CHECK(p.gift_of(actor) < 0);  // you only act when empty-handed
            CHECK(p.wrapped_count() > 0); // so there's always something to open
        }

        int g;
        if (use_ai) {
            AiProfile profile{static_cast<Personality>(rng.below(5)), static_cast<GiftCategory>(rng.below(4)),
                              rng.below(3), rng.range(-8.0f, 13.0f), 0};
            g = ai_choose(p, profile, rng);
        } else {
            g = -1;
            if (rng.chance(0.6f)) {
                int start = rng.below(seats);
                for (int i = 0; i < seats; ++i) {
                    int cand = (start + i) % seats;
                    if (p.can_steal(cand)) {
                        g = cand;
                        break;
                    }
                }
            }
        }

        if (g >= 0) {
            CHECK(p.can_steal(g));
            CHECK(g != p.forbidden());
            CHECK(!p.frozen(g));
            int victim = p.gift(g).holder;
            int steals_before = p.gift(g).steals;
            bool final_swap = p.final_swap();
            p.steal(g);
            ++stats.steals;
            CHECK(p.gift(g).steals == steals_before + 1);
            CHECK(p.gift(g).steals <= tuning::STEALS_TO_FREEZE);
            CHECK(p.gift(g).holder == actor);
            if (final_swap) {
                ++stats.final_swaps;
                CHECK(p.over());
                CHECK(p.gift_of(victim) >= 0);  // the victim got the swapper's gift
            } else {
                CHECK(p.actor() == victim);   // robbed player goes next
                CHECK(p.forbidden() == g);    // and can't take it straight back
                CHECK(!p.can_steal(g));
            }
        } else if (p.final_swap()) {
            p.keep();
            CHECK(p.over());
        } else {
            int w = p.pick_wrapped(rng);
            CHECK(w >= 0 && p.gift(w).holder < 0);
            p.open(w);
            ++opens;
            CHECK(p.gift(w).holder == actor);
        }
    }

    CHECK(p.over());
    CHECK(p.actor() == -1);
    CHECK(opens == seats);
    CHECK(p.wrapped_count() == 0);
    for (int s = 0; s < seats; ++s) {
        int held = 0;
        for (int g = 0; g < seats; ++g) {
            if (p.gift(g).holder == s) ++held;
        }
        CHECK(held == 1);  // everyone goes home with exactly one gift
    }
    ++stats.parties;
}

}  // namespace

int main() {
    Stats ai_stats;
    Stats random_stats;
    for (unsigned seed = 1; seed <= 20000; ++seed) {
        current_seed = seed;
        Rng rng(seed);
        play_party(rng, true, ai_stats);
        play_party(rng, false, random_stats);
    }

    std::printf("AI parties:     %ld, %.2f steals per party, %ld final swaps\n", ai_stats.parties,
                static_cast<double>(ai_stats.steals) / ai_stats.parties, ai_stats.final_swaps);
    std::printf("Random parties: %ld, %.2f steals per party, %ld final swaps\n", random_stats.parties,
                static_cast<double>(random_stats.steals) / random_stats.parties, random_stats.final_swaps);
    if (failures) {
        std::printf("%d check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("All rule checks passed.\n");
    return 0;
}
