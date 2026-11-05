# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Anthony Charretier
cmake_minimum_required(VERSION 3.22)
project(GRMGuitar VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(GRMG_BUILD_PLUGIN "Build the JUCE plugin (downloads JUCE)" ON)
option(GRMG_BUILD_TESTS "Build the tests and the offline renderer" ON)

# ---------------------------------------------------------------- the GRM core and the guitar engine (no JUCE)
add_library(grm_core INTERFACE)
target_include_directories(grm_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/core)

if(GRMG_BUILD_TESTS)
    enable_testing()
    add_executable(test_core tests/test_core.cpp)
    target_link_libraries(test_core PRIVATE grm_core)
    add_test(NAME grm_core_matches_python COMMAND test_core)
    add_executable(test_physics tests/test_physics.cpp)
    target_link_libraries(test_physics PRIVATE grm_core)
    add_test(NAME guitar_physics_matches_python COMMAND test_physics)
    add_executable(test_play tests/test_play.cpp)
    target_link_libraries(test_play PRIVATE grm_core)
    add_test(NAME guitar_playing COMMAND test_play)
    add_executable(render_offline tests/render_offline.cpp)
    target_link_libraries(render_offline PRIVATE grm_core)
endif()

# ---------------------------------------------------------------- the plugin
if(GRMG_BUILD_PLUGIN)
    include(FetchContent)
    set(FETCHCONTENT_QUIET OFF)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(GRMGuitar
        COMPANY_NAME "Anthony Charretier"
        PLUGIN_MANUFACTURER_CODE Grmc
        PLUGIN_CODE Grmg
        FORMATS VST3 Standalone
        PRODUCT_NAME "GRM Guitar"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(GRMGuitar PRIVATE
        plugin/PluginProcessor.cpp
        plugin/PluginEditor.cpp
        plugin/RollView.cpp)

    target_compile_definitions(GRMGuitar PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(GRMGuitar
        PRIVATE
            grm_core
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_audio_formats
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    if(GRMG_BUILD_TESTS)
        # the plugin's own code, called as a host would, without a host
        add_executable(plugin_host_test tests/plugin_host_test.cpp)
        target_link_libraries(plugin_host_test PRIVATE GRMGuitar juce::juce_audio_formats juce::juce_recommended_config_flags)
        target_compile_definitions(plugin_host_test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    endif()
endif()
