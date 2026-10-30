#pragma once
#include "pico/types.h"

// Which Pico pin each part is wired to. Matches WIRING.md -- change both
// together. The screen and amp numbers are GP numbers, not physical pin numbers.
namespace pins {

// 2.8" SPI screen (SPI0)
constexpr uint LCD_SCK = 18;
constexpr uint LCD_MOSI = 19;  // "SDI (MOSI)" on the screen board
constexpr uint LCD_CS = 17;
constexpr uint LCD_DC = 16;
constexpr uint LCD_RESET = 20;
constexpr uint LCD_LED = 21;  // backlight, PWM
constexpr uint LCD_MISO = 4;  // "SDO (MISO)": optional, only for reading the screen's settings back

// The screen's controller chip. Ours reported an ST7789V (ID 85 85 52) when
// asked. Many red 2.8" boards are ILI9341 instead; hw_test reads the chip's
// ID if SDO is wired to GP4.
constexpr bool LCD_ST7789 = true;

// Which way up the picture is. If the test image is mirrored or upside down,
// see the table in WIRING.md.
constexpr uint8_t LCD_MADCTL = LCD_ST7789 ? 0x60 : 0x28;

// SPI speed for the game. 75 MHz (a full frame in about 16 ms) was clean even
// on the breadboard; hw_test's Y button checks other speeds.
constexpr uint LCD_SPI_HZ = 75'000'000;

// I2S amp (MAX98357A). LRC must be BCLK + 1.
constexpr uint AMP_DIN = 9;
constexpr uint AMP_BCLK = 10;
constexpr uint AMP_LRC = 11;
constexpr uint AMP_SD = 8;  // shutdown: pulled low = amp off

// Buttons: each one connects its pin to GND when pressed.
constexpr uint BTN_UP = 0;
constexpr uint BTN_DOWN = 1;
constexpr uint BTN_LEFT = 2;
constexpr uint BTN_RIGHT = 3;
constexpr uint BTN_PRESS = 6;
constexpr uint BTN_A = 12;
constexpr uint BTN_B = 13;
constexpr uint BTN_X = 14;
constexpr uint BTN_Y = 15;

}  // namespace pins
