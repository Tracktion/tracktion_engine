/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

  name:             Coverage
  version:          0.0.1
  vendor:           Tracktion
  website:          www.tracktion.com
  description:      This simply runs the unit tests within Tracktion Engine.

  dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_processors, juce_audio_utils,
                    juce_core, juce_data_structures, juce_dsp, juce_events, juce_graphics,
                    juce_gui_basics, juce_gui_extra, juce_osc, tracktion_engine, tracktion_graph
  exporters:        linux_make, vs2017, xcode_iphone, xcode_mac

  moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1, JUCE_PLUGINHOST_AU=1, JUCE_PLUGINHOST_VST3=1
  defines:          TRACKTION_UNIT_TESTS=1, JUCE_MODAL_LOOPS_PERMITTED=1, TRACKTION_ENABLE_SOUNDTOUCH=1

  type:             Console
  mainClass:        Coverage

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

using namespace tracktion;

#define DOCTEST_CONFIG_IMPLEMENT
#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

#include "../../examples/common/tracktion_graph_Dev.h"

//==============================================================================
//==============================================================================
int main (int, char**)
{
    ScopedJuceInitialiser_GUI init;

    auto doctestContext = std::make_unique<doctest::Context>();
    doctestContext->setOption ("duration", true);
    doctestContext->addFilter("test-suite", "tracktion_core");
    doctestContext->addFilter("test-suite", "tracktion_graph");
    doctestContext->addFilter("test-suite", "tracktion_engine");

    return TestRunner::runTests ({},
                                 std::vector<juce::String> {
                                    "Tracktion",
                                    "Tracktion:Longer",
                                    "tracktion_benchmarks",
                                    "tracktion_core",
                                    "tracktion_graph",
                                    "tracktion_engine",
                                    "tracktion_graph_performance" },
                                    std::move (doctestContext));
}
