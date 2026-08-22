# Pico 2 firmware bring-up (milestone 3, just started - see CLAUDE.md's
# "Deferred work"). This is its OWN standalone top-level CMake project,
# not something the repo-root CMakeLists.txt add_subdirectory()s: the
# Pico SDK's board/toolchain selection has to happen before this file's
# project() call, which a shared root project (already configured for
# the host compiler, building core/tests/harness) can't retroactively
# hand off to a different cross-compiler mid-tree. soynut's own firmware/
# is built the same standalone way, for the same reason - see its
# CMakeLists.txt.
#
# Build with:
#   cmake -S firmware -B firmware/build -DPICO_SDK_PATH=/path/to/pico-sdk
#   cmake --build firmware/build
# (or set the PICO_SDK_PATH environment variable instead of the -D flag).
# Requires the arm-none-eabi GCC toolchain (e.g. `brew install
# --cask gcc-arm-embedded` or the arm-none-eabi-gcc package for your OS)
# and a local checkout of https://github.com/raspberrypi/pico-sdk.

cmake_minimum_required(VERSION 3.13)

# Pico 2 = RP2350. Override with -DPICO_BOARD=... on the command line if
# bringing this up on a different board first. Must be set before
# project() - the SDK resolves the board/platform during that call.
set(PICO_BOARD pico2 CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(vger_firmware C CXX ASM)

set(CMAKE_C_STANDARD 17) # matches core/'s and harness/'s C17, see ../CLAUDE.md

pico_sdk_init()

set(VGER_STRICT_WARNING_FLAGS
    -Wall -Wextra -Wpedantic -Werror
    -Wshadow -Wconversion -Wsign-conversion
    -Wundef -Wcast-qual -Wpointer-arith
    -Wwrite-strings -Wredundant-decls
    -Wmissing-prototypes -Wstrict-prototypes
)

# core/ is platform-independent by design (see ../core/CMakeLists.txt's
# own header comment) - reused here unmodified, cross-compiled for
# RP2350 instead of the host, to prove that claim true rather than just
# asserted. VGER_ENABLE_STRICT_WARNINGS/VGER_STRICT_WARNING_FLAGS are the
# same two variables ../CMakeLists.txt defines for the desktop build;
# this standalone project tree has to define its own copy since it never
# includes that root file - see the header comment above for why.
set(VGER_ENABLE_STRICT_WARNINGS ON)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../core ${CMAKE_CURRENT_BINARY_DIR}/core)

add_executable(vger_firmware
    main.c
    st7920.c
)

target_include_directories(vger_firmware PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vger_firmware PRIVATE vger_core pico_stdlib hardware_gpio)

# Scoped to this project's own sources only (never target-wide), same
# rationale as ../core/CMakeLists.txt and ../harness/CMakeLists.txt -
# pico-sdk's own sources compiled into this target must never inherit
# these flags. See ../DEVIATIONS.md.
set_source_files_properties(main.c st7920.c PROPERTIES
    COMPILE_OPTIONS "${VGER_STRICT_WARNING_FLAGS}"
)

pico_add_extra_outputs(vger_firmware)
