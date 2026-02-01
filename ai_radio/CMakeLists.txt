cmake_minimum_required(VERSION 3.16)
project(keegan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN -DNOMINMAX)

include_directories(src)
include_directories(vendor)
include_directories(vendor/vjson)

# Keegan core sources
set(KEEGAN_SOURCES
    src/main.cpp
    src/audio/reverb.cpp
    src/audio/ducking.cpp
    src/audio/scheduler.cpp
    src/audio/engine.cpp
    src/audio/device.cpp
    src/audio/limiter.cpp
    src/audio/stem_player.cpp
    src/ui/tray.cpp
    src/ui/web_server.cpp
    src/ui/ws_server.cpp
    src/util/logger.cpp
    src/util/platform.cpp
    src/brain/app_heuristics.cpp
    src/brain/state_machine.cpp
    src/brain/story_generator.cpp
    src/config/mood_loader.cpp
    src/voice/story_bank.cpp
    vendor/vjson/vjson.cpp
)

add_executable(keegan_patched WIN32 ${KEEGAN_SOURCES})

# Link Windows libraries for tray and process detection
if(WIN32)
    target_link_libraries(keegan_patched PRIVATE
        user32
        shell32
        gdi32
        ole32
        Psapi
        ws2_32
    )
endif()

# LLM router disabled for now - can be built separately
# if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/llm_router/CMakeLists.txt")
#     add_subdirectory(llm_router)
# endif()
