cmake_minimum_required(VERSION 3.22)

# ---------------------------------------------------------------------------
# Product identity. Change these in one place.
# ---------------------------------------------------------------------------
set(PRODUCT_NAME "ShobeatsFX")
set(PRODUCT_DISPLAY_NAME "SHOBEATS FX")
set(COMPANY_NAME "SHOBEATS")
set(BUNDLE_ID "com.shobeats.shobeatsfx")
set(MANUFACTURER_CODE "Shbt") # 4 chars, at least one uppercase
set(PLUGIN_CODE "Shfx")       # 4 chars, unique per plugin

project(${PRODUCT_NAME} VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static MSVC runtime so the VST3 has no dependency on the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Release builds still emit .pdb symbol files on MSVC so crashes can be diagnosed.
# (/Zi and /DEBUG do not change code generation; /OPT:REF,ICF keep the usual Release linker optimisations.)
if(MSVC)
    string(APPEND CMAKE_C_FLAGS_RELEASE " /Zi")
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " /Zi")
    foreach(kind SHARED MODULE EXE)
        string(APPEND CMAKE_${kind}_LINKER_FLAGS_RELEASE " /DEBUG /OPT:REF /OPT:ICF")
    endforeach()
endif()

option(SHOBEATS_BUILD_TESTS "Build the unit tests" ON)
option(SHOBEATS_BUILD_STANDALONE "Also build a standalone app (handy for UI work)" OFF)

# ---------------------------------------------------------------------------
# Dependencies: pinned git submodules (see .gitmodules / README).
# ---------------------------------------------------------------------------
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE submodule is missing. Run:\n  git submodule update --init --recursive")
endif()
add_subdirectory(libs/JUCE EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# Pure C++ DSP library (no JUCE dependency) — easy to unit test.
# ---------------------------------------------------------------------------
add_library(shobeats_dsp STATIC
    source/dsp/Saturation.cpp
    source/dsp/Resampler.cpp
    source/dsp/Tape.cpp
    source/dsp/Chorus.cpp
    source/dsp/Tremolo.cpp
    source/dsp/Reverse.cpp
    source/dsp/Delay.cpp
    source/dsp/Reverb.cpp)
target_include_directories(shobeats_dsp PUBLIC source)
target_compile_features(shobeats_dsp PUBLIC cxx_std_20)
set_target_properties(shobeats_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(shobeats_dsp PRIVATE /W4 /permissive-)
else()
    target_compile_options(shobeats_dsp PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
endif()

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
set(SHOBEATS_FORMATS VST3)
if(SHOBEATS_BUILD_STANDALONE)
    list(APPEND SHOBEATS_FORMATS Standalone)
endif()

juce_add_plugin(${PRODUCT_NAME}
    PRODUCT_NAME "${PRODUCT_NAME}"
    PLUGIN_NAME "${PRODUCT_DISPLAY_NAME}"
    DESCRIPTION "Analog-style multi-effects pedalboard"
    COMPANY_NAME "${COMPANY_NAME}"
    BUNDLE_ID "${BUNDLE_ID}"
    PLUGIN_MANUFACTURER_CODE ${MANUFACTURER_CODE}
    PLUGIN_CODE ${PLUGIN_CODE}
    FORMATS ${SHOBEATS_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion Modulation Delay Reverb)

# Factory presets: JSON files compiled into the plugin.
file(GLOB SHOBEATS_FACTORY_PRESETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/presets/factory/*.json")
juce_add_binary_data(ShobeatsPresetData
    HEADER_NAME ShobeatsPresetData.h
    NAMESPACE ShobeatsPresetData
    SOURCES ${SHOBEATS_FACTORY_PRESETS})
set_target_properties(ShobeatsPresetData PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(${PRODUCT_NAME} PRIVATE
    source/Parameters.cpp
    source/presets/Preset.cpp
    source/presets/PresetManager.cpp
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/ui/ShobeatsLookAndFeel.cpp
    source/ui/Controls.cpp
    source/ui/EffectDisplay.cpp
    source/ui/Pedalboard.cpp
    source/ui/DetailPanel.cpp
    source/ui/PresetBrowser.cpp)

target_compile_definitions(${PRODUCT_NAME} PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# JUCE modules are linked PRIVATE so their sources are compiled exactly once,
# into the plugin's shared-code static library (which the tests reuse).
target_link_libraries(${PRODUCT_NAME}
    PUBLIC
        shobeats_dsp
        ShobeatsPresetData
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PRIVATE
        juce::juce_recommended_warning_flags
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(SHOBEATS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(libs/Catch2 EXCLUDE_FROM_ALL)
    add_subdirectory(tests)
endif()
