cmake_minimum_required(VERSION 3.10)
project(llm_router LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-D_WIN32_WINNT=0x0A00 -DWIN32_LEAN_AND_MEAN)

include_directories(vendor)
include_directories(vendor/vjson)
include_directories(src)

# Source files
set(SOURCES
    src/main.cpp
    src/http/server.cpp
    src/config/loader.cpp
    src/routing/router.cpp
    src/logging/logger.cpp
    src/providers/openai_client.cpp
    vendor/vjson/vjson.cpp
)

# Headers
set(HEADERS
    src/http/server.h
    src/config/loader.h
    src/routing/router.h
    src/providers/base_client.h
    src/providers/openai_client.h
    src/logging/logger.h
    vendor/httplib.h
    vendor/vjson/vjson.h
)

if(WIN32)
    set(LIBS ws2_32 crypt32)
endif()

add_executable(llm_router ${SOURCES} ${HEADERS})
target_link_libraries(llm_router ${LIBS})
