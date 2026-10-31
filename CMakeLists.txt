cmake_minimum_required(VERSION 3.20)
project(StealNight C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The game itself (shared with the handheld build in platform/pico/).
include(game/game.cmake)

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
