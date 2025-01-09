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

if (JUCE_CPM_DEVELOP)
    include(cmake/CPM.cmake)
    CPMAddPackage("gh:juce-framework/JUCE#develop")
else()
    add_subdirectory(modules/juce)
endif()

add_subdirectory(modules)

enable_testing()
add_subdirectory(examples)
