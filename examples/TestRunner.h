/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

  name:             TestRunner
  version:          0.0.1
  vendor:           Tracktion
  website:          www.tracktion.com
  description:      This simply runs the unit tests within Tracktion Engine.

  dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_processors, juce_audio_utils,
                    juce_core, juce_data_structures, juce_dsp, juce_events, juce_graphics,
                    juce_gui_basics, juce_gui_extra, juce_osc, tracktion_engine
  exporters:        linux_make, vs2017, xcode_iphone, xcode_mac

  moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1, JUCE_PLUGINHOST_AU=1, JUCE_PLUGINHOST_VST3=1
  defines:          TRACKTION_UNIT_TESTS=1

  type:             Component
  mainClass:        TestRunner

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once


//==============================================================================
class TestRunner  : public Component
{
public:
    //==============================================================================
    TestRunner()
    {
        setSize (600, 400);

        UnitTestRunner testRunner;
        testRunner.setAssertOnFailure (false);
        testRunner.runTestsInCategory ("Tracktion");
        testRunner.runTestsInCategory ("Tracktion:Longer");

        int numFailues = 0;

        for (int i = 0; i <= testRunner.getNumResults(); ++i)
            if (auto result = testRunner.getResult (i))
                numFailues += result->failures;

        if (numFailues > 0)
            JUCEApplication::getInstance()->setApplicationReturnValue (1);

        JUCEApplication::getInstance()->quit();        
    }

    //==============================================================================
    void paint (Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    }

private:
    //==============================================================================
    tracktion_engine::Engine engine { ProjectInfo::projectName };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TestRunner)
};
