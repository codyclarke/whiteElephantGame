#include "platform/pico/lcd.h"

#include <initializer_list>

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "platform/pico/pins.h"

namespace lcd {
namespace {

constexpr int WIDTH = 320;
constexpr int HEIGHT = 240;
spi_inst_t* const SPI = spi0;

int dma_channel = -1;
bool sending = false;
uint actual_hz = 0;

void command(uint8_t cmd, const uint8_t* data = nullptr, size_t len = 0) {
    gpio_put(pins::LCD_CS, 0);
    gpio_put(pins::LCD_DC, 0);
    spi_write_blocking(SPI, &cmd, 1);
    if (len) {
        gpio_put(pins::LCD_DC, 1);
        spi_write_blocking(SPI, data, len);
    }
    gpio_put(pins::LCD_CS, 1);
}

// Startup commands: command byte, number of data bytes, the data, and an
// optional pause. A count with 0x80 set means "then wait 150 ms".
constexpr uint8_t DELAY = 0x80;

constexpr uint8_t ILI9341_INIT[] = {
    0x01, DELAY,                         // software reset
    0x11, DELAY,                         // wake up
    0xC0, 1, 0x23,                       // power control
    0xC1, 1, 0x10,
    0xC5, 2, 0x3E, 0x28,                 // VCOM
    0xC7, 1, 0x86,
    0x3A, 1, 0x55,                       // 16 bits per pixel
    0xB1, 2, 0x00, 0x18,                 // frame rate
    0xB6, 3, 0x08, 0x82, 0x27,           // display function
    0x26, 1, 0x01,                       // gamma curve
    0xE0, 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
    0xE1, 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
    0x29, DELAY,                         // display on
};

constexpr uint8_t ST7789_INIT[] = {
    0x01, DELAY,                         // software reset
    0x11, DELAY,                         // wake up
    0x3A, 1, 0x55,                       // 16 bits per pixel
    0x13, 0,                             // normal mode
    0x29, DELAY,                         // display on
};

void run(const uint8_t* list, size_t size) {
    for (size_t i = 0; i < size;) {
        uint8_t cmd = list[i++];
        uint8_t count = list[i++];
        bool pause = count & DELAY;
        count &= ~DELAY;
        command(cmd, &list[i], count);
        i += count;
        if (pause) sleep_ms(150);
    }
}

// Orientation and the drawing window (the whole screen). Sent before every
// frame too, so if one of these commands ever gets garbled on the wires, the
// next frame puts it right.
void set_window() {
    command(0x36, &pins::LCD_MADCTL, 1);
    command(pins::LCD_INVERT ? 0x21 : 0x20);  // color inversion on / off
    const uint8_t cols[] = {0, 0, (WIDTH - 1) >> 8, (WIDTH - 1) & 0xFF};
    const uint8_t rows[] = {0, 0, (HEIGHT - 1) >> 8, (HEIGHT - 1) & 0xFF};
    command(0x2A, cols, 4);
    command(0x2B, rows, 4);
}

}  // namespace

void init() {
    for (uint pin : {pins::LCD_CS, pins::LCD_DC, pins::LCD_RESET}) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 1);
    }

    // Backlight starts off so the screen doesn't flash garbage.
    gpio_set_function(pins::LCD_LED, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pins::LCD_LED);
    pwm_set_wrap(slice, 255);
    pwm_set_gpio_level(pins::LCD_LED, 0);
    pwm_set_enabled(slice, true);

    // Start slow; set_spi_hz() speeds it up once the wiring is known to cope.
    actual_hz = spi_init(SPI, 10'000'000);
    gpio_set_function(pins::LCD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(pins::LCD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(pins::LCD_MISO, GPIO_FUNC_SPI);
    gpio_pull_up(pins::LCD_MISO);  // reads 0xFF if the wire isn't there

    gpio_put(pins::LCD_RESET, 0);
    sleep_ms(20);
    gpio_put(pins::LCD_RESET, 1);
    sleep_ms(120);

    if (pins::LCD_ST7789) {
        run(ST7789_INIT, sizeof ST7789_INIT);
    } else {
        run(ILI9341_INIT, sizeof ILI9341_INIT);
    }
    set_window();
    actual_hz = spi_set_baudrate(SPI, pins::LCD_SPI_HZ);

    dma_channel = dma_claim_unused_channel(true);
}

void set_backlight(uint8_t level) { pwm_set_gpio_level(pins::LCD_LED, level); }

void set_spi_hz(uint hz) {
    wait();
    actual_hz = spi_set_baudrate(SPI, hz);
}

uint spi_hz() { return actual_hz; }

void start_frame(const uint16_t* pixels) {
    wait();
    set_window();
    gpio_put(pins::LCD_CS, 0);
    gpio_put(pins::LCD_DC, 0);
    const uint8_t write = 0x2C;  // memory write, from the top-left of the window
    spi_write_blocking(SPI, &write, 1);
    gpio_put(pins::LCD_DC, 1);

    // 16-bit SPI words send each pixel high byte first, which is what the
    // screen wants.
    spi_set_format(SPI, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    dma_channel_config c = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_dreq(&c, spi_get_dreq(SPI, true));
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    dma_channel_configure(dma_channel, &c, &spi_get_hw(SPI)->dr, pixels, WIDTH * HEIGHT, true);
    sending = true;
}

void read(uint8_t cmd, uint8_t* out, int count) {
    wait();
    uint fast = actual_hz;
    spi_set_baudrate(SPI, 1'000'000);
    gpio_put(pins::LCD_CS, 0);
    gpio_put(pins::LCD_DC, 0);
    spi_write_blocking(SPI, &cmd, 1);
    gpio_put(pins::LCD_DC, 1);
    spi_read_blocking(SPI, 0x00, out, count);
    gpio_put(pins::LCD_CS, 1);
    actual_hz = spi_set_baudrate(SPI, fast);
}

void wait() {
    if (!sending) return;
    dma_channel_wait_for_finish_blocking(dma_channel);
    while (spi_is_busy(SPI)) {
    }
    // Throw away what the screen "sent back" while we were writing.
    while (spi_is_readable(SPI)) (void)spi_get_hw(SPI)->dr;
    spi_get_hw(SPI)->icr = SPI_SSPICR_RORIC_BITS;
    spi_set_format(SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_put(pins::LCD_CS, 1);
    sending = false;
}

}  // namespace lcd
