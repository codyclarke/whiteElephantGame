#pragma once
#include <cstdint>

#include "pico/types.h"

// The 2.8" 320x240 SPI screen. Frames go out by DMA, so the Pico can work on
// the next frame while the last one is still being sent.
namespace lcd {

void init();  // also sets the SPI speed to pins::LCD_SPI_HZ
void set_backlight(uint8_t level);  // 0 = off, 255 = full

// SPI clock. The Pico can only divide its clock by even numbers, so the speed
// you get may be lower than asked; spi_hz() says what it really is.
void set_spi_hz(uint hz);
uint spi_hz();

// Start sending a full 320x240 RGB565 frame. Returns right away; the pixels
// must not change until wait() returns.
void start_frame(const uint16_t* pixels);
void wait();

// For troubleshooting: send a read command and collect the bytes the screen
// sends back (needs the SDO wire). Runs at a slow speed, then restores it.
void read(uint8_t cmd, uint8_t* out, int count);

}  // namespace lcd
