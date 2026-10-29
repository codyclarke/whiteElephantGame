#pragma once

#include "game/rng.h"

// The white elephant rules, and nothing else: who holds what, whose turn it
// is, and what can be stolen. It knows nothing about drawing, timing, or which
// seats are human, so it can be tested on its own (tests/party_test.cpp).
//
// Rules:
//   - Turn order is shuffled. Each turn, the actor opens a wrapped gift or
//     steals an opened one.
//   - Whoever is robbed acts next, but can't steal back what was just taken.
//   - A turn ends when someone opens a gift.
//   - A gift freezes after tuning::STEALS_TO_FREEZE steals.
//   - After the last gift is opened, the first player may swap with any
//     unfrozen gift, and then the party is over.
class Party {
public:
    static constexpr int MAX_SEATS = 8;

    struct Gift {
        int content_id;  // index into content::GIFTS
        int value;       // rolled at party start (wildcards vary)
        int holder;      // seat holding it, or -1 while still wrapped
        int steals;      // times it has been stolen
        int wrap;        // wrapping-paper pick, for drawing
    };

    // One gift per seat: content_ids[i] and values[i] describe gift i.
    void start(int seats, const int* content_ids, const int* values, Rng& rng);

    int seats() const { return seats_; }
    int gift_count() const { return seats_; }
    const Gift& gift(int g) const { return gifts_[g]; }

    int actor() const { return actor_; }          // seat that must act, -1 when over
    bool over() const { return over_; }
    bool final_swap() const { return final_; }    // actor is making the end-of-party swap
    int first_seat() const { return order_[0]; }
    int forbidden() const { return forbidden_; }  // gift the actor can't steal back, or -1

    int gift_of(int seat) const;                  // gift held by seat, or -1
    int stolen_from_by(int seat) const { return stolen_by_[seat]; }  // last thief, or -1
    int wrapped_count() const;
    bool frozen(int g) const;
    bool can_steal(int g) const;                  // for the current actor
    bool any_steal() const;
    int pick_wrapped(Rng& rng) const;             // a random wrapped gift

    void open(int g);   // actor unwraps gift g
    void steal(int g);  // actor takes gift g (a swap during the final swap)
    void keep();        // final swap: keep what you have

private:
    void next_turn();
    void finish();

    int seats_ = 0;
    Gift gifts_[MAX_SEATS] = {};
    int order_[MAX_SEATS] = {};
    int stolen_by_[MAX_SEATS] = {};
    int turn_ = 0;
    int actor_ = -1;
    int forbidden_ = -1;
    bool final_ = false;
    bool over_ = true;
};
