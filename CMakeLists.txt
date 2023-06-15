include_guard()

cmake_minimum_required (VERSION 3.15...3.20)

file(READ VERSION.md CURRENT_VERSION)
project(tracktion
        VERSION ${CURRENT_VERSION}
        DESCRIPTION
        "High level data model and set of classes for building \
        sequence based audio applications."
        HOMEPAGE_URL "https://github.com/Tracktion/tracktion_engine"
)

add_subdirectory(modules/juce)
add_subdirectory(modules)

add_subdirectory(examples)

# Add engine code to IDE browser
file(GLOB_RECURSE LibraryFiles CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/tracktion_core/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/tracktion_core/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/tracktion_graph/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/tracktion_graph/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/tracktion_engine/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/tracktion_engine/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/3rd_party/choc/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/3rd_party/choc/*.h)
set_source_files_properties(${LibraryFiles} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/modules PREFIX Libraries FILES ${LibraryFiles})
add_library(EngineSource ${LibraryFiles})
