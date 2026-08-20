# FOCAL core: platform-independent, zero hardware/SDK dependencies.
# Buildable standalone on desktop and linked into Pico 2 firmware later.

add_library(vger_core STATIC
    vger_assert.c
    vger_program.c
    vger_state.c
    vger_mode_policy.c
    vger_interp.c
)

target_include_directories(vger_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(vger_core PUBLIC c_std_17)

if(VGER_ENABLE_STRICT_WARNINGS)
    set(VGER_CORE_SOURCES
        vger_assert.c
        vger_program.c
        vger_state.c
        vger_mode_policy.c
        vger_interp.c
    )
    # Scoped to this project's own sources only (never target-wide), so a
    # future vendored dependency (e.g. pico-sdk) added to this build never
    # inherits these flags. See ../DEVIATIONS.md and soynut's CMakeLists.txt
    # for the lesson this avoids repeating.
    set_source_files_properties(${VGER_CORE_SOURCES} PROPERTIES
        COMPILE_OPTIONS "${VGER_STRICT_WARNING_FLAGS}"
    )
endif()
