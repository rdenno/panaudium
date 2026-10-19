cmake_minimum_required(VERSION 3.22)
project(Panaudium VERSION 0.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
# Link the MSVC runtime statically so the plugin loads on machines without the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
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
  target_sources(Panaudium PRIVATE src/plugin/PluginProcessor.cpp src/plugin/PluginEditor.cpp)
  target_compile_definitions(Panaudium PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
  # Headless host test: loads the built VST3 the way a DAW does (see tools/hosttest.cpp).
  juce_add_console_app(panaudium_hosttest PRODUCT_NAME "panaudium_hosttest")
  target_sources(panaudium_hosttest PRIVATE tools/hosttest.cpp)
  target_compile_definitions(panaudium_hosttest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(panaudium_hosttest PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_gui_extra
    juce::juce_recommended_config_flags)
  # GUI snapshot tool: compiles the processor + editor directly and renders each page to PNG.
  juce_add_console_app(panaudium_snapshot PRODUCT_NAME "panaudium_snapshot")
  target_sources(panaudium_snapshot PRIVATE tools/snapshot.cpp src/plugin/PluginProcessor.cpp src/plugin/PluginEditor.cpp)
  target_compile_definitions(panaudium_snapshot PRIVATE JucePlugin_Name="Panaudium" JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(panaudium_snapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
  target_link_libraries(Panaudium PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
endif()
