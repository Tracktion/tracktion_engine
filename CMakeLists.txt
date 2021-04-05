include_guard()

cmake_minimum_required(3.17...3.20)

project(
        tracktion_engine
        VERSION 1.0.0
        DESCRIPTION
        "High level data model and set of classes for building
        sequence based audio applications."
        HOMEPAGE_URL "https://github.com/Tracktion/tracktion_engine"
)

add_subdirectory(${PROJECT_SOURCE_DIR}/modules)

if (JUCE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()
