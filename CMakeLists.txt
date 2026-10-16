cmake_minimum_required(VERSION 3.22)
project(FuseFlow VERSION 0.1.0 LANGUAGES CXX)

if (NOT WIN32)
    message(FATAL_ERROR "FuseFlow is currently configured for Windows 10/11.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.11.zip
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(FuseFlow
    PRODUCT_NAME "FuseFlow"
    COMPANY_NAME "FuseFlow Audio"
    VERSION "0.1.0"
)

target_sources(FuseFlow PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/MainComponent.h
    Source/AudioEngine.cpp
    Source/AudioEngine.h
)

target_compile_features(FuseFlow PRIVATE cxx_std_20)

target_compile_definitions(FuseFlow PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_ASIO=1
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    _CRT_SECURE_NO_WARNINGS=1
)

target_link_libraries(FuseFlow PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

if (MSVC)
    target_compile_options(FuseFlow PRIVATE /W4 /permissive-)
endif()
