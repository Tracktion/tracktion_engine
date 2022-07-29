/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "common/Utilities.h"
#include "common/PlaybackDemoAudio.h"

using namespace tracktion::literals;
using namespace std::literals;

//==============================================================================
class PlaybackDemo  : public Component,
                      private ChangeListener,
                      private Timer
{
public:
    //==============================================================================
    PlaybackDemo (te::Engine& e)
        : engine (e)
    {
        const auto editFilePath = JUCEApplication::getCommandLineParameters().replace ("-NSDocumentRevisionsDebugMode YES", "").unquoted().trim();

        const File editFile (editFilePath);

        if (editFile.existsAsFile())
        {
            edit = te::loadEditFromFile (engine, editFile);
            auto& transport = edit->getTransport();
            transport.setLoopRange ({ 0_tp, edit->getLength() });
            transport.looping = true;
            transport.play (false);
            transport.addChangeListener (this);

            editNameLabel.setText (editFile.getFileNameWithoutExtension(), dontSendNotification);
        }
        else
        {
            auto f = File::createTempFile (".ogg");
            f.replaceWithData (PlaybackDemoAudio::BITs_Export_2_ogg, PlaybackDemoAudio::BITs_Export_2_oggSize);

            edit = te::createEmptyEdit (engine, editFile);
            edit->getTransport().addChangeListener (this);
            auto clip = EngineHelpers::loadAudioFileAsClip (*edit, f);
            EngineHelpers::loopAroundClip (*clip);

            editNameLabel.setText ("Demo Song", dontSendNotification);
        }

        playPauseButton.onClick = [this] { EngineHelpers::togglePlay (*edit); };

        updatePlayButtonText();
        editNameLabel.setJustificationType (Justification::centred);
        cpuLabel.setJustificationType (Justification::centred);
        Helpers::addAndMakeVisible (*this, { &playPauseButton, &editNameLabel, &cpuLabel });

        setSize (600, 400);
        startTimerHz (5);
    }

    ~PlaybackDemo() override
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
        playPauseButton.setBounds (topR.reduced (2));

        auto middleR = r.withSizeKeepingCentre (r.getWidth(), 40);

        cpuLabel.setBounds (middleR.removeFromBottom (20));
        editNameLabel.setBounds (middleR);
    }

private:
    //==============================================================================
    te::Engine& engine;
    std::unique_ptr<te::Edit> edit;

    TextButton playPauseButton { "Play" };
    Label editNameLabel { "No Edit Loaded" }, cpuLabel;

    //==============================================================================
    void updatePlayButtonText()
    {
        if (edit != nullptr)
            playPauseButton.setButtonText (edit->getTransport().isPlaying() ? "Pause" : "Play");
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        updatePlayButtonText();
    }

    void timerCallback() override
    {
        cpuLabel.setText (String::formatted ("CPU: %d%%", int (engine.getDeviceManager().getCpuUsage() * 100)), dontSendNotification);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackDemo)
};

//==============================================================================
static DemoTypeBase<PlaybackDemo> playbackDemo ("Playback");
