// Runs the idle screen's snow scene for its full 2 minutes with many random
// seeds, and checks that snow actually builds up on the tree, the far drifts
// and the field in front,
// that branches shed, and how long a frame takes.
// Run: build/Release/snow_test.exe

#include <chrono>
#include <cstdio>

#include "game/gfx.h"
#include "game/snow.h"
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

SnowScene scene;      // big: keep them off the stack
gfx::Canvas canvas;

}  // namespace

int main() {
    const int SEEDS = 20;
    const float DT = 1.0f / 30.0f;
    const int FRAMES = static_cast<int>(tuning::IDLE_SECONDS * 30.0f);

    int total_snow = 0, total_tree = 0, total_sheds = 0, most_particles = 0;
    float total_cover = 0.0f, least_cover = 1.0f;
    double update_ms = 0.0, draw_ms = 0.0;

    for (int seed = 1; seed <= SEEDS; ++seed) {
        Rng rng(static_cast<uint32_t>(seed) * 7919u);
        scene.generate(rng);
        CHECK(scene.snow_cells() == 0);
        CHECK(scene.field_cover() == 0.0f);

        for (int f = 0; f < FRAMES; ++f) {
            auto t0 = std::chrono::steady_clock::now();
            scene.update(DT, rng);
            auto t1 = std::chrono::steady_clock::now();
            scene.draw(canvas);
            auto t2 = std::chrono::steady_clock::now();
            update_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
            draw_ms += std::chrono::duration<double, std::milli>(t2 - t1).count();
            if (scene.particles() > most_particles) most_particles = scene.particles();
        }

        CHECK(scene.snow_cells() > 500);  // drifts along the far edge
        CHECK(scene.snow_on_tree() > 20);  // and the branches have some
        CHECK(scene.field_cover() > 0.6f);  // the field in front has mostly gone white
        total_cover += scene.field_cover();
        if (scene.field_cover() < least_cover) least_cover = scene.field_cover();
        total_snow += scene.snow_cells();
        total_tree += scene.snow_on_tree();
        total_sheds += scene.sheds();
    }

    CHECK(total_sheds > 0);
    int frames = SEEDS * FRAMES;
    std::printf("After %.0f seconds, on average: %d snow cells, %d resting on the tree, %.1f branch sheds\n",
                tuning::IDLE_SECONDS, total_snow / SEEDS, total_tree / SEEDS, static_cast<double>(total_sheds) / SEEDS);
    std::printf("Field covered: %.0f%% on average, %.0f%% at least\n", total_cover / SEEDS * 100.0f, least_cover * 100.0f);
    std::printf("Most particles at once: %d\n", most_particles);
    std::printf("Per frame on this PC: update %.3f ms, draw %.3f ms\n", update_ms / frames, draw_ms / frames);

    if (failures) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("All snow checks passed.\n");
    return 0;
}
