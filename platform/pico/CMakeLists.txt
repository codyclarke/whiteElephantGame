# Handheld (Pico) build. A separate project from the desktop build at the top
# level, because it uses the ARM compiler and the Pico SDK.
# Build it with: python tools/pico.py build
cmake_minimum_required(VERSION 3.13)

# The board the Pico SDK sets up for: the Waveshare RP2350-Plus (RP2350A,
# battery sensing on GP29). It comes with 4 MB or 16 MB of flash; the 4 MB
# setting runs on both. Switch to waveshare_rp2350_plus_16mb for the room,
# then delete build-pico/ so it takes effect.
set(PICO_BOARD waveshare_rp2350_plus_4mb CACHE STRING "Pico board")
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH} CACHE PATH "Pico SDK folder")
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(StealNightPico C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
pico_sdk_init()

set(ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

# The game itself, the same code the desktop build runs.
include(${ROOT}/game/game.cmake)

# The game.
add_executable(steal_night
    main_pico.cpp
    lcd.cpp
    audio_out.cpp
)
pico_generate_pio_header(steal_night ${CMAKE_CURRENT_LIST_DIR}/i2s.pio)
target_include_directories(steal_night PRIVATE ${ROOT})
target_link_libraries(steal_night PRIVATE game pico_stdlib pico_flash pico_rand
                      hardware_spi hardware_dma hardware_pio hardware_pwm hardware_flash)
pico_enable_stdio_usb(steal_night 1)
pico_enable_stdio_uart(steal_night 0)
pico_add_extra_outputs(steal_night)

# Wiring check: screen, buttons, speaker.
add_executable(hw_test
    hw_test.cpp
    lcd.cpp
    audio_out.cpp
    ${ROOT}/game/gfx.cpp
)
pico_generate_pio_header(hw_test ${CMAKE_CURRENT_LIST_DIR}/i2s.pio)
target_include_directories(hw_test PRIVATE ${ROOT})
target_link_libraries(hw_test PRIVATE pico_stdlib hardware_spi hardware_dma hardware_pio hardware_pwm)
pico_enable_stdio_usb(hw_test 1)
pico_enable_stdio_uart(hw_test 0)
pico_add_extra_outputs(hw_test)
