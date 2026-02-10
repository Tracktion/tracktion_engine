include_guard()

cmake_minimum_required (VERSION 3.15...3.20)

# Add a build option for enabling examples
option(TE_ADD_EXAMPLES "Add the example targets" ON)

file(READ VERSION.md CURRENT_VERSION)
project(tracktion
        VERSION ${CURRENT_VERSION}
        DESCRIPTION
        "High level data model and set of classes for building \
        sequence based audio applications."
        HOMEPAGE_URL "https://github.com/Tracktion/tracktion_engine"
)

# Only add JUCE if not already available from a parent project
if(NOT TARGET juce::juce_core)
    if (JUCE_CPM_DEVELOP)
        include(cmake/CPM.cmake)
        CPMAddPackage("gh:juce-framework/JUCE#develop")
    else()
        add_subdirectory(modules/juce)
    endif()
endif()

# Only register tracktion modules if not already defined
if(NOT TARGET tracktion_core)
    add_subdirectory(modules)
else()
    # Parent registered modules without tracktion:: namespace aliases.
    # Create aliases so examples can link against tracktion::tracktion_engine etc.
    if(NOT TARGET tracktion::tracktion_core)
        add_library(tracktion::tracktion_core ALIAS tracktion_core)
        add_library(tracktion::tracktion_engine ALIAS tracktion_engine)
        add_library(tracktion::tracktion_graph ALIAS tracktion_graph)
    endif()
endif()

if (TE_ADD_EXAMPLES)
    enable_testing()
    add_subdirectory(examples)
endif()
