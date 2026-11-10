include("${CMAKE_SOURCE_DIR}/libs/Catch2/extras/Catch.cmake")

# DSP tests: pure C++, no JUCE.
add_executable(shobeats_dsp_tests
    DspTestUtils.h
    SaturationTests.cpp
    ResamplerTests.cpp
    TapeTests.cpp
    ChorusTests.cpp
    TremoloTests.cpp
    ReverseTests.cpp
    DelayTests.cpp
    MovementTests.cpp
    ReverbTests.cpp)
target_link_libraries(shobeats_dsp_tests PRIVATE shobeats_dsp Catch2::Catch2WithMain)
catch_discover_tests(shobeats_dsp_tests DISCOVERY_MODE PRE_TEST)

# Plugin tests: link the plugin's shared code (processor + parameters + editor).
add_executable(shobeats_plugin_tests PluginTests.cpp)
target_link_libraries(shobeats_plugin_tests PRIVATE
    ${PRODUCT_NAME}
    Catch2::Catch2WithMain)
# Reuse the plugin's JUCE include paths and definitions without recompiling the modules.
target_include_directories(shobeats_plugin_tests PRIVATE
    $<TARGET_PROPERTY:${PRODUCT_NAME},INCLUDE_DIRECTORIES>)
target_compile_definitions(shobeats_plugin_tests PRIVATE
    $<TARGET_PROPERTY:${PRODUCT_NAME},COMPILE_DEFINITIONS>)
catch_discover_tests(shobeats_plugin_tests DISCOVERY_MODE PRE_TEST)

