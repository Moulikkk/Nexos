cmake_minimum_required(VERSION 3.20)

project(Nexos VERSION 0.1.0 LANGUAGES CXX)

# C++ configuration
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build the static library
add_library(nexos STATIC src/nexos.cpp)

# Make public headers accessible to library users
target_include_directories(nexos PUBLIC ${PROJECT_SOURCE_DIR}/include)

# Build the mini shell
add_executable(nex apps/nex/main.cpp)

target_link_libraries(nex PRIVATE nexos)

# Download doctest
include(FetchContent)

FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.12
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(doctest)

# Enable testing
enable_testing()

add_executable(nexos_tests tests/test_nexos.cpp)

target_link_libraries(nexos_tests PRIVATE nexos doctest::doctest)

target_compile_definitions(nexos_tests PRIVATE DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN)

# Automatically register doctest test cases with CTest
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(nexos_tests)
