#include "game/party.h"

#include "game/tuning.h"

void Party::start(int seats, const int* content_ids, const int* values, Rng& rng) {
    if (seats > MAX_SEATS) seats = MAX_SEATS;
    seats_ = seats;
    for (int g = 0; g < seats_; ++g) {
        gifts_[g] = {content_ids[g], values[g], -1, 0, rng.below(16)};
    }

    // Draw numbers: shuffle the turn order.
    for (int s = 0; s < seats_; ++s) {
        order_[s] = s;
        stolen_by_[s] = -1;
    }
    for (int i = seats_ - 1; i > 0; --i) {
        int j = rng.below(i + 1);
        int tmp = order_[i];
        order_[i] = order_[j];
        order_[j] = tmp;
    }

    turn_ = 0;
    actor_ = order_[0];
    forbidden_ = -1;
    final_ = false;
    over_ = false;
}

int Party::gift_of(int seat) const {
    for (int g = 0; g < seats_; ++g) {
        if (gifts_[g].holder == seat) return g;
    }
    return -1;
}

int Party::wrapped_count() const {
    int n = 0;
    for (int g = 0; g < seats_; ++g) {
        if (gifts_[g].holder < 0) ++n;
    }
    return n;
}

bool Party::frozen(int g) const {
    return gifts_[g].steals >= tuning::STEALS_TO_FREEZE;
}

bool Party::can_steal(int g) const {
    if (over_) return false;
    int holder = gifts_[g].holder;
    return holder >= 0 && holder != actor_ && !frozen(g) && g != forbidden_;
}

bool Party::any_steal() const {
    for (int g = 0; g < seats_; ++g) {
        if (can_steal(g)) return true;
    }
    return false;
}

int Party::pick_wrapped(Rng& rng) const {
    int k = rng.below(wrapped_count());
    for (int g = 0; g < seats_; ++g) {
        if (gifts_[g].holder < 0 && k-- == 0) return g;
    }
    return -1;
}

void Party::open(int g) {
    gifts_[g].holder = actor_;
    next_turn();
}

void Party::steal(int g) {
    int victim = gifts_[g].holder;
    ++gifts_[g].steals;
    stolen_by_[victim] = actor_;

    if (final_) {
        // The final swap trades gifts instead of starting a chain.
        int mine = gift_of(actor_);
        if (mine >= 0) gifts_[mine].holder = victim;
        gifts_[g].holder = actor_;
        finish();
        return;
    }

    gifts_[g].holder = actor_;
    actor_ = victim;
    forbidden_ = g;
}

void Party::keep() {
    finish();
}

void Party::next_turn() {
    forbidden_ = -1;
    ++turn_;
    if (turn_ < seats_) {
        actor_ = order_[turn_];
        return;
    }
    // Everything's open: the first player gets the final swap, if there's
    // anything left to swap for.
    final_ = true;
    actor_ = order_[0];
    if (!any_steal()) finish();
}

void Party::finish() {
    over_ = true;
    final_ = false;
    actor_ = -1;
    forbidden_ = -1;
}
