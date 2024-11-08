cmake_minimum_required(VERSION 3.15)

set (TARGET_NAME coverage)

set(HAS_RUBBERBAND 0)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../modules/3rd_party/rubberband/")
    message(STATUS "Found rubberband, enabling")
    set(HAS_RUBBERBAND 1)
endif()

file(READ ../../VERSION.md CURRENT_VERSION)
project(${TARGET_NAME} VERSION ${CURRENT_VERSION})

add_subdirectory(../../modules/juce ./cmake_build_juce)
add_subdirectory(../../modules ./cmake_build_tracktion)

# If we are using MSVC we want static runtime linkage
if (MSVC)
    set (CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# If we are compiling for macOS we want to target OS versions down to 10.13
if (APPLE)
    set (CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE INTERNAL "")
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

juce_add_console_app(${TARGET_NAME})

juce_generate_juce_header(${TARGET_NAME})

target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

set_target_properties(${TARGET_NAME} PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden)

set(SourceFiles
    ${TARGET_NAME}.cpp
    ${TARGET_NAME}.h)

target_sources(${TARGET_NAME} PRIVATE ${SourceFiles})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX Source FILES ${SourceFiles})

include("${CMAKE_CURRENT_SOURCE_DIR}/../../tests/utils.cmake")
addModuleSourceTarget()

if (DEFINED ENV{VST2_SDK_DIR})
    target_compile_definitions(${TARGET_NAME} PRIVATE
        JUCE_PLUGINHOST_VST=1)
endif()

if (HAS_RUBBERBAND)
    target_include_directories(${TARGET_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../modules/3rd_party")
endif()

target_compile_definitions(${TARGET_NAME} PRIVATE
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_LADSPA=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1
    TRACKTION_UNIT_TESTS=1
    TRACKTION_BENCHMARKS=1
    TRACKTION_LOG_DEVICES=0
    TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND=${HAS_RUBBERBAND}
    TRACKTION_BUILD_RUBBERBAND=${HAS_RUBBERBAND})

target_link_libraries(${TARGET_NAME} PRIVATE
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_recommended_warning_flags)

# Compile with werror in release builds
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
    CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(${TARGET_NAME} PRIVATE "-Wimplicit-fallthrough")
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

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
    CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
    target_link_options(${TARGET_NAME} PRIVATE "--coverage")
endif()
