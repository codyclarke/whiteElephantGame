# White Elephant Handheld — Project Plan

A one-of-a-kind, Tiger-Electronics-style handheld game for a white elephant party.
Gameplay: swap/steal gifts, manage drinks to stay buzzed but not drunk (design TBD).

Gameplay design lives in [DESIGN.md](DESIGN.md).

## Platform decisions (locked)

| Area | Choice |
|---|---|
| MCU | Waveshare RP2350-Plus (RP2350A, Pico pin layout, USB-C, built-in LiPo charging) |
| Display | 2.8" 320x240 SPI TFT, ST7789V controller (confirmed by reading its ID), landscape. The game's 240x240 picture sits in the middle with a 40-pixel strip on each side |
| Input | Separate buttons in the case: a 5-way switch or D-pad (up/down/left/right/press) + A/B/X/Y tactile buttons |
| Audio | MAX98357A I2S amp + small 8 ohm speaker (replaces the piezo, so WAV sounds and music can play) |
| Power | 2000 mAh single-cell LiPo (protected; plug must match the board's battery socket, check polarity) + slide switch; boots straight into the game |
| Language | C++ (plain, readable; no clever templates) |
| Desktop build | Same game code compiled for Windows with SDL2 (window, keyboard, sound) |
| Look | Full color, with the option to emulate a segmented-LCD / monochrome look |

## Shopping list

- Waveshare RP2350-Plus (RP2350A, USB-C, LiPo charging) -- arrived
- 2.8" 320x240 SPI TFT module, 3.3 V logic. Got: the red (ST7789V inside, despite most listings saying ILI9341)
  "2.8\" TFT 240xRGBx320" module (82 x 50 mm board, 14-pin header, touch and SD
  slot on board but unused)
- MAX98357A I2S amp breakout + 8 ohm speaker (about 0.5–1 W, 28–40 mm)
- 2000 mAh 1S LiPo with protection circuit, and a plug that fits the
  RP2350-Plus battery socket (Waveshare uses a small 2-pin socket, likely
  MX1.25, not JST-PH: check before buying or plugging in)
- 5-way navigation switch (or a D-pad) + 4 tactile buttons (12 mm)
- 2 more tactile buttons (same type as each other) for volume up / down
- Slide switch (SPDT, rated 1 A or more) for the battery + wire, and a short
  battery extension lead with the board's battery plug, to put it in
- Hookup wire, heat-shrink, enclosure

## Wiring (draft)

Step-by-step breadboard guide, with physical pin numbers and what the test
should show: [WIRING.md](WIRING.md).

A starting point; check it against the RP2350-Plus pinout when the boards
arrive, and the pins get locked into the Pico code then. Buttons connect
between the pin and GND (the Pico's internal pull-ups are used). GP4/GP5 are
left free. The Waveshare RP2350-Plus has the regular Pico pin layout; its
battery voltage is on GP29 (from the SDK's `waveshare_rp2350_plus_4mb` board
file), so GP29 stays free.

| Part | Signal | Pico pin |
|---|---|---|
| TFT | SCK | GP18 (SPI0) |
| TFT | SDI (MOSI) | GP19 (SPI0) |
| TFT | CS | GP17 |
| TFT | DC | GP16 |
| TFT | RESET | GP20 |
| TFT | LED / backlight | GP21 (PWM, for brightness and sleep) |
| TFT | VCC, GND | 3V3 out, GND (short jumper J1 on the module for 3.3 V power; check the listing) |
| TFT | SDO (MISO), T_* touch pins, SD_* pins | not connected |
| Amp | DIN | GP9 |
| Amp | BCLK | GP10 |
| Amp | LRC | GP11 (must be BCLK + 1 for the I2S code) |
| Amp | SD (shutdown) | GP8 (off while asleep) |
| Amp | VIN, GND | VSYS (battery side, for more volume), GND |
| Buttons | Up / Down / Left / Right / Press | GP0 / GP1 / GP2 / GP3 / GP6 |
| Buttons | A / B / X / Y | GP12 / GP13 / GP14 / GP15 |
| Buttons | Volume down / up | GP26 / GP27 |
| Power | Slide switch | In the battery's + wire (see WIRING.md, step 4) |

## Architecture

```
game/        Platform-independent game logic (no hardware or SDL includes)
  tuning.h   Every gameplay number in one commented file -- safe to edit
platform/
  platform.h Tiny interface: drawing, buttons, beeps, time
  sdl/       Windows/desktop backend (SDL2)
  pico/      RP2350 backend (2.8" TFT over SPI, GPIO buttons, I2S amp)
assets/      Source PNGs (Aseprite etc.)
tools/       PNG -> C array converter script
```

Rule: `game/` only talks to `platform.h`. If it compiles on the PC, it runs on the Pico.

## Toolchain status (checked 2026-10-25)

- [x] MSVC Build Tools 2022 (14.43) + Windows SDK
- [x] CMake, Ninja, Git, Python
- [x] SDL2 2.30.9 (pulled automatically by CMake)
- [ ] Pico SDK + ARM GCC (needed once hardware arrives; easiest via the
      official "Raspberry Pi Pico" VS Code extension)

## Milestones

1. ~~Gameplay design pass (core loop, swap/steal rules, buzz meter, round end)~~
2. ~~Desktop skeleton: SDL window at 240x240 scaled up, input, game loop~~
   Done: title screen, party room, cursor, working buzz meter with drinks,
   water, drunk sway and cursor drift, hardcore cut-off screen, pause.
3. Playable prototype on PC with placeholder art
   Done so far: full white elephant turn system (AI personalities, open,
   steal chains, freeze rule, final swap), steal grab mini-game, gift
   reveals, party results, 3-party season, vibe meter and banter prompts,
   hide-your-gift (X), water as a drink, sprite and sound pipelines (PNG/WAV
   in assets/, built-in fallbacks), sound mixer with music loops.
   Drink tray grab mini-game. Lottery scratcher mini-game. Character
   builder with tinted parts (guests built from parts too), saved look.
   Persistent top-10 high score board with 3-letter initials.
   Idle screen (simulated snow on a random tree) and sleep. 320x240 screen
   layout with reactive string lights in the side strips.
   Remaining: shake hints, hiccups, HR strikes.
4. Hardware arrives -> Pico backend.
   Done (2026-10-30): screen confirmed as ST7789V, clean at 75 MHz SPI on the
   breadboard (full frame in ~16 ms, sent by DMA while the next update runs).
   The game runs on the RP2350-Plus: title ~3 ms of work per frame, snowy
   idle screen ~7 ms (budget 33 ms). Backend in platform/pico/: GPIO buttons,
   I2S audio (PIO + DMA), saves in the last flash sector, backlight and amp
   off while asleep, `Game` held in static memory (267 KB of 520 KB RAM used).
   Remaining: test buttons, sound and saves once wired; true low-power sleep
   (wake on a button); battery gauge from GP29.
5. Art, sound, polish, tuning
6. Enclosure + physical build
7. Party
