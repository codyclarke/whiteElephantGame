#pragma once

// The buzz meter: real buzz, plus drinks still soaking in ("pending").
enum class Zone { Sober, SweetSpot, Drunk, Blackout };

class Buzz {
public:
    void reset();
    void update(float dt);

    // Start absorbing a drink. A negative amount sobers you up (water).
    // Returns false if you're already juggling too many.
    bool drink(float amount, float absorb_seconds);

    float level() const { return level_; }
    float pending() const;      // net buzz still soaking in (negative while water works)
    bool holding_drink() const { return dose_count_ > 0; }
    Zone zone() const;

    // Seconds spent above the blackout line (resets when you drop below).
    float over_limit_seconds() const { return over_limit_; }
    bool cut_off() const;

    // 0 at the top of the sweet spot, 1 at the blackout line.
    float drunkenness() const;

private:
    struct Dose {
        float remaining;
        float per_second;
    };
    static constexpr int MAX_DOSES = 8;

    void drop_empty_doses();

    float level_ = 0.0f;
    float over_limit_ = 0.0f;
    Dose doses_[MAX_DOSES] = {};
    int dose_count_ = 0;
};
