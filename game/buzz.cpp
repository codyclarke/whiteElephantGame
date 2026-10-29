#include "game/buzz.h"

#include <cmath>

#include "game/tuning.h"

void Buzz::reset() {
    level_ = 0.0f;
    over_limit_ = 0.0f;
    dose_count_ = 0;
}

void Buzz::update(float dt) {
    // Drinks soak in a little each frame (water soaks in as negative buzz).
    for (int i = 0; i < dose_count_; ++i) {
        Dose& d = doses_[i];
        float take = d.per_second * dt;
        if (std::fabs(take) > std::fabs(d.remaining)) take = d.remaining;
        d.remaining -= take;
        level_ += take;
    }
    drop_empty_doses();

    level_ -= tuning::BUZZ_DECAY_PER_SECOND * dt;
    if (level_ < 0.0f) level_ = 0.0f;
    if (level_ > 100.0f) level_ = 100.0f;

    if (level_ >= tuning::BLACKOUT) {
        over_limit_ += dt;
    } else {
        over_limit_ = 0.0f;
    }
}

bool Buzz::drink(float amount, float absorb_seconds) {
    if (dose_count_ == MAX_DOSES) return false;
    if (absorb_seconds < 0.1f) absorb_seconds = 0.1f;
    doses_[dose_count_++] = {amount, amount / absorb_seconds};
    return true;
}

float Buzz::pending() const {
    float total = 0.0f;
    for (int i = 0; i < dose_count_; ++i) total += doses_[i].remaining;
    return total;
}

Zone Buzz::zone() const {
    if (level_ < tuning::SWEET_SPOT_LOW) return Zone::Sober;
    if (level_ <= tuning::SWEET_SPOT_HIGH) return Zone::SweetSpot;
    if (level_ < tuning::BLACKOUT) return Zone::Drunk;
    return Zone::Blackout;
}

bool Buzz::cut_off() const {
    return over_limit_ >= tuning::BLACKOUT_SECONDS;
}

float Buzz::drunkenness() const {
    float d = (level_ - tuning::SWEET_SPOT_HIGH) / (tuning::BLACKOUT - tuning::SWEET_SPOT_HIGH);
    if (d < 0.0f) return 0.0f;
    if (d > 1.0f) return 1.0f;
    return d;
}

void Buzz::drop_empty_doses() {
    int kept = 0;
    for (int i = 0; i < dose_count_; ++i) {
        if (std::fabs(doses_[i].remaining) > 0.001f) doses_[kept++] = doses_[i];
    }
    dose_count_ = kept;
}
