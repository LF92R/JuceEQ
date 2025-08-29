cmake_minimum_required(VERSION 3.21)
project(JuceEQ VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

set(CPM_DOWNLOAD_VERSION 0.38.2)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM.cmake")
if(NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
  file(DOWNLOAD
    https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
    ${CPM_DOWNLOAD_LOCATION}
    TLS_VERIFY ON)
endif()
include(${CPM_DOWNLOAD_LOCATION})

CPMAddPackage(
  NAME JUCE
  GITHUB_REPOSITORY juce-framework/JUCE
  GIT_TAG 8.0.8
)

add_compile_definitions(
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
)

juce_add_plugin(JuceEQ
  COMPANY_NAME                 "MyPlugin"
  BUNDLE_ID                    com.myplugin.juceeq
  IS_SYNTH                     FALSE
  NEEDS_MIDI_INPUT             FALSE
  NEEDS_MIDI_OUTPUT            FALSE
  IS_MIDI_EFFECT               FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS  FALSE
  COPY_PLUGIN_AFTER_BUILD      TRUE
  PLUGIN_MANUFACTURER_CODE     MyPl    
  PLUGIN_CODE                  JcEQ    
  FORMATS                      Standalone
  PRODUCT_NAME                 "JuceEQ"
)

target_sources(JuceEQ PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginProcessor.h
  Source/PluginEditor.cpp
  Source/PluginEditor.h
  Source/EqGraphComponent.cpp
  Source/EqGraphComponent.h
  Source/BandControlsComponent.cpp
  Source/BandControlsComponent.h
  Source/LookAndFeel.cpp
  Source/LookAndFeel.h
  #Source/Meters.cpp
  #Source/Meters.h
)

target_link_libraries(JuceEQ PRIVATE
  juce::juce_dsp
  juce::juce_gui_extra
  juce::juce_audio_utils
  juce::juce_audio_processors
  juce::juce_audio_basics
  juce::juce_graphics
  juce::juce_gui_basics
)

if(WIN32)
  target_compile_definitions(JuceEQ PRIVATE JUCE_WIN_PER_MONITOR_DPI_AWARE=1)
endif()
