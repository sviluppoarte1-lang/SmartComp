cmake_minimum_required(VERSION 3.22)
project(CompSage VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE 7.0.12
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(SmartComp
    COMPANY_NAME "Fear Escape"
    BUNDLE_ID "com.fearescape.compsage"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE FeEs
    PLUGIN_CODE SmCp
    FORMATS VST3 Standalone
    PRODUCT_NAME "SmartComp"
    DISPLAY_NAME "Comp Sage Leveling Compressor"
    VST3_CATEGORIES "Fx|Dynamics|Mastering"
    DESCRIPTION "Leveling compressor-limiter with automatic dynamics regulation by Fear Escape"
    VERSION "1.0.0"
)

target_sources(SmartComp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/Compressor.cpp
    Source/DSP/AutoLeveler.cpp
    Source/DSP/SongDynamics.cpp
    Source/Presets/CompPresetManager.cpp
    Source/UI/CompDisplay.cpp
    Source/UI/ModernLookAndFeel.cpp
)

target_include_directories(SmartComp PRIVATE Source)

target_compile_definitions(SmartComp PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(SmartComp PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce_recommended_config_flags
    juce_recommended_lto_flags
    juce_recommended_warning_flags
)

# Linux: explicit X11 deps (avoids editor crashes in Bitwig/DAW)
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ALSA alsa)
    pkg_check_modules(FREETYPE freetype2)
    pkg_check_modules(X11 REQUIRED x11 xext xcomposite xcursor xinerama xrandr)
    pkg_check_modules(FONTCONFIG fontconfig)

    target_link_libraries(SmartComp PRIVATE
        juce::juce_audio_devices
        ${X11_LIBRARIES}
        ${FONTCONFIG_LIBRARIES}
    )
    target_include_directories(SmartComp PRIVATE ${X11_INCLUDE_DIRS} ${FONTCONFIG_INCLUDE_DIRS})
    target_compile_options(SmartComp PRIVATE ${X11_CFLAGS_OTHER} ${FONTCONFIG_CFLAGS_OTHER})

    target_compile_definitions(SmartComp PRIVATE JUCE_USE_X11=1 JUCE_USE_XCURSOR=1 JUCE_USE_XINERAMA=1 JUCE_USE_XRANDR=1)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/Source PREFIX Source FILES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/Compressor.cpp
    Source/DSP/AutoLeveler.cpp
    Source/DSP/SongDynamics.cpp
    Source/Presets/CompPresetManager.cpp
    Source/UI/CompDisplay.cpp
    Source/UI/ModernLookAndFeel.cpp
)
