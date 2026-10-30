# Wiring the handheld (breadboard first)

Build it up one part at a time, and run the hardware test after each step:
screen first, then buttons, then sound. That way, if something doesn't work,
you know which wires to look at.

Pin numbers below are the **GP labels** printed on the RP2350-Plus, with the
physical pin number (counting from the USB end, 1–20 down the left side and
21–40 back up the right) in brackets. The RP2350-Plus uses the same layout as
a regular Pico, but **trust the labels printed on your board** over this table.

Code side: the pins live in `platform/pico/pins.h`. If you change a wire,
change it there too.

## 0. Before you start

- Solder header pins onto the RP2350-Plus (long side down) and push it into
  the breadboard across the center gap.
- Run one breadboard rail as **GND** from a GND pin (pin 38) and one as
  **3V3** from the 3V3 pin (pin 36, sometimes labeled 3V3(OUT)). Not 3V3_EN
  (pin 37): that one switches the board's power off when pulled low.
- Power everything from USB-C while testing. Leave the battery unplugged
  for now. Before you do plug it in, check its red wire goes to **+** on the
  board (battery plugs are not all wired the same way).

## 1. The screen

| Screen pin | Goes to | Notes |
|---|---|---|
| VCC | 3V3 rail | |
| GND | GND rail | |
| CS | GP17 (22) | |
| RESET | GP20 (26) | |
| DC | GP16 (21) | |
| SDI (MOSI) | GP19 (25) | |
| SCK | GP18 (24) | |
| LED | GP21 (27) | backlight |
| SDO (MISO) | nothing | |
| T_CLK, T_CS, T_DIN, T_DO, T_IRQ | nothing | touch, unused |

The SD card pins on the screen's other edge stay unconnected. Leave jumper J1
alone for now. It only needs shorting if the screen is flaky at 3.3 V, and it
must **never** be shorted if VCC goes to 5 V.

Keep these wires short (under ~15 cm) if you can. The screen data runs fast.

Flash the test (see below). You should see:

- A red square in the **top-left** corner, green top-right, blue bottom-left,
  white bottom-right.
- Color bars labeled RED GRN BLU YEL CYN MAG WHT, each matching its label.
- A smooth gray ramp and a small pink block sliding back and forth.
- At the bottom: the SPI speed, how long a frame takes to send, and frames
  per second.

| What you see | What to do |
|---|---|
| Nothing, backlight off | Check VCC, GND and LED |
| Backlight on, plain white, or striped garbage | Check CS, DC, RESET, SDI, SCK. Then wire SDO to GP4: the test prints the chip's ID over USB. 85 85 52 (or 42 C2 A9) is an ST7789, 00 93 41 an ILI9341; set `LCD_ST7789` in pins.h to match |
| Picture mirrored or upside down | Change `LCD_MADCTL` in pins.h: try `0x28`, `0x68`, `0xA8`, `0xE8` (ST7789: `0x60`, `0x20`, `0xA0`, `0xE0`); one of them is right |
| Red and blue swapped | Add or remove `0x08` in `LCD_MADCTL` (e.g. `0x28` becomes `0x20`) |
| Colors inverted (like a photo negative) | Wrong controller setting: flip `LCD_ST7789` |
| Stripes, speckles or flicker | Bits are getting garbled on the wires. The test starts at a slow 10 MHz; Y (or a jumper from GP15 to GND, before the buttons are wired) steps up through 20, 37.5 and 75 MHz. Note the fastest speed that stays clean. Shorter wires, firm connections on SCK and SDI, and a second GND wire all help |

## 2. The buttons

Each button goes between its pin and the GND rail; no resistors needed. A
4-legged tactile button has two pairs of legs that are always connected, so
use two **diagonally opposite** legs and it's guaranteed to switch.

| Button | Pin |
|---|---|
| Up | GP0 (1) |
| Down | GP1 (2) |
| Left | GP2 (4) |
| Right | GP3 (5) |
| D-pad press (optional) | GP6 (9) |
| A | GP12 (16) |
| B | GP13 (17) |
| X | GP14 (19) |
| Y | GP15 (20) |

In the test, each button's box lights green while you hold it. If the wrong
box lights, two wires are swapped. If a box is lit without touching anything,
that button's legs are the always-connected pair: turn it 90°.

## 3. Sound

| Amp (MAX98357A) pin | Goes to |
|---|---|
| VIN | VSYS (39) |
| GND | GND rail |
| DIN | GP9 (12) |
| BCLK | GP10 (14) |
| LRC | GP11 (15) |
| SD | GP8 (11) |
| GAIN | nothing |

Speaker wires go to the amp's **+** and **−** terminals, never to GND.

In the test, every button plays its own note while held. If there's no
sound, check DIN, BCLK and LRC first; LRC has to be on the pin right after
BCLK.

## Building and flashing

Once the VS Code extension has installed the tools (see README):

```
python tools/pico.py flash hw_test
```

The first time, the Pico needs to be in BOOT mode: hold the **BOOT** button
while plugging in USB-C (a drive called RP2350 shows up), then run the
command. After that, the same command reloads it without touching the button.

The test also prints each button press and the frame rate over USB. To see
it, open the Serial Monitor in VS Code (part of the Pico extension).
