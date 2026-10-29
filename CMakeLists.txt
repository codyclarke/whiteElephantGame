cmake_minimum_required(VERSION 3.20)
project(StealNight C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Art: every PNG under assets/ is converted to C++ data on each build.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(GLOB_RECURSE SPRITE_PNGS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/*.png)
set(SPRITE_CPP ${CMAKE_CURRENT_BINARY_DIR}/generated/sprites_data.cpp)
add_custom_command(
    OUTPUT ${SPRITE_CPP}
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tools/png2sprite.py
            ${CMAKE_CURRENT_SOURCE_DIR}/assets ${SPRITE_CPP}
    DEPENDS ${SPRITE_PNGS} ${CMAKE_CURRENT_SOURCE_DIR}/tools/png2sprite.py
    COMMENT "Converting sprites from assets/"
)

# Sound: every WAV under assets/ (sounds/ and music/) is converted too.
file(GLOB_RECURSE SOUND_WAVS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/*.wav)
set(SOUND_CPP ${CMAKE_CURRENT_BINARY_DIR}/generated/sounds_data.cpp)
add_custom_command(
    OUTPUT ${SOUND_CPP}
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/tools/wav2sound.py
            ${CMAKE_CURRENT_SOURCE_DIR}/assets ${SOUND_CPP}
    DEPENDS ${SOUND_WAVS} ${CMAKE_CURRENT_SOURCE_DIR}/tools/wav2sound.py
    COMMENT "Converting sounds from assets/"
)

# Platform-independent game code. Shared by the desktop and handheld builds.
add_library(game STATIC
    game/ai.cpp
    game/audio.cpp
    game/buzz.cpp
    game/game.cpp
    game/gfx.cpp
    game/party.cpp
    game/scratch.cpp
    game/snow.cpp
    game/sprites.cpp
    ${SPRITE_CPP}
    ${SOUND_CPP}
)
target_include_directories(game PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
    target_compile_options(game PRIVATE /W4 /utf-8)
else()
    target_compile_options(game PRIVATE -Wall -Wextra)
endif()

# Desktop build: SDL2 is downloaded and built automatically on first configure.
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(SDL2
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz
)
FetchContent_MakeAvailable(SDL2)

add_executable(steal_night platform/sdl/main_sdl.cpp)
target_link_libraries(steal_night PRIVATE game SDL2::SDL2-static)

# Rules test: plays thousands of parties and checks the white elephant rules.
# It never draws or beeps, so a do-nothing platform stub is enough.
add_executable(party_test tests/party_test.cpp tests/platform_stub.cpp)
target_link_libraries(party_test PRIVATE game)
enable_testing()
add_test(NAME party_rules COMMAND party_test)

add_executable(audio_test tests/audio_test.cpp tests/platform_stub.cpp)
target_link_libraries(audio_test PRIVATE game)
add_test(NAME audio_mixer COMMAND audio_test)

add_executable(scratch_test tests/scratch_test.cpp tests/platform_stub.cpp)
target_link_libraries(scratch_test PRIVATE game)
add_test(NAME scratch_tickets COMMAND scratch_test)

add_executable(snow_test tests/snow_test.cpp tests/platform_stub.cpp)
target_link_libraries(snow_test PRIVATE game)
add_test(NAME snow_scene COMMAND snow_test)
