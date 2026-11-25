# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Anthony Charretier
cmake_minimum_required(VERSION 3.22)
project(GRMGuitar VERSION 0.3.9 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(GRMG_BUILD_PLUGIN "Build the JUCE plugin (downloads JUCE)" ON)
option(GRMG_BUILD_TESTS "Build the tests and the offline renderer" OFF)
option(GRMG_PORTABLE_CPU "Disable x86-64-v3 / AVX2 requirements for older CPUs" OFF)

include(FetchContent)
FetchContent_Declare(cppgrm
    GIT_REPOSITORY https://github.com/innermost47/cppgrm.git
    GIT_TAG        main
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(cppgrm)

add_library(grm_core ALIAS cppgrm)


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
    add_executable(test_amp tests/test_amp.cpp)
    target_link_libraries(test_amp PRIVATE grm_core)
    add_test(NAME guitar_amplifier COMMAND test_amp)
    add_executable(render_offline tests/render_offline.cpp)
    target_link_libraries(render_offline PRIVATE grm_core)
    add_executable(test_pickup tests/test_pickup.cpp)
    target_link_libraries(test_pickup PRIVATE grm_core)
    add_test(NAME guitar_magnetic_pickup COMMAND test_pickup)
    add_executable(test_strings tests/test_strings.cpp)
    target_link_libraries(test_strings PRIVATE grm_core)
    add_test(NAME guitar_string_sets COMMAND test_strings)
    add_executable(timbre_probe tests/timbre_probe.cpp)
    target_link_libraries(timbre_probe PRIVATE grm_core)
    add_test(NAME timbre_probe_runs COMMAND timbre_probe ${CMAKE_BINARY_DIR}/timbre_probe_out)
endif()

if(GRMG_BUILD_PLUGIN)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)

    set(PROJECT_VENDOR "InnerMost47")
    juce_add_plugin(GRMGuitar
        COMPANY_NAME ${PROJECT_VENDOR}
        BUNDLE_ID "com.innermost47.grm-guitar"
        PLUGIN_MANUFACTURER_CODE Grmc
        PLUGIN_CODE Grmg
        FORMATS VST3 Standalone
        PRODUCT_NAME "GRM Guitar"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    juce_add_binary_data(GRMGuitarBinaryData
        SOURCES
            assets/fonts/michroma.ttf
            assets/fonts/noto-bold.ttf
            assets/fonts/noto-italic.ttf
            assets/fonts/noto-regular.ttf
            assets/images/grm-equations.svg
            assets/presets/acoustic-dadgad-church.grmpreset
            assets/presets/acoustic-dreadnought-studio.grmpreset
            assets/presets/acoustic-fingerstyle-cedar.grmpreset
            assets/presets/acoustic-hard-strum.grmpreset
            assets/presets/acoustic-old-parlour.grmpreset
            assets/presets/acoustic-piezo-stage.grmpreset
            assets/presets/electric-british-crunch.grmpreset
            assets/presets/electric-clean-single-coil.grmpreset
            assets/presets/electric-djent.grmpreset
            assets/presets/electric-lead-neck.grmpreset
            assets/presets/electric-metal-drop-d.grmpreset
            assets/presets/electric-overdrive-blues.grmpreset
            assets/presets/electric-punk.grmpreset
            assets/icons/arpeggio.svg
            assets/icons/arrow-clockwise.svg
            assets/icons/arrow-counter-clockwise.svg
            assets/icons/arrows-clockwise.svg
            assets/icons/caret-down.svg
            assets/icons/caret-left.svg
            assets/icons/caret-right.svg
            assets/icons/caret-up.svg
            assets/icons/clipboard.svg
            assets/icons/copy-simple.svg
            assets/icons/copy.svg
            assets/icons/crosshair.svg
            assets/icons/eraser.svg
            assets/icons/export.svg
            assets/icons/file-plus.svg
            assets/icons/floppy-disk.svg
            assets/icons/ghost.svg
            assets/icons/headphones.svg
            assets/icons/minus.svg
            assets/icons/music-notes.svg
            assets/icons/play.svg
            assets/icons/plus.svg
            assets/icons/speaker-slash.svg
            assets/icons/stop.svg
            assets/icons/strum.svg
            assets/icons/trash.svg
    )

    target_sources(GRMGuitar PRIVATE
        plugin/PluginProcessor.cpp
        plugin/GuitarTrack.cpp
        plugin/PluginEditor.cpp
        plugin/solver/SolverThread.cpp
        plugin/style/CustomLookAndFeel.cpp
        plugin/style/IconLibrary.cpp
        plugin/style/Assets.cpp
        plugin/widgets/IconButton.cpp
        plugin/widgets/EquationsPanel.cpp
        plugin/widgets/Modal.cpp
        plugin/services/UpdateChecker.cpp
        plugin/services/GuitarTuning.cpp
        plugin/services/PresetLibrary.cpp
        plugin/widgets/PresetBar.cpp
        plugin/widgets/TensionView.cpp
        plugin/views/MixerView.cpp
        plugin/views/NeckView.cpp
        plugin/views/NetworksView.cpp
        plugin/views/ParamPanel.cpp
        plugin/views/RollView.cpp
        plugin/views/ScopeView.cpp)

    target_include_directories(GRMGuitar PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin/solver
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin/views
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin/style
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin/widgets
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin/services)

    target_compile_definitions(GRMGuitar PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_compile_options(GRMGuitar PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:
            $<$<CONFIG:Release>:
                -O3
                -ffast-math
                -fno-finite-math-only
                -fno-math-errno
                -fno-trapping-math
            >
        >
        $<$<CXX_COMPILER_ID:MSVC>:
            $<$<CONFIG:Release>:
                /O2
                /fp:fast
            >
        >)

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        if(NOT GRMG_PORTABLE_CPU)
            target_compile_options(GRMGuitar PRIVATE
                $<$<CXX_COMPILER_ID:GNU,Clang>:
                    -march=x86-64-v3
                >
                $<$<CXX_COMPILER_ID:MSVC>:
                    /arch:AVX2
                >)
        endif()
    endif()

    target_link_libraries(GRMGuitar
        PRIVATE
            grm_core
            GRMGuitarBinaryData
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_audio_formats
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    if(GRMG_BUILD_TESTS)
        # a test built against the plugin's code (editor, processor, look and feel, binary data)
        function(grmg_add_plugin_test name)
            add_executable(${name} tests/${name}.cpp)
            target_link_libraries(${name} PRIVATE GRMGuitar GRMGuitarBinaryData grm_core
                juce::juce_audio_formats juce::juce_recommended_config_flags)
            target_compile_definitions(${name} PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        endfunction()

        grmg_add_plugin_test(plugin_host_test)
        grmg_add_plugin_test(plugin_cpu_test)
        grmg_add_plugin_test(editor_snapshot)
        grmg_add_plugin_test(test_icons)
        add_test(NAME interface_icons COMMAND test_icons)
        grmg_add_plugin_test(test_modal)
        add_test(NAME interface_dialogs COMMAND test_modal)
        grmg_add_plugin_test(test_presets)
        add_test(NAME interface_presets COMMAND test_presets)
        grmg_add_plugin_test(test_tuning)
        add_test(NAME guitar_free_tuning COMMAND test_tuning)
    endif()
endif()