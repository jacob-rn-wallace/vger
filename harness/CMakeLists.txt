# Desktop SDL2 test harness. Links vger_core but is not itself part of it -
# vger_core has zero dependency on SDL2 or anything else in this file.

find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
endif()

add_executable(vger_harness
    main.c
    vger_sevenseg.c
    vger_keymap.c
)

target_include_directories(vger_harness PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_directories(vger_harness PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(vger_harness PRIVATE vger_core ${SDL2_LIBRARIES})
target_compile_options(vger_harness PRIVATE ${SDL2_CFLAGS_OTHER})

target_compile_features(vger_harness PRIVATE c_std_17)

if(VGER_ENABLE_STRICT_WARNINGS)
    set(VGER_HARNESS_SOURCES main.c vger_sevenseg.c vger_keymap.c)
    # Scoped to this project's own sources only - SDL2 is a system library
    # here, not vendored source compiled into this target, but this stays
    # file-scoped rather than target-wide so it's already correct once
    # firmware/ starts pulling in pico-sdk sources. See ../DEVIATIONS.md.
    set_source_files_properties(${VGER_HARNESS_SOURCES} PROPERTIES
        COMPILE_OPTIONS "${VGER_STRICT_WARNING_FLAGS}"
    )
endif()
