cmake_minimum_required(VERSION 3.20)
project(Nexos VERSION 1.0.0 LANGUAGES CXX)

if(NOT UNIX)
    message(FATAL_ERROR "Nexos currently requires a Unix-like operating system.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(nexos STATIC
    src/nexos.cpp
    src/file.cpp
    src/pipe.cpp
    src/process.cpp
)
target_include_directories(nexos PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_compile_options(nexos PRIVATE -Wall -Wextra -Wpedantic)

add_executable(nex apps/nex/main.cpp)
target_link_libraries(nex PRIVATE nexos)
target_compile_options(nex PRIVATE -Wall -Wextra -Wpedantic)

add_executable(nexps apps/nexps/main.cpp)
target_compile_options(nexps PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
    add_executable(nexos_tests tests/test_nexos.cpp)
    target_link_libraries(nexos_tests PRIVATE nexos)
    add_test(NAME NexosTests COMMAND nexos_tests)

    add_executable(nex_shell_tests tests/test_shell.cpp)
    add_test(
        NAME NexosShellTests
        COMMAND nex_shell_tests $<TARGET_FILE:nex>
    )
endif()

install(TARGETS nexos nex nexps)
install(DIRECTORY include/ DESTINATION include)
