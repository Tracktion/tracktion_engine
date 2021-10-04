/*******************************************************************************
The block below describes the properties of this PIP. A PIP is a short snippet
of code that can be read by the Projucer and used to generate a JUCE project.

BEGIN_JUCE_PIP_METADATA

name:             RubberBandDemo
version:          0.0.1
vendor:           Tracktion
website:          www.tracktion.com
description:      This example simply loads a project from the command line and plays it back in a loop.

dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_processors, juce_audio_utils,
            juce_core, juce_data_structures, juce_dsp, juce_events, juce_graphics,
            juce_gui_basics, juce_gui_extra, juce_osc, tracktion_engine
exporters:        linux_make, vs2017, xcode_iphone, xcode_mac

moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1, JUCE_PLUGINHOST_AU=1, JUCE_PLUGINHOST_VST3=1, JUCE_MODAL_LOOPS_PERMITTED=1

type:             Component
mainClass:        RubberBandDemo

END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include "../modules/tracktion_engine/timestretch/tracktion_RubberBand.cpp"

//==============================================================================
//==============================================================================
class RubberBandDemo  : public Component
{
public:
    //==============================================================================
    RubberBandDemo()
    {
        // This example just runs the tests
        juce::UnitTestRunner tr;
        tr.runTests ({ &rubberBandStretcherTests });

        // This really should be a CLI but we'll just quit here instead
        JUCEApplicationBase::quit();
        
        setSize (600, 400);
    }

    //==============================================================================
    void paint(Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    }

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RubberBandDemo)
};

