# The platform-independent game, as a library. Shared by the desktop build
# (CMakeLists.txt at the top) and the handheld build (platform/pico/).
# Needs Python with Pillow to convert assets/.

set(GAME_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Art: every PNG under assets/ is converted to C++ data on each build.
file(GLOB_RECURSE SPRITE_PNGS CONFIGURE_DEPENDS ${GAME_ROOT}/assets/*.png)
set(SPRITE_CPP ${CMAKE_CURRENT_BINARY_DIR}/generated/sprites_data.cpp)
add_custom_command(
    OUTPUT ${SPRITE_CPP}
    COMMAND Python3::Interpreter ${GAME_ROOT}/tools/png2sprite.py ${GAME_ROOT}/assets ${SPRITE_CPP}
    DEPENDS ${SPRITE_PNGS} ${GAME_ROOT}/tools/png2sprite.py
    COMMENT "Converting sprites from assets/"
)

# Sound: every WAV under assets/ (sounds/ and music/) is converted too.
file(GLOB_RECURSE SOUND_WAVS CONFIGURE_DEPENDS ${GAME_ROOT}/assets/*.wav)
set(SOUND_CPP ${CMAKE_CURRENT_BINARY_DIR}/generated/sounds_data.cpp)
add_custom_command(
    OUTPUT ${SOUND_CPP}
    COMMAND Python3::Interpreter ${GAME_ROOT}/tools/wav2sound.py ${GAME_ROOT}/assets ${SOUND_CPP}
    DEPENDS ${SOUND_WAVS} ${GAME_ROOT}/tools/wav2sound.py
    COMMENT "Converting sounds from assets/"
)

add_library(game STATIC
    ${GAME_ROOT}/game/ai.cpp
    ${GAME_ROOT}/game/audio.cpp
    ${GAME_ROOT}/game/buzz.cpp
    ${GAME_ROOT}/game/game.cpp
    ${GAME_ROOT}/game/gfx.cpp
    ${GAME_ROOT}/game/party.cpp
    ${GAME_ROOT}/game/scratch.cpp
    ${GAME_ROOT}/game/snow.cpp
    ${GAME_ROOT}/game/sprites.cpp
    ${SPRITE_CPP}
    ${SOUND_CPP}
)
target_include_directories(game PUBLIC ${GAME_ROOT})
if(MSVC)
    target_compile_options(game PRIVATE /W4 /utf-8)
else()
    target_compile_options(game PRIVATE -Wall -Wextra)
endif()
