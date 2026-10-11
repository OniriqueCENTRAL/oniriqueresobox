cmake_minimum_required(VERSION 3.22)

project(OniriqueResoBox VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    # Universal binary, and the oldest macOS the JUCE 9 runtime supports.
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# JUCE is looked for next to this file first, then in the PitchBox checkout
# beside this one, so the two plugins can share a single copy.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    set(RESOBOX_JUCE_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
else()
    set(RESOBOX_JUCE_DEFAULT "${CMAKE_CURRENT_SOURCE_DIR}/../PitchBox/JUCE")
endif()

set(RESOBOX_JUCE_DIR "${RESOBOX_JUCE_DEFAULT}" CACHE PATH "Path to JUCE")

if(NOT EXISTS "${RESOBOX_JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE not found at ${RESOBOX_JUCE_DIR}.\n"
        "Run:  git clone --depth 1 --branch 9.0.3 https://github.com/juce-framework/JUCE.git")
endif()

add_subdirectory("${RESOBOX_JUCE_DIR}" JUCE EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# The DSP, built on its own so the offline tests can link it without dragging
# in JUCE. Everything that decides how the plugin sounds lives here.
add_library(resobox_core STATIC
    Source/dsp/Fft.cpp
    Source/dsp/ResonanceSuppressor.cpp)

target_include_directories(resobox_core PUBLIC Source)
set_target_properties(resobox_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    # MSVC's <cmath> only declares M_PI when asked. PUBLIC so the tests get it too.
    target_compile_definitions(resobox_core PUBLIC _USE_MATH_DEFINES)
    target_compile_options(resobox_core PRIVATE /W4)
else()
    target_compile_options(resobox_core PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# The plugin.
set(RESOBOX_FORMATS VST3)

if(APPLE)
    list(APPEND RESOBOX_FORMATS AU)
endif()

# The system VST3 folder on Windows is under Program Files, which a normal build
# cannot write to, so installing after the build is opt-in there.
if(WIN32)
    option(RESOBOX_COPY_AFTER_BUILD "Install the plugin after building" OFF)
else()
    option(RESOBOX_COPY_AFTER_BUILD "Install the plugin after building" ON)
endif()

juce_add_plugin(OniriqueResoBox
    COMPANY_NAME "Onirique"
    BUNDLE_ID "com.onirique.resobox"
    PLUGIN_MANUFACTURER_CODE Onrq
    PLUGIN_CODE Rsbx
    FORMATS ${RESOBOX_FORMATS}
    PRODUCT_NAME "ResoBox"
    # What hosts list it as. The file on disk keeps the short name.
    PLUGIN_NAME "ONIRIQUE ResoBox"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    # For the Delete key on the curve, and for typing a preset's name.
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Fx Dynamics EQ
    COPY_PLUGIN_AFTER_BUILD ${RESOBOX_COPY_AFTER_BUILD})

# The Onirique sigil, as it appears in the header of the site.
juce_add_binary_data(ResoBoxAssets SOURCES Resources/sigil.png)

target_sources(OniriqueResoBox PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/SpectrumDisplay.cpp
    Source/ui/Theme.cpp)

if(MSVC)
    # The descriptions of the controls are written in French in the sources,
    # which are UTF-8: without this, MSVC reads them in the system code page.
    target_compile_options(OniriqueResoBox PRIVATE /utf-8)
endif()

target_compile_definitions(OniriqueResoBox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(OniriqueResoBox
    PRIVATE
        resobox_core
        ResoBoxAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Offline test: the whole audio path without a host.
enable_testing()

add_executable(test_suppressor tests/test_suppressor.cpp)
target_link_libraries(test_suppressor PRIVATE resobox_core)
target_include_directories(test_suppressor PRIVATE tests)
add_test(NAME suppressor COMMAND test_suppressor)

# A development tool, not part of the product: renders the editor to a PNG
# without a host. Built only when asked for by name.
add_executable(resobox_snapshot EXCLUDE_FROM_ALL tools/snapshot.cpp)

target_include_directories(resobox_snapshot PRIVATE
    tests
    $<TARGET_PROPERTY:OniriqueResoBox,INCLUDE_DIRECTORIES>)

target_compile_definitions(resobox_snapshot PRIVATE
    $<TARGET_PROPERTY:OniriqueResoBox,COMPILE_DEFINITIONS>)

target_link_libraries(resobox_snapshot PRIVATE OniriqueResoBox resobox_core ResoBoxAssets)
