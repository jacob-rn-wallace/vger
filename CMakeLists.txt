cmake_minimum_required(VERSION 3.20)
project(vger C)

option(VGER_ENABLE_STRICT_WARNINGS "Enable pedantic warnings-as-errors on this project's own sources" ON)
option(VGER_BUILD_HARNESS "Build the desktop SDL2 test harness" ON)

# The Pico 2 firmware target (firmware/) is NOT add_subdirectory()'d from
# here - it's its own standalone top-level CMake project, built via
# `cmake -S firmware -B firmware/build`. Reason: the Pico SDK's board/
# toolchain selection has to happen before firmware/CMakeLists.txt's own
# project() call, which this already-configured-for-the-host-compiler
# root project can't hand off to mid-tree. See firmware/CMakeLists.txt's
# header comment.

set(VGER_STRICT_WARNING_FLAGS
    -Wall -Wextra -Wpedantic -Werror
    -Wshadow -Wconversion -Wsign-conversion
    -Wundef -Wcast-qual -Wpointer-arith
    -Wwrite-strings -Wredundant-decls
    -Wmissing-prototypes -Wstrict-prototypes
)

find_program(VGER_CLANG_TIDY NAMES clang-tidy)
if(VGER_CLANG_TIDY AND VGER_ENABLE_STRICT_WARNINGS)
    set(CMAKE_C_CLANG_TIDY "${VGER_CLANG_TIDY}")
    message(STATUS "vger: clang-tidy found, wired into the build (${VGER_CLANG_TIDY})")
else()
    message(STATUS "vger: clang-tidy not found - static analysis is not active this build. "
                    "Install it (e.g. `brew install llvm` and add it to PATH) to enable it.")
endif()

enable_testing()

add_subdirectory(core)
add_subdirectory(tests)

if(VGER_BUILD_HARNESS)
    add_subdirectory(harness)
endif()
