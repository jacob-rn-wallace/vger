add_executable(vger_tests test_core.c)
target_link_libraries(vger_tests PRIVATE vger_core)
target_compile_features(vger_tests PRIVATE c_std_17)

if(VGER_ENABLE_STRICT_WARNINGS)
    set_source_files_properties(test_core.c PROPERTIES
        COMPILE_OPTIONS "${VGER_STRICT_WARNING_FLAGS}"
    )
endif()

add_test(NAME vger_core_tests COMMAND vger_tests)
