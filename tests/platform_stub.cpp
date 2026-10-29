// Do-nothing platform for tests: no screen, no buttons, no sound device.
#include "platform/platform.h"

namespace platform {

uint16_t buttons() { return 0; }
uint32_t millis() { return 0; }
void present(const uint16_t*) {}
void set_backlight(bool) {}
bool save_data(const void*, uint32_t) { return false; }
uint32_t load_data(void*, uint32_t) { return 0; }
void audio_lock() {}
void audio_unlock() {}

}  // namespace platform
