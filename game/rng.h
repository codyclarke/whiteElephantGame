#pragma once
#include <cstdint>

// Small, fast, deterministic random numbers (xorshift32). Same seed, same game,
// which makes bugs reproducible.
struct Rng {
    uint32_t state;

    explicit Rng(uint32_t seed) : state(seed ? seed : 0x9E3779B9u) {}

    uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    // Integer in [0, n).
    int below(int n) { return n <= 0 ? 0 : static_cast<int>(next() % static_cast<uint32_t>(n)); }

    // Float in [0, 1).
    float unit() { return (next() >> 8) * (1.0f / 16777216.0f); }

    float range(float lo, float hi) { return lo + (hi - lo) * unit(); }

    bool chance(float p) { return unit() < p; }
};
