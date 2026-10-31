// Desktop backend: runs the game in a window with keyboard controls and sound.
//
// Controls: arrow keys = joystick, A = act (handheld A), S = hide (handheld X),
//           D = drink (handheld B), Enter = pause (handheld Y),
//           - and = = volume down / up, Esc = quit.
//
// Test mode (no window, runs as fast as possible, saves a screenshot):
//   steal_night --shot out.bmp --frames 300 --script "30:A 60:B 61-70:RIGHT"
// Script entries are FRAME:BUTTONS or FIRST-LAST:BUTTONS, buttons joined by
// commas (UP DOWN LEFT RIGHT PRESS A B X Y VOLUP VOLDOWN). The game runs at 30 frames/sec.
// Add --scene scratch or --scene builder to start on the lottery scratcher or
// the character builder, --scene initials to enter initials for a $500 run,
// or --scene idle for the snowy-tree idle screen. --seed N changes the random
// seed (default 12345), e.g. for a different snowy scene.
//
//   steal_night --reset-scores
// puts the high score board back to DEFAULT_HIGH_SCORES (game/content.h) in
// the save file, keeps your look, and exits.

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "game/audio.h"
#include "game/game.h"
#include "game/gfx.h"
#include "platform/platform.h"

namespace {

constexpr int WINDOW_SCALE = 3;
// The window is the handheld's 2.8" screen: 320x240, game in the middle.
constexpr int SCREEN_W = gfx::SCREEN_W;
constexpr int SCREEN_H = gfx::SCREEN_H;
constexpr float STEP = 1.0f / 30.0f;
SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_Texture* texture = nullptr;
SDL_AudioDeviceID audio_device = 0;

// Test mode replaces the keyboard with a script.
bool scripted = false;
uint16_t scripted_buttons = 0;

gfx::Canvas canvas;  // 150 KB -- too big for the stack

// SDL asks for sound on its own thread; the game's mixer fills it.
void audio_callback(void*, Uint8* stream, int len) {
    audio::mix(reinterpret_cast<int16_t*>(stream), len / 2);
}

uint16_t keyboard_buttons() {
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    uint16_t b = 0;
    if (k[SDL_SCANCODE_UP]) b |= platform::BTN_UP;
    if (k[SDL_SCANCODE_DOWN]) b |= platform::BTN_DOWN;
    if (k[SDL_SCANCODE_LEFT]) b |= platform::BTN_LEFT;
    if (k[SDL_SCANCODE_RIGHT]) b |= platform::BTN_RIGHT;
    if (k[SDL_SCANCODE_A]) b |= platform::BTN_A;       // act
    if (k[SDL_SCANCODE_S]) b |= platform::BTN_X;       // hide
    if (k[SDL_SCANCODE_D]) b |= platform::BTN_B;       // drink
    if (k[SDL_SCANCODE_RETURN]) b |= platform::BTN_Y;  // pause
    if (k[SDL_SCANCODE_EQUALS] || k[SDL_SCANCODE_KP_PLUS]) b |= platform::BTN_VOL_UP;
    if (k[SDL_SCANCODE_MINUS] || k[SDL_SCANCODE_KP_MINUS]) b |= platform::BTN_VOL_DOWN;
    return b;
}

// --- Test-mode script ---------------------------------------------------------

struct ScriptEntry {
    int first;
    int last;
    uint16_t buttons;
};

uint16_t button_from_name(const std::string& name) {
    static const struct {
        const char* name;
        uint16_t bit;
    } NAMES[] = {
        {"UP", platform::BTN_UP},   {"DOWN", platform::BTN_DOWN}, {"LEFT", platform::BTN_LEFT},
        {"RIGHT", platform::BTN_RIGHT}, {"PRESS", platform::BTN_PRESS}, {"A", platform::BTN_A},
        {"B", platform::BTN_B},     {"X", platform::BTN_X},       {"Y", platform::BTN_Y},
        {"VOLUP", platform::BTN_VOL_UP}, {"VOLDOWN", platform::BTN_VOL_DOWN},
    };
    for (const auto& n : NAMES) {
        if (name == n.name) return n.bit;
    }
    std::fprintf(stderr, "Unknown button in script: %s\n", name.c_str());
    std::exit(1);
}

std::vector<ScriptEntry> parse_script(const char* text) {
    std::vector<ScriptEntry> entries;
    std::string s(text);
    size_t pos = 0;
    while (pos < s.size()) {
        size_t end = s.find(' ', pos);
        if (end == std::string::npos) end = s.size();
        std::string item = s.substr(pos, end - pos);
        pos = end + 1;
        if (item.empty()) continue;

        size_t colon = item.find(':');
        if (colon == std::string::npos) {
            std::fprintf(stderr, "Bad script entry (want FRAME:BUTTONS): %s\n", item.c_str());
            std::exit(1);
        }
        ScriptEntry e{};
        std::string frames = item.substr(0, colon);
        size_t dash = frames.find('-');
        e.first = std::atoi(frames.substr(0, dash).c_str());
        e.last = dash == std::string::npos ? e.first : std::atoi(frames.substr(dash + 1).c_str());

        std::string names = item.substr(colon + 1);
        size_t npos = 0;
        while (npos <= names.size()) {
            size_t comma = names.find(',', npos);
            if (comma == std::string::npos) comma = names.size();
            e.buttons |= button_from_name(names.substr(npos, comma - npos));
            npos = comma + 1;
        }
        entries.push_back(e);
    }
    return entries;
}

bool save_screenshot(const char* path) {
    // Save at 2x so it's easier to look at.
    SDL_Surface* src = SDL_CreateRGBSurfaceWithFormatFrom(canvas.px, SCREEN_W, SCREEN_H, 16, SCREEN_W * 2,
                                                          SDL_PIXELFORMAT_RGB565);
    SDL_Surface* big = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_W * 2, SCREEN_H * 2, 16, SDL_PIXELFORMAT_RGB565);
    bool ok = src && big && SDL_BlitScaled(src, nullptr, big, nullptr) == 0 && SDL_SaveBMP(big, path) == 0;
    if (!ok) std::fprintf(stderr, "Screenshot failed: %s\n", SDL_GetError());
    SDL_FreeSurface(big);
    SDL_FreeSurface(src);
    return ok;
}

int run_test_mode(const char* shot_path, int frames, const char* script_text, const char* scene, uint32_t seed) {
    std::vector<ScriptEntry> script = parse_script(script_text ? script_text : "");
    scripted = true;
    SDL_Init(0);

    Game game(seed);  // fixed seed (12345 unless --seed) so test runs are repeatable
    if (scene && std::strcmp(scene, "scratch") == 0) game.debug_start_scratch();
    if (scene && std::strcmp(scene, "builder") == 0) game.debug_start_builder();
    if (scene && std::strcmp(scene, "initials") == 0) game.debug_start_initials(500);
    if (scene && std::strcmp(scene, "idle") == 0) game.debug_start_idle();
    uint16_t prev = 0;
    std::string last_message;
    for (int frame = 0; frame < frames; ++frame) {
        scripted_buttons = 0;
        for (const ScriptEntry& e : script) {
            if (frame >= e.first && frame <= e.last) scripted_buttons |= e.buttons;
        }
        uint16_t held = platform::buttons();
        game.update({held, static_cast<uint16_t>(held & ~prev)}, STEP);
        prev = held;

        // Log each new message line, so you can find the frame something happened on.
        if (last_message != game.ticker()) {
            last_message = game.ticker();
            std::printf("%5d  %s\n", frame, last_message.c_str());
        }
    }
    game.render(canvas);
    bool ok = save_screenshot(shot_path);
    SDL_Quit();
    return ok ? 0 : 1;
}

}  // namespace

// --- platform.h -----------------------------------------------------------------

namespace platform {

uint16_t buttons() {
    return scripted ? scripted_buttons : keyboard_buttons();
}

uint32_t millis() {
    return SDL_GetTicks();
}

// No backlight on a PC: the game draws a black screen while it sleeps.
void set_backlight(bool) {}

void present(const uint16_t* pixels) {
    if (!renderer) return;
    SDL_UpdateTexture(texture, nullptr, pixels, SCREEN_W * 2);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}

// Saves go in a file next to the game's .exe. Test mode never touches it, so
// scripted runs stay repeatable.
static std::string save_path() {
    char* base = SDL_GetBasePath();
    std::string path = base ? base : "";
    SDL_free(base);
    return path + "steal_night.sav";
}

bool save_data(const void* data, uint32_t size) {
    if (scripted) return false;
    FILE* f = std::fopen(save_path().c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(data, 1, size, f) == size;
    std::fclose(f);
    return ok;
}

uint32_t load_data(void* data, uint32_t size) {
    if (scripted) return 0;
    FILE* f = std::fopen(save_path().c_str(), "rb");
    if (!f) return 0;
    uint32_t got = static_cast<uint32_t>(std::fread(data, 1, size, f));
    std::fclose(f);
    return got;
}

void audio_lock() {
    if (audio_device) SDL_LockAudioDevice(audio_device);
}

void audio_unlock() {
    if (audio_device) SDL_UnlockAudioDevice(audio_device);
}

}  // namespace platform

// --- main -----------------------------------------------------------------------

int main(int argc, char** argv) {
    SDL_SetMainReady();

    const char* shot = nullptr;
    const char* script = nullptr;
    const char* scene = nullptr;
    int frames = 1;
    uint32_t seed = 12345;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--reset-scores") == 0) {
            SDL_Init(0);  // for the save file's location
            Game game(0);
            game.reset_high_scores();
            std::printf("High scores reset to the defaults in game/content.h.\n");
            SDL_Quit();
            return 0;
        }
    }
    for (int i = 1; i + 1 < argc; i += 2) {
        if (std::strcmp(argv[i], "--shot") == 0) shot = argv[i + 1];
        else if (std::strcmp(argv[i], "--frames") == 0) frames = std::atoi(argv[i + 1]);
        else if (std::strcmp(argv[i], "--script") == 0) script = argv[i + 1];
        else if (std::strcmp(argv[i], "--scene") == 0) scene = argv[i + 1];
        else if (std::strcmp(argv[i], "--seed") == 0) seed = static_cast<uint32_t>(std::strtoul(argv[i + 1], nullptr, 10));
    }
    if (shot) return run_test_mode(shot, frames, script, scene, seed);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");  // crisp pixels
    window = SDL_CreateWindow("Steal Night", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              SCREEN_W * WINDOW_SCALE, SCREEN_H * WINDOW_SCALE, SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!window || !renderer) {
        std::fprintf(stderr, "Window setup failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_RenderSetLogicalSize(renderer, SCREEN_W, SCREEN_H);
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);

    // Sound is optional -- the game runs silently if there's no audio device.
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
        SDL_AudioSpec want{};
        want.freq = audio::SAMPLE_RATE;  // SDL converts to whatever the sound card wants
        want.format = AUDIO_S16SYS;
        want.channels = 1;
        want.samples = 512;
        want.callback = audio_callback;
        audio_device = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
        if (audio_device) SDL_PauseAudioDevice(audio_device, 0);
    }

    Game game(SDL_GetTicks() ^ static_cast<uint32_t>(SDL_GetPerformanceCounter()));
    uint16_t prev = 0;
    float accumulator = 0.0f;
    uint32_t last = SDL_GetTicks();
    bool running = true;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) running = false;
        }

        uint32_t now = SDL_GetTicks();
        accumulator += (now - last) / 1000.0f;
        last = now;
        if (accumulator > 0.25f) accumulator = 0.25f;  // don't spiral after a stall

        // Fixed-rate updates, same as the handheld will do.
        while (accumulator >= STEP) {
            uint16_t held = platform::buttons();
            game.update({held, static_cast<uint16_t>(held & ~prev)}, STEP);
            prev = held;
            accumulator -= STEP;
        }

        game.render(canvas);
        platform::present(canvas.px);
        SDL_Delay(game.sleeping() ? 30 : 1);  // asleep: go easy on the CPU
    }

    if (audio_device) SDL_CloseAudioDevice(audio_device);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
