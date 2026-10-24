cmake_minimum_required(VERSION 3.22)

project(JACKPOT VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Universal binary on macOS (Apple Silicon + Intel) unless overridden
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")

if(MSVC)
    # static runtime so the .vst3 runs on machines without the VC++ redistributable
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory(JUCE)

set(JACKPOT_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND JACKPOT_FORMATS AU)
endif()

juce_add_plugin(JACKPOT
    COMPANY_NAME "Jackpot Audio"
    COMPANY_WEBSITE ""
    BUNDLE_ID "com.jackpotaudio.jackpot"
    PLUGIN_MANUFACTURER_CODE Jkpa
    PLUGIN_CODE Jkp1
    FORMATS ${JACKPOT_FORMATS}
    PRODUCT_NAME "JACKPOT"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion Delay
    AU_MAIN_TYPE kAudioUnitType_Effect)

juce_add_binary_data(JACKPOTData SOURCES
    Resources/fonts/Inter-Regular.ttf
    Resources/fonts/Inter-SemiBold.ttf
    Resources/fonts/Inter-Bold.ttf
    Resources/fonts/Inter-ExtraBold.ttf
    Resources/fonts/IBMPlexMono-Regular.ttf)

target_sources(JACKPOT PRIVATE
    Source/Evolution.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Randomizer.cpp
    Source/PresetManager.cpp)

target_include_directories(JACKPOT PRIVATE Source)

target_compile_definitions(JACKPOT PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(JACKPOT
    PRIVATE
        JACKPOTData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# Offline test harness (renders audio + UI snapshot). Not needed for the plugin.
option(JACKPOT_BUILD_TESTS "Build the offline render test" OFF)
if(JACKPOT_BUILD_TESTS)
    juce_add_console_app(JackpotTest PRODUCT_NAME "JackpotTest")
    target_sources(JackpotTest PRIVATE
        tools/RenderTest.cpp
        Source/Evolution.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Randomizer.cpp
        Source/PresetManager.cpp)
    target_include_directories(JackpotTest PRIVATE Source)
    target_compile_definitions(JackpotTest PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JACKPOT_TEST_BUILD=1 JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="JACKPOT" JucePlugin_Build_Standalone=0)
    target_link_libraries(JackpotTest PRIVATE
        JACKPOTData juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
