cmake_minimum_required(VERSION 3.20)
project(orderbook-engine
    VERSION 0.1.0
    DESCRIPTION "High-performance limit order book and matching engine"
    LANGUAGES CXX
)

# ──────────────────────────────────────────────
# Global settings
# ──────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warnings — treat code quality seriously
add_compile_options(
    -Wall -Wextra -Wpedantic -Wshadow
    -Wnon-virtual-dtor -Woverloaded-virtual
    -Wconversion -Wsign-conversion
    -Wnull-dereference -Wdouble-promotion
    -Wformat=2
)

# ──────────────────────────────────────────────
# Core library
# ──────────────────────────────────────────────
add_library(orderbook_core
    src/core/order.cpp
    src/core/price_level.cpp
    src/core/order_book.cpp
)

target_include_directories(orderbook_core
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)

# ──────────────────────────────────────────────
# Matching engine library
# ──────────────────────────────────────────────
add_library(orderbook_matching
    src/matching/matching_engine.cpp
)

target_link_libraries(orderbook_matching
    PUBLIC orderbook_core
)

# ──────────────────────────────────────────────
# Tests (Google Test)
# ──────────────────────────────────────────────
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
    enable_testing()

    add_executable(orderbook_tests
        tests/unit/order_test.cpp
        tests/unit/price_level_test.cpp
        tests/unit/order_book_test.cpp
        tests/unit/matching_engine_test.cpp
    )

    target_link_libraries(orderbook_tests
        PRIVATE orderbook_matching
        PRIVATE GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(orderbook_tests)
endif()

# ──────────────────────────────────────────────
# Benchmarks
# ──────────────────────────────────────────────
option(BUILD_BENCHMARKS "Build benchmarks" OFF)

if(BUILD_BENCHMARKS)
    add_executable(orderbook_bench
        benchmarks/throughput_bench.cpp
    )

    target_link_libraries(orderbook_bench
        PRIVATE orderbook_core
        PRIVATE orderbook_matching
    )
endif()
