#include "game/sprites.h"

#include <cstring>

// Defined in the generated sprites_data.cpp.
extern const Sprite SPRITES[];
extern const int SPRITE_COUNT;

const Sprite* find_sprite(const char* name) {
    for (int i = 0; i < SPRITE_COUNT; ++i) {
        if (std::strcmp(SPRITES[i].name, name) == 0) return &SPRITES[i];
    }
    return nullptr;
}
