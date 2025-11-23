cmake_minimum_required(VERSION 3.16)
project(keegan LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-D_CRT_SECURE_NO_WARNINGS -DWIN32_LEAN_AND_MEAN)

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
    src/util/logger.cpp
    src/brain/app_heuristics.cpp
    src/brain/state_machine.cpp
    src/config/mood_loader.cpp
    vendor/vjson/vjson.cpp
)

add_executable(keegan ${KEEGAN_SOURCES})
target_link_libraries(keegan PRIVATE)

# Bring in the LLM router as a sub-project (kept building separately).
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/llm_router/CMakeLists.txt")
    add_subdirectory(llm_router)
endif()
