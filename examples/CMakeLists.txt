include_guard()

add_subdirectory(Benchmarks)
add_subdirectory(TestRunner)
add_subdirectory(DemoRunner)
add_subdirectory(EngineInPluginDemo)

add_custom_target("AllExamples")
add_dependencies("AllExamples"
    Benchmarks
    TestRunner
    DemoRunner
    EngineInPluginDemo_common)

# TODO: Remove this section when JUCE is updated to fix these warnings upstream.
# - juce_events.mm: implicit int64-to-double conversion in juce_MessageManager_mac.mm:395
# - juce_audio_processors_headless.mm: deprecated wstring_convert usage from VST3 SDK
if (NOT MSVC)
    set(_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../modules/juce")

    # If using an external JUCE (e.g. JUCE-dev from parent project), try that path instead
    if (NOT EXISTS "${_JUCE_DIR}/modules/juce_events/juce_events.mm")
        set(_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../JUCE-dev")
    endif()

    set(_JUCE_EVENTS_MM "${_JUCE_DIR}/modules/juce_events/juce_events.mm")
    set(_JUCE_AUDIO_PROC_MM "${_JUCE_DIR}/modules/juce_audio_processors_headless/juce_audio_processors_headless.mm")

    set(_EXAMPLE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/Benchmarks
        ${CMAKE_CURRENT_SOURCE_DIR}/TestRunner
        ${CMAKE_CURRENT_SOURCE_DIR}/DemoRunner
        ${CMAKE_CURRENT_SOURCE_DIR}/EngineInPluginDemo)

    if (EXISTS "${_JUCE_EVENTS_MM}")
        set_source_files_properties("${_JUCE_EVENTS_MM}"
            DIRECTORY ${_EXAMPLE_DIRS}
            PROPERTIES COMPILE_OPTIONS "-Wno-implicit-int-float-conversion")
    endif()

    if (EXISTS "${_JUCE_AUDIO_PROC_MM}")
        set_source_files_properties("${_JUCE_AUDIO_PROC_MM}"
            DIRECTORY ${_EXAMPLE_DIRS}
            PROPERTIES COMPILE_OPTIONS "-Wno-deprecated-declarations")
    endif()
endif()