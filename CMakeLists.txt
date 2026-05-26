cmake_minimum_required(VERSION 3.15)

project(layerhost VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(
    juce
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/7.0.9.zip
)
FetchContent_MakeAvailable(juce)

FetchContent_Declare(
    juce_clap_hosting
    GIT_REPOSITORY https://github.com/jatinchowdhury18/juce_clap_hosting.git
    GIT_TAG        main
    SOURCE_DIR     ${CMAKE_BINARY_DIR}/modules/juce_clap_hosting
)
FetchContent_MakeAvailable(juce_clap_hosting)

juce_add_gui_app(layerhost
    PRODUCT_NAME "layerhost"
    VERSION 1.0.0)

target_sources(layerhost PRIVATE
    Source/Main.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp
    Source/AudioEngine.h
    Source/AudioEngine.cpp
    Source/Track.h
    Source/Track.cpp
    Source/Item.h
    Source/Item.cpp
)

target_compile_definitions(layerhost PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_AU=0
)

target_link_libraries(layerhost PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_audio_formats
    juce::juce_audio_devices
    juce::juce_events
    juce::juce_core
    juce::juce_data_structures
    juce::juce_opengl
    juce_clap_hosting
)

target_include_directories(layerhost PRIVATE
    ${CMAKE_BINARY_DIR}/modules/juce_clap_hosting/clap/include
    ${CMAKE_BINARY_DIR}/modules/juce_clap_hosting/clap-helpers/include
)

juce_generate_juce_header(layerhost)
