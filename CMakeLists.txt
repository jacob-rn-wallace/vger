cmake_minimum_required(VERSION 3.20)
project(vger C)

option(VGER_ENABLE_STRICT_WARNINGS "Enable pedantic warnings-as-errors on this project's own sources" ON)
option(VGER_BUILD_HARNESS "Build the desktop SDL2 test harness" ON)
option(VGER_BUILD_FIRMWARE "Build the Pico 2 firmware target" OFF)

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

if(VGER_BUILD_FIRMWARE)
    add_subdirectory(firmware)
endif()
