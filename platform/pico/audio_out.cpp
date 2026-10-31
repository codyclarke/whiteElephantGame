#include "platform/pico/audio_out.h"

#include <initializer_list>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "i2s.pio.h"
#include "platform/pico/pins.h"

namespace audio_out {
namespace {

// Two buffers take turns: one plays while the other is refilled. 256 samples
// is 16 ms at 16 kHz, plenty of time for the refill.
constexpr int BUFFER_SAMPLES = 256;
uint32_t buffers[2][BUFFER_SAMPLES];
int channels[2];
FillFn filler = nullptr;

void refill(int which) {
    int16_t mono[BUFFER_SAMPLES];
    filler(mono, BUFFER_SAMPLES);
    for (int i = 0; i < BUFFER_SAMPLES; ++i) {
        uint32_t s = static_cast<uint16_t>(mono[i]);
        buffers[which][i] = (s << 16) | s;  // same sound on both channels
    }
}

void on_buffer_done() {
    for (int which = 0; which < 2; ++which) {
        if (!dma_channel_get_irq0_status(channels[which])) continue;
        dma_channel_acknowledge_irq0(channels[which]);
        refill(which);
        // Rewind it, ready for when the other channel hands back to it.
        dma_channel_set_read_addr(channels[which], buffers[which], false);
    }
}

}  // namespace

void start(int sample_rate, FillFn fill) {
    filler = fill;

    // The amp's SD pin has a pull-up on the breakout; letting it float keeps
    // the amp on and mixing both channels.
    gpio_init(pins::AMP_SD);
    set_amp(true);

    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &i2s_out_program);
    pio_sm_config c = i2s_out_program_get_default_config(offset);
    sm_config_set_out_pins(&c, pins::AMP_DIN, 1);
    sm_config_set_sideset_pins(&c, pins::AMP_BCLK);
    sm_config_set_out_shift(&c, false, true, 32);  // most significant bit first, auto-refill
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&c, static_cast<float>(clock_get_hz(clk_sys)) / (sample_rate * 64.0f));
    for (uint pin : {pins::AMP_DIN, pins::AMP_BCLK, pins::AMP_LRC}) pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pins::AMP_DIN, 1, true);
    pio_sm_set_consecutive_pindirs(pio, sm, pins::AMP_BCLK, 2, true);
    pio_sm_init(pio, sm, offset + i2s_out_offset_entry_point, &c);

    // Two DMA channels feed the state machine, each starting the other when
    // it finishes, so the sound never stops.
    channels[0] = dma_claim_unused_channel(true);
    channels[1] = dma_claim_unused_channel(true);
    for (int which = 0; which < 2; ++which) {
        refill(which);
        dma_channel_config dc = dma_channel_get_default_config(channels[which]);
        channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
        channel_config_set_read_increment(&dc, true);
        channel_config_set_write_increment(&dc, false);
        channel_config_set_dreq(&dc, pio_get_dreq(pio, sm, true));
        channel_config_set_chain_to(&dc, channels[1 - which]);
        dma_channel_configure(channels[which], &dc, &pio->txf[sm], buffers[which], BUFFER_SAMPLES, false);
        dma_channel_set_irq0_enabled(channels[which], true);
    }
    irq_set_exclusive_handler(DMA_IRQ_0, on_buffer_done);
    irq_set_enabled(DMA_IRQ_0, true);

    pio_sm_set_enabled(pio, sm, true);
    dma_channel_start(channels[0]);
}

void set_amp(bool on) {
    if (on) {
        gpio_set_dir(pins::AMP_SD, GPIO_IN);
    } else {
        gpio_set_dir(pins::AMP_SD, GPIO_OUT);
        gpio_put(pins::AMP_SD, 0);
    }
}

void lock() { irq_set_enabled(DMA_IRQ_0, false); }
void unlock() { irq_set_enabled(DMA_IRQ_0, true); }

}  // namespace audio_out
