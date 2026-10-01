cmake_minimum_required(VERSION 3.22)

project(RexMixNode VERSION 0.4.1)

set(JUCE_SOURCE_DIR "$ENV{USERPROFILE}/source/JUCE" CACHE PATH "Path to the JUCE source tree")

if(NOT EXISTS "${JUCE_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE was not found at '${JUCE_SOURCE_DIR}'. Set JUCE_SOURCE_DIR to the JUCE source directory.")
endif()

add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")

juce_add_plugin(RexMixNode
    COMPANY_NAME "RexMix"
    BUNDLE_ID com.rexmix.node
    PLUGIN_MANUFACTURER_CODE RexM
    PLUGIN_CODE rxN2
    FORMATS VST3
    PRODUCT_NAME "RexMix Node"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Analyzer)

target_sources(RexMixNode
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/RexMixSharedMemory.cpp)

target_compile_features(RexMixNode PRIVATE cxx_std_17)

target_compile_definitions(RexMixNode
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(RexMixNode
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

include(CTest)

if(BUILD_TESTING)
    add_executable(RexMixNodeSharedMemoryTest
        tests/NodeSharedMemoryTest.cpp
        Source/RexMixSharedMemory.cpp)

    target_include_directories(RexMixNodeSharedMemoryTest PRIVATE Source)
    target_compile_features(RexMixNodeSharedMemoryTest PRIVATE cxx_std_17)
    target_compile_definitions(RexMixNodeSharedMemoryTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(RexMixNodeSharedMemoryTest PRIVATE ole32)
    add_test(NAME RexMixNodeSharedMemoryTest COMMAND RexMixNodeSharedMemoryTest)
endif()

target_link_libraries(RexMixNode PRIVATE ole32)
