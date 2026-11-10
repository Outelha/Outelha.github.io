include("${CMAKE_SOURCE_DIR}/libs/Catch2/extras/Catch.cmake")

# DSP tests: pure C++, no JUCE.
add_executable(triofx_dsp_tests
    DspTestUtils.h
    SaturationTests.cpp
    ChorusTests.cpp
    ReverbTests.cpp)
target_link_libraries(triofx_dsp_tests PRIVATE triofx_dsp Catch2::Catch2WithMain)
catch_discover_tests(triofx_dsp_tests DISCOVERY_MODE PRE_TEST)

# Plugin tests: link the plugin's shared code (processor + parameters + editor).
add_executable(triofx_plugin_tests PluginTests.cpp)
target_link_libraries(triofx_plugin_tests PRIVATE
    ${PRODUCT_NAME}
    Catch2::Catch2WithMain)
# Reuse the plugin's JUCE include paths and definitions without recompiling the modules.
target_include_directories(triofx_plugin_tests PRIVATE
    $<TARGET_PROPERTY:${PRODUCT_NAME},INCLUDE_DIRECTORIES>)
target_compile_definitions(triofx_plugin_tests PRIVATE
    $<TARGET_PROPERTY:${PRODUCT_NAME},COMPILE_DEFINITIONS>)
catch_discover_tests(triofx_plugin_tests DISCOVERY_MODE PRE_TEST)

