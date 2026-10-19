cmake_minimum_required(VERSION 3.22)
project(Panaudium VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Offline renderer: depends only on the header-only DSP core (src/dsp).
add_executable(panaudium_render tools/render.cpp)
if(MSVC)
  target_compile_options(panaudium_render PRIVATE /O2 /fp:fast)
endif()

option(PANAUDIUM_BUILD_PLUGIN "Build the JUCE VST3/Standalone plugin" ON)
if(PANAUDIUM_BUILD_PLUGIN)
  # Use a local ./JUCE checkout if present, otherwise fetch the pinned release.
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt)
    add_subdirectory(JUCE)
  else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.8 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
  endif()
  juce_add_plugin(Panaudium
    COMPANY_NAME "Panaudium"
    PLUGIN_MANUFACTURER_CODE Pnau
    PLUGIN_CODE Pna1
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3 Standalone
    PRODUCT_NAME "Panaudium")
  target_sources(Panaudium PRIVATE src/plugin/PluginProcessor.cpp)
  target_compile_definitions(Panaudium PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
  target_link_libraries(Panaudium PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
endif()
