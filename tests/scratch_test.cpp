// Generates lots of scratch tickets and checks each has exactly one matching
// pair (the prize) and that prizes come up about as often as their weights say.
// Run: build/Release/scratch_test.exe

#include <cmath>
#include <cstdio>

#include "game/scratch.h"
#include "game/tuning.h"

namespace {

int failures = 0;

#define CHECK(cond)                                              \
    do {                                                         \
        if (!(cond)) {                                           \
            if (failures < 20) std::printf("FAIL line %d: %s\n", __LINE__, #cond); \
            ++failures;                                          \
        }                                                        \
    } while (0)

constexpr int PRIZE_COUNT = sizeof(tuning::SCRATCH_PRIZES) / sizeof(tuning::SCRATCH_PRIZES[0]);

}  // namespace

int main() {
    const int TICKETS = 200000;
    int hits[PRIZE_COUNT] = {};
    Rng rng(42);

    for (int t = 0; t < TICKETS; ++t) {
        ScratchTicket ticket;
        ticket.generate(rng);

        // Exactly two spots show the prize; every other value shows once.
        for (int i = 0; i < ScratchTicket::SPOTS; ++i) {
            int same = 0;
            for (int j = 0; j < ScratchTicket::SPOTS; ++j) same += ticket.values[j] == ticket.values[i];
            CHECK(same == (ticket.values[i] == ticket.prize ? 2 : 1));
        }

        // Nothing is uncovered yet, so nothing's won.
        CHECK(!ticket.won());

        // Uncover spots one at a time: it's won exactly when the second prize spot shows.
        int prize_seen = 0;
        for (int i = 0; i < ScratchTicket::SPOTS; ++i) {
            ticket.scratched[i] = 1.0f;
            if (ticket.values[i] == ticket.prize) ++prize_seen;
            CHECK(ticket.won() == (prize_seen >= 2));
        }

        for (int p = 0; p < PRIZE_COUNT; ++p) {
            if (tuning::SCRATCH_PRIZES[p].value == ticket.prize) ++hits[p];
        }
    }

    // Each prize's share should match its weight (within a small tolerance).
    int total_weight = 0;
    for (const tuning::ScratchPrize& p : tuning::SCRATCH_PRIZES) total_weight += p.weight;
    std::printf("prize   expected  actual\n");
    for (int p = 0; p < PRIZE_COUNT; ++p) {
        double expected = static_cast<double>(tuning::SCRATCH_PRIZES[p].weight) / total_weight;
        double actual = static_cast<double>(hits[p]) / TICKETS;
        std::printf("$%-5d  %6.2f%%   %6.2f%%\n", tuning::SCRATCH_PRIZES[p].value, expected * 100, actual * 100);
        CHECK(std::fabs(actual - expected) < 0.01);
    }
    std::printf("Average ticket: $%.2f\n", scratch_expected_value());

    if (failures) {
        std::printf("%d scratch check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("All scratch checks passed.\n");
    return 0;
}
