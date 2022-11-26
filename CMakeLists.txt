include_guard()

cmake_minimum_required (VERSION 3.15...3.20)

project(
        tracktion_engine
        VERSION 2.0.0
        DESCRIPTION
        "High level data model and set of classes for building \
        sequence based audio applications."
        HOMEPAGE_URL "https://github.com/Tracktion/tracktion_engine"
)

add_subdirectory(modules)

if (TRACKTION_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()