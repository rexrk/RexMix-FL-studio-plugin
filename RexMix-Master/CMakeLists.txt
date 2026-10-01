cmake_minimum_required(VERSION 3.22)

project(RexMixMaster VERSION 0.3.0)

set(JUCE_SOURCE_DIR "$ENV{USERPROFILE}/source/JUCE" CACHE PATH "Path to the JUCE source tree")

if(NOT EXISTS "${JUCE_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE was not found at '${JUCE_SOURCE_DIR}'. Set JUCE_SOURCE_DIR to the JUCE source directory.")
endif()

add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")

juce_add_plugin(RexMixMaster
    COMPANY_NAME "RexMix"
    BUNDLE_ID com.rexmix.master
    PLUGIN_MANUFACTURER_CODE RexM
    PLUGIN_CODE rxM1
    FORMATS VST3
    PRODUCT_NAME "RexMix Master"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Analyzer)

target_sources(RexMixMaster
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/CaptureSession.cpp
        Source/RexMixSharedMemory.cpp)

target_compile_features(RexMixMaster PRIVATE cxx_std_17)

target_compile_definitions(RexMixMaster
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX)

target_link_libraries(RexMixMaster
    PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

include(CTest)

if(BUILD_TESTING)
    add_executable(RexMixSharedMemoryTest
        tests/SharedMemoryTest.cpp
        Source/RexMixSharedMemory.cpp)

    target_include_directories(RexMixSharedMemoryTest PRIVATE Source)
    target_compile_features(RexMixSharedMemoryTest PRIVATE cxx_std_17)
    target_compile_definitions(RexMixSharedMemoryTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    add_test(NAME RexMixSharedMemoryTest COMMAND RexMixSharedMemoryTest)

    add_executable(RexMixCaptureSessionTest
        tests/CaptureSessionTest.cpp
        Source/CaptureSession.cpp)

    target_include_directories(RexMixCaptureSessionTest PRIVATE Source)
    target_compile_features(RexMixCaptureSessionTest PRIVATE cxx_std_17)
    add_test(NAME RexMixCaptureSessionTest COMMAND RexMixCaptureSessionTest)
endif()
