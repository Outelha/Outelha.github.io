# Standalone JUCE host that loads the built ShobeatsFX.vst3 and runs plugin
# lifecycle stress scenarios (create / process / editor / destroy / module unload).
juce_add_console_app(shobeats_lifecycle_host PRODUCT_NAME "ShobeatsLifecycleHost")

target_sources(shobeats_lifecycle_host PRIVATE Main.cpp)

target_compile_definitions(shobeats_lifecycle_host PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(shobeats_lifecycle_host PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

add_dependencies(shobeats_lifecycle_host ${PRODUCT_NAME}_VST3)
