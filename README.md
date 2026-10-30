# Steal Night

A white elephant party game for a custom Pico 2 handheld. See [PLAN.md](PLAN.md)
for the platform and [DESIGN.md](DESIGN.md) for the gameplay.

## Build and run on Windows

Requires Visual Studio Build Tools 2022, CMake, and Python with Pillow (for
converting art). SDL2 is downloaded automatically the first time.

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build/Release/steal_night.exe
```

## Build for the handheld

The handheld build uses the ARM compiler and Pico SDK. Get them by installing
the **Raspberry Pi Pico** extension in VS Code and creating any one project
with it (that downloads everything into `~/.pico-sdk`). Then:

```bash
python tools/pico.py flash hw_test
```

and then the game itself:

```bash
python tools/pico.py flash steal_night
```

`hw_test` is the wiring check; [WIRING.md](WIRING.md) says how to hook
everything up and what the test should show.

## Controls (desktop)

| Key | Handheld | Does |
|---|---|---|
| Arrow keys | Joystick | Move the cursor / answer banter |
| A | A | Act: open / steal |
| S (hold) | X | Hide your gift |
| D | B | Call the drink tray, then grab the drink under your hand |
| Enter | Y | Pause |
| Esc | - | Quit |

Test-mode scripts still use the handheld's button names (`A B X Y`).

## Side strips: string lights

The handheld's 2.8" screen is 320x240, and the game is 240x240 in the middle,
so there's a 40-pixel strip on each side. Each holds a strand of string lights
(the desktop window shows the same layout):

- Off the party (title, builder, results, idle snow), they twinkle gently.
- At the party, the chase speeds up as your buzz climbs, and the strands swing
  when you're drunk.
- Past the blackout line, every bulb flashes red together.
- A gold chase runs up both strands when you pull off a steal.
- Cut off: the lights die, except one sputtering bulb.
- Asleep: off.

The drawing code is `draw_strip()` in `game/game.cpp`. Test-mode screenshots
now include the strips.

## Idle screen and sleep

Leave the title screen or the character builder alone for 30 seconds and it
switches to a snowy night: a Christmas tree outside, slowly collecting snow.
Every time, the tree's shape, lights, ornaments, topper, moon and stars are
rolled fresh. The snow is simulated: it piles up on the branches and in
drifts along the far edge of the ground, overloaded branches dump their snow,
gusts push it around, and the snowfall comes and goes. The ground in front is
a field seen from above: nearer, faster flakes fall in front of the tree and
land at different depths, so it goes from bare dirt to patchy to white.

Any button goes back to where you were. After 2 minutes the screen turns off
(sleep); any button then wakes it to the title screen. The timings, snowfall
and gust strength are in the "Idle screen" section of `game/tuning.h`.

## High scores

After the season (or getting cut off, with what you'd banked), a run that
makes the top 10 asks for 3 initials: Up/Down changes the letter, A moves on
(and finishes on the third), B goes back. The board then shows, and the title
screen takes turns with it every 8 seconds. Scores are saved in
`steal_night.sav` next to the game, along with your look.

- **Prepopulate:** edit `DEFAULT_HIGH_SCORES` in `game/content.h`. A fresh
  handheld (or a fresh save) starts with that list, and resetting puts it back.
  For a blank board, leave a single `{"", 0}` line.
- **Reset on the desktop:**

  ```bash
  build/Release/steal_night.exe --reset-scores
  ```

  This keeps your look. Deleting `steal_night.sav` resets the look too.
- **Reset on the handheld:** hold X + Y on the title screen for 3 seconds
  (S + Enter on the desktop). To disable this before the party, set
  `SCORE_RESET_HOLD_SECONDS` to 0 in `game/tuning.h`.
- **For the final release:** set your `DEFAULT_HIGH_SCORES`, rebuild, reset
  the board once, then turn the combo off.

## Where things live

| File | What's in it |
|---|---|
| `game/tuning.h` | Every gameplay number: buzz, drinks, AI greed, steal mini-game, pacing, scoring |
| `game/content.h` | All the jokey text: guests and their personalities, gifts, chatter |
| `game/game.cpp` | Game flow, turns, mini-game, screens and drawing |
| `game/party.cpp` | The white elephant rules (turn order, steals, freeze, final swap) |
| `game/ai.cpp` | How AI guests decide to open or steal |
| `game/buzz.cpp` | The buzz meter model |
| `game/audio.cpp` | Sound mixer: effects, music loops, fallback beeps |
| `tests/party_test.cpp` | Plays 40,000 parties and checks the rules always hold |
| `game/gfx.cpp` | Drawing primitives and text |
| `assets/` | Your art (PNGs) and sound (WAVs) |
| `tools/png2sprite.py`, `tools/wav2sound.py` | Convert `assets/` into game data (run on every build) |
| `platform/platform.h` | The only things the game may ask of the hardware |
| `platform/sdl/` | Desktop backend (window, keyboard, sound) |
| `platform/pico/` | Handheld backend: screen, sound and the wiring test; `pins.h` has the pin numbers |
| `tools/pico.py` | Builds the handheld firmware and loads it onto the Pico |

## Making art

Every PNG in `assets/` is converted into game data on each build. Save a PNG,
rebuild, and it's in the game. Any piece without a PNG falls back to the
built-in shapes.

- **Transparency:** pixels with alpha below 50% are see-through.
- **Animation:** put frames side by side and add `_fN` to the file name,
  e.g. `body_f4.png` is 4 frames, each a quarter of the image's width.
- **Placeholders:** the PNGs currently in `assets/` are templates that match
  the built-in art. Paint over them. To regenerate any you've deleted (existing
  files are never touched):

  ```bash
  python tools/make_placeholders.py
  ```

| File | Frame size | Frames |
|---|---|---|
| `tree_f2.png` | 64x64 | Cycles twice a second, for twinkling |
| `gift_wrapped_f5.png` | 14x12 | One per wrapping paper; gifts pick one at random |
| `gift_open_f5.png` | 12x8 | Opened gift above someone's head, matching its paper |
| `couch.png` | 112x30 | Cut-off screen |
| `drinks/<name>.png` or `drinks/<look>.png` | ~16x20 | Optional. A drink's own art (`good_wine.png`) wins; otherwise its *look* from `game/tuning.h` (`beer`, `wine`, `nog`, `shot`, `punch`, `water`). Give two drinks one shared look file to make them indistinguishable |
| `hand.png` | ~12x12 | Optional: your hand sweeping over the drink tray |
| `room.png` | 240x196 | Background behind everyone (the string lights draw on top) |

### People: built from parts

You and every guest are layered from parts in `assets/people/`, all 24x32 and
lined up with each other:

| File | Frames | Notes |
|---|---|---|
| `people/body_f4.png` | 4 | Head, face and neck: 0 idle, 1 talking (banter), 2 thinking (their turn), 3 drunk (you, when drunk) |
| `people/sweaters/<style>.png` | 1 | `plain`, `stripes`, `snowflake`, `reindeer`, `tree` |
| `people/hair/<style>.png` | 1 or 2 | `classic`, `long`, `curly`, `bun`, `spiky`, `mohawk` (Bald has no file). Optional second frame: see "Hair under hats" |
| `people/hats/<hat>.png` | 1 | `santa_hat`, `antlers`, `bow`, `beanie` |

**Tinting:** draw the parts of a layer that should change color using exactly
these four grays, and the game swaps them for the chosen color ramp:

| Gray | Hex | Role |
|---|---|---|
| 255 | `#FFFFFF` | highlight |
| 192 | `#C0C0C0` | light (main color) |
| 128 | `#808080` | mid |
| 64 | `#404040` | shadow |

Any other color is drawn exactly as painted: black outlines, a white
snowflake, a red Santa hat. Bodies use the skin ramp, hair the hair ramp, and
sweaters and hats the sweater ramp. The ramps, style names and each guest's
look are in `game/content.h`. Adding a style means adding its name there plus
a PNG.

**Hair under hats:** to give a hairstyle a version that fits under a hat,
widen its PNG to two frames and rename it with `_f2` (`hair/spiky.png` becomes
`hair/spiky_f2.png`). Frame 1 is the normal hair, frame 2 is the tucked-in
version. Hats with `tucks_hair` set to `true` in `game/content.h` (currently
Santa Hat and Beanie) use frame 2; hairstyles with only one frame keep their
main version under every hat. Delete the old one-frame file when you rename it,
or the build stops and tells you there are two.

The base templates were sliced from your own `guests/you_f4.png`, and the
other styles drawn to match. To regenerate any you delete (existing files are
never touched):

```bash
python tools/make_people_parts.py
```

**Hand-drawn guests:** a full 4-frame sprite at `guests/<label>_f4.png` (label
in lowercase with underscores, e.g. `guests/sales_guy_f4.png`) replaces that
guest's parts. You always use your built look.

You can change frame sizes, but the layout is tuned for the sizes above, so
tell Claude if you want a different size and it'll adjust where things sit.

## Making sound

Every WAV in `assets/` is converted into game data on each build, just like
the art. Save a WAV, rebuild, and it plays. Any event without a WAV plays its
built-in beep instead, so you can add sounds one at a time.

- **Format:** any sample rate, mono or stereo, 8/16/24-bit PCM. (If your editor
  offers "32-bit float", pick 16-bit instead.) Everything is mixed down to mono
  16 kHz 8-bit: lo-fi on purpose, to fit on the handheld.
- **Sound effects** go in `assets/sounds/<name>.wav`. Up to 4 play at once.
- **Music** goes in `assets/music/<name>.wav` and loops. Trim it to loop
  cleanly. Music sits under the effects.
- **Volumes** are at the bottom of `game/tuning.h` (`MUSIC_VOLUME`,
  `SFX_VOLUME`, `BEEP_VOLUME`).
- **Budget:** about 16 KB per second of sound. Effects are tiny; keep the music
  loops to about 1–2 minutes in total.
- **Good tools:** jsfxr (sfxr.me) or ChipTone for retro effects, and Audacity
  for recording and trimming.

**Sound effects** (`assets/sounds/`):

| File | When it plays |
|---|---|
| `cursor.wav` | Moving the cursor |
| `tray.wav` | The drink tray appears |
| `drink.wav` / `water.wav` | Grabbing a drink / grabbing water |
| `hide.wav` / `caught_hiding.wav` | Starting to hide / getting caught |
| `your_turn.wav` | Your turn starts |
| `reveal_good.wav` / `reveal_bad.wav` | Anyone unwraps a gift (bad = negative value) |
| `steal.wav` | A guest steals from another guest |
| `stolen_from_you.wav` | A guest steals your gift |
| `grab_start.wav` / `grab_hit.wav` / `grab_miss.wav` | Your steal mini-game: start / success / fumble |
| `banter_start.wav` | A guest starts talking to you |
| `banter_ok.wav` / `banter_flop.wav` | Your banter lands / flops |
| `warning.wav` | Beeps while you're over the blackout line |
| `cut_off.wav` | You're cut off |
| `party_start.wav` / `party_over.wav` | A party begins / ends |
| `season_over.wav` | The season is complete |
| `pause.wav` | Pausing and unpausing (also plays when the high scores are reset) |
| `high_score.wav` | Your run made the high score board |
| `builder_change.wav` / `builder_surprise.wav` | Changing an option / "Surprise me" in the character builder |
| `scratch_start.wav` | The scratcher mini-game begins |
| `scratch.wav` | Repeats while you hold A to scratch (keep it very short) |
| `scratch_reveal.wav` / `scratch_win.wav` | A spot is fully uncovered / the matching pair turns up |

**Music loops** (`assets/music/`): `title.wav`, `party.wav`, `scratch.wav`,
`results.wav`, `season_over.wav`, `cut_off.wav`, `idle.wav` (the snowy idle
screen; keep it quiet).


## Tests

```bash
build/Release/party_test.exe
```

```bash
build/Release/audio_test.exe
```

```bash
build/Release/scratch_test.exe
```

```bash
build/Release/snow_test.exe
```

The scratch test also prints how often each prize comes up and the average
ticket value, which is handy when you're tuning `SCRATCH_PRIZES`. The snow
test runs 20 random idle scenes for the full 2 minutes, checks the snow builds
up and branches shed, and prints how long a frame takes.

## Test mode

The desktop build can run without a window, follow a button script, and save a
screenshot:

```bash
build/Release/steal_night.exe --shot out.bmp --frames 300 --script "10:A 20:B 70:B"
```

`FRAME:BUTTONS` presses buttons on one frame, and `FIRST-LAST:BUTTONS` holds
them across a range. Buttons are `UP DOWN LEFT RIGHT PRESS A B X Y`, joined
with commas. The game runs at 30 frames per second. Add `--scene scratch` to
start directly on the lottery scratcher mini-game, `--scene builder` for the
character builder, `--scene initials` to enter initials for a $500 run, or
`--scene idle` for the snowy idle screen. `--seed N` changes the random seed
(default 12345); for the idle screen that's a different tree and sky.
Test mode never reads or writes the save file. Test mode prints every
message-line change with its frame number, so you can see what happened when.
# whiteElephantGame
