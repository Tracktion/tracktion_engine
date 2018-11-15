/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

  name:             PlaybackDemo
  version:          0.0.1
  vendor:           Tracktion
  website:          www.tracktion.com
  description:      This example simply loads a project from the command line and plays it back in a loop.

  dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_processors, juce_audio_utils,
                    juce_core, juce_data_structures, juce_dsp, juce_events, juce_graphics,
                    juce_gui_basics, juce_gui_extra, tracktion_engine
  exporters:        linux_make, vs2017, xcode_iphone, xcode_mac

  moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1, JUCE_PLUGINHOST_AU=1

  type:             Component
  mainClass:        PlaybackDemo

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include "common/Utilities.h"


//==============================================================================
class PlaybackDemo  : public Component,
                      private ChangeListener
{
public:
    //==============================================================================
    PlaybackDemo()
    {
        const auto editFilePath = JUCEApplication::getCommandLineParameterArray()[0];
        jassert (editFilePath.isNotEmpty());
        const File editFile (editFilePath);

        if (editFile.existsAsFile())
        {
            edit = std::make_unique<te::Edit> (engine, te::loadEditFromFile (editFile, {}), te::Edit::forEditing, nullptr, 0);
            auto& transport = edit->getTransport();
            transport.setLoopRange ({ 0.0, edit->getLength() });
            transport.looping = true;
            transport.play (false);
            transport.addChangeListener (this);

            editNameLabel.setText (editFile.getFileNameWithoutExtension(), dontSendNotification);
            playPauseButton.onClick = [this] { EngineHelpers::togglePlay (*edit); };
        }
        else
        {
            jassertfalse;
        }

        settingsButton.onClick  = [this] { EngineHelpers::showAudioDeviceSettings (engine); };
        updatePlayButtonText();
        editNameLabel.setJustificationType (Justification::centred);
        Helpers::addAndMakeVisible (*this, { &settingsButton, &playPauseButton, &editNameLabel });

        setSize (600, 400);
    }

    ~PlaybackDemo()
    {
        engine.getTemporaryFileManager().getTempDirectory().deleteRecursively();
    }

    //==============================================================================
    void paint (Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto topR = r.removeFromTop (30);
        settingsButton.setBounds (topR.removeFromLeft (topR.getWidth() / 2).reduced (2));
        playPauseButton.setBounds (topR.reduced (2));
        editNameLabel.setBounds (r);
    }

private:
    //==============================================================================
    te::Engine engine { ProjectInfo::projectName };
    std::unique_ptr<te::Edit> edit;

    TextButton settingsButton { "Settings" }, playPauseButton { "Play" };
    Label editNameLabel { "No Edit Loaded" };

    //==============================================================================
    void updatePlayButtonText()
    {
        playPauseButton.setButtonText (edit->getTransport().isPlaying() ? "Pause" : "Play");
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        updatePlayButtonText();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackDemo)
};
