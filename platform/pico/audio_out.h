#pragma once
#include <cstdint>

// Sound out through the I2S amp. Runs in the background: whenever a buffer
// has played, an interrupt asks `fill` for the next batch of samples.
namespace audio_out {

using FillFn = void (*)(int16_t* out, int count);  // mono, signed 16-bit

void start(int sample_rate, FillFn fill);
void set_amp(bool on);  // off = the amp's shutdown pin, for sleep

// Keep fill() from running while the main loop changes what it reads.
void lock();
void unlock();

}  // namespace audio_out
