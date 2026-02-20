cmake_minimum_required(VERSION 3.16)
project(CacheCraft VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Options ---
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif()

# --- Dependencies ---
find_package(Threads REQUIRED)

# Asio (header-only, standalone)
if(APPLE)
    execute_process(
        COMMAND brew --prefix
        OUTPUT_VARIABLE HOMEBREW_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

find_path(ASIO_INCLUDE_DIR asio.hpp
    HINTS
        ${HOMEBREW_PREFIX}/include
        /usr/include
        /usr/local/include
)

if(NOT ASIO_INCLUDE_DIR)
    message(FATAL_ERROR "Asio not found. Install: brew install asio (macOS) or apt install libasio-dev (Ubuntu)")
endif()
message(STATUS "Asio found at: ${ASIO_INCLUDE_DIR}")

# Catch2 v3 — prefer system install, fallback to FetchContent
find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
    message(STATUS "Catch2 v3 not found locally — fetching via FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.2
    )
    FetchContent_MakeAvailable(Catch2)
endif()

# --- Compiler warnings ---
add_compile_options(-Wall -Wextra -Wpedantic)

# --- Common library ---
add_library(cachecraft_common STATIC
    src/common/protocol.cpp
    src/store/lru_shard.cpp
    src/store/cache_store.cpp
)
target_include_directories(cachecraft_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${ASIO_INCLUDE_DIR}
)
target_link_libraries(cachecraft_common PUBLIC Threads::Threads)
target_compile_definitions(cachecraft_common PUBLIC ASIO_STANDALONE)

# --- Server library (for test reuse) ---
add_library(cachecraft_server STATIC
    src/server/server.cpp
    src/server/session.cpp
)
target_link_libraries(cachecraft_server PUBLIC cachecraft_common)

# --- Server binary ---
add_executable(cachecraftd src/server/main.cpp)
target_link_libraries(cachecraftd PRIVATE cachecraft_server)

# --- Client binary ---
add_executable(cachecraft-cli src/client/main.cpp)
target_link_libraries(cachecraft-cli PRIVATE cachecraft_common)

# --- Benchmark binary ---
add_executable(cachecraft-bench src/bench/main.cpp)
target_link_libraries(cachecraft-bench PRIVATE cachecraft_common)

# --- Tests ---
enable_testing()
add_executable(cachecraft-tests
    tests/test_protocol.cpp
    tests/test_lru.cpp
    tests/test_store.cpp
    tests/test_ttl.cpp
    tests/test_integration.cpp
)
target_link_libraries(cachecraft-tests PRIVATE cachecraft_server Catch2::Catch2WithMain)
add_test(NAME cachecraft-tests COMMAND cachecraft-tests)
