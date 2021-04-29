/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

  name:             tracktion_graph_PerformanceTests
  version:          0.0.1
  vendor:           Tracktion
  website:          www.tracktion.com
  description:      Used for development of the new tracktion_graph.

  dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_processors, juce_audio_utils,
                    juce_core, juce_data_structures, juce_dsp, juce_events, juce_graphics,
                    juce_gui_basics, juce_gui_extra, juce_osc, tracktion_engine, tracktion_graph
  exporters:        linux_make, vs2017, xcode_iphone, xcode_mac

  moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1, JUCE_PLUGINHOST_AU=1, JUCE_PLUGINHOST_VST3=1
  defines:          ENABLE_EXPERIMENTAL_TRACKTION_GRAPH=1, TRACKTION_GRAPH_PERFORMANCE_TESTS=1

  type:             Console
  mainClass:        tracktion_graph_PerformanceTests

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include "common/tracktion_graph_Dev.h"

//==============================================================================
//==============================================================================
int main (int argv, char** argc)
{
    File junitFile;
    
    for (int i = 1; i < argv; ++i)
        if (String (argc[i]) == "--junit-xml-file")
            if ((i + 1) < argv)
                junitFile = String (argc[i + 1]);
    
    ScopedJuceInitialiser_GUI init;
    return TestRunner::runTests (junitFile, "tracktion_graph_performance");
}
