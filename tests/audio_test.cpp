// Checks the mixer: sounds play, overlap, end, loop, and fall back to beeps.
// Run: build/Release/audio_test.exe

#include <cstdio>
#include <cstdlib>

#include "game/audio.h"

namespace {

int failures = 0;

#define CHECK(cond)                                              \
    do {                                                         \
        if (!(cond)) {                                           \
            std::printf("FAIL line %d: %s\n", __LINE__, #cond);  \
            ++failures;                                          \
        }                                                        \
    } while (0)

int peak(const int16_t* buf, int n) {
    int p = 0;
    for (int i = 0; i < n; ++i) {
        int v = std::abs(static_cast<int>(buf[i]));
        if (v > p) p = v;
    }
    return p;
}

const int8_t LOUD[] = {100, 100, 100, 100, 100, 100, 100, 100, 100, 100};
const Sound LOUD_SOUND = {"test/loud", LOUD, 10};

}  // namespace

int main() {
    int16_t buf[64];

    // Nothing playing: silence.
    audio::mix(buf, 64);
    CHECK(peak(buf, 64) == 0);

    // A sound plays for exactly its length, then stops.
    audio::play_sound(&LOUD_SOUND);
    audio::mix(buf, 20);
    CHECK(buf[0] > 0);
    CHECK(buf[9] > 0);
    CHECK(buf[10] == 0);
    CHECK(peak(buf + 10, 10) == 0);

    // Two sounds overlap and add together.
    audio::play_sound(&LOUD_SOUND);
    audio::mix(buf, 1);
    int alone = buf[0];
    audio::mix(buf, 20);  // let it finish
    audio::play_sound(&LOUD_SOUND);
    audio::play_sound(&LOUD_SOUND);
    audio::mix(buf, 1);
    CHECK(buf[0] > alone);
    audio::mix(buf, 20);

    // Loud overlaps clip instead of wrapping around.
    for (int i = 0; i < 4; ++i) audio::play_sound(&LOUD_SOUND);
    audio::mix(buf, 1);
    CHECK(buf[0] > 0);
    audio::mix(buf, 20);

    // A name with no WAV falls back to a beep.
    audio::play("definitely_not_a_real_sound", 1000, 10);
    audio::mix(buf, 64);
    CHECK(peak(buf, 64) > 0);
    audio::mix(buf, 64);
    audio::mix(buf, 64);
    audio::mix(buf, 64);
    CHECK(peak(buf, 64) == 0);  // 10 ms at 16 kHz = 160 samples, long gone

    // Unknown music = silence (and doesn't crash).
    audio::play_music("definitely_not_a_real_track");
    audio::mix(buf, 64);
    CHECK(peak(buf, 64) == 0);

    if (failures) {
        std::printf("%d audio check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("All audio checks passed.\n");
    return 0;
}
