cmake_minimum_required(VERSION 3.15)

set (TARGET_NAME DemoRunner)

if (DEFINED PROJECT_NAME)
    message ("Adding ${TARGET_NAME} to the ${PROJECT_NAME} project")
else()
    file(READ ../../VERSION.md CURRENT_VERSION)
    project(${TARGET_NAME} VERSION ${CURRENT_VERSION})
    add_subdirectory(../../modules/juce ./cmake_build_juce)
    add_subdirectory(../../modules ./cmake_build_tracktion)
endif()

# If we are using MSVC we want static runtime linkage
if (MSVC)
    set (CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# If we are compiling for macOS we want to target OS versions down to 10.13
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
message ("-- building for macOS")
    set (CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE INTERNAL "")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "iOS")
    set (CMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "17.0" CACHE INTERNAL "")
    set (CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development" CACHE INTERNAL "")
endif()

# Adds all the module sources so they appear correctly in the IDE
set_property(GLOBAL PROPERTY USE_FOLDERS YES)
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Enable Module Source Groups" ON)

if (DEFINED ENV{VST2_SDK_DIR})
    MESSAGE(STATUS "Building with VST2 SDK: $ENV{VST2_SDK_DIR}")
    juce_set_vst2_sdk_path($ENV{VST2_SDK_DIR})
else()
    MESSAGE(STATUS "Not building with VST2")
endif()

#juce_add_pip (${TARGET_NAME}.h)
juce_add_gui_app(${TARGET_NAME}
    BUNDLE_ID "com.tracktion.DemoRunner"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Audio recording"
    REQUIRES_FULL_SCREEN TRUE
    STATUS_BAR_HIDDEN TRUE
    PLUGINHOST_AU TRUE)

juce_generate_juce_header(${TARGET_NAME})

juce_add_binary_data (${TARGET_NAME}BinaryData SOURCES
    resources/edm_song.ogg
    resources/drum_loop.wav
    resources/synth_loop.wav
    resources/midi_arp.mid
    resources/midi_arp_2.mid)

target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

set_target_properties(${TARGET_NAME} PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden)

set(SourceFiles
    ${TARGET_NAME}.h
    ${TARGET_NAME}.cpp
    demos/AbletonLinkDemo.h
    demos/ContainerClipDemo.h
    demos/DistortionEffectDemo.h
    demos/GlobalQuantiseDemo.h
    demos/IRPluginDemo.h
    demos/MidiRecordingDemo.h
    demos/PatternGeneratorDemo.h
    demos/PitchAndTimeDemo.h
    demos/PlaybackDemo.h
    demos/PluginDemo.h
    demos/RecordingDemo.h
    demos/StepSequencerDemo.h)

target_sources(${TARGET_NAME} PRIVATE ${SourceFiles})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/.. PREFIX Source FILES ${SourceFiles})

include("${CMAKE_CURRENT_SOURCE_DIR}/../../tests/utils.cmake")
addModuleSourceTarget()

if (DEFINED ENV{VST2_SDK_DIR})
    target_compile_definitions(${TARGET_NAME} PRIVATE
        JUCE_PLUGINHOST_VST=1)
endif()


target_compile_definitions(${TARGET_NAME} PRIVATE
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_LADSPA=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1)

target_link_libraries(${TARGET_NAME} PRIVATE
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_recommended_warning_flags
    ${TARGET_NAME}BinaryData)

# Compile with werror in release builds
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
    CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(${TARGET_NAME} PRIVATE "$<$<CONFIG:Release>:-Werror>")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(${TARGET_NAME} PRIVATE "/W4")
    target_compile_options(${TARGET_NAME} PRIVATE "$<$<CONFIG:Release>:/WX>")
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_compile_options(${TARGET_NAME} PRIVATE "-fno-aligned-allocation")
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(${TARGET_NAME} PRIVATE "-latomic")

  if (NOT ${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "aarch64")
    target_link_options(${TARGET_NAME} PRIVATE "-m64")
  endif()
endif()
