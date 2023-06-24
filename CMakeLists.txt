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
