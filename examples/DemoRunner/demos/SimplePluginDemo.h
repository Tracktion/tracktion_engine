/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "../../common/Utilities.h"
#include "../../common/PlaybackDemoAudio.h"

using namespace tracktion::literals;
using namespace std::literals;

//==============================================================================
class SimplePluginDemo    : public juce::Component,
                            public te::SelectableListener,
                            private juce::ChangeListener,
                            private juce::Timer
{
public:
    //==============================================================================
    SimplePluginDemo (te::Engine& e)
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
            f.replaceWithData (PlaybackDemoAudio::guitar_loop_ogg, PlaybackDemoAudio::guitar_loop_oggSize);

            edit = te::createEmptyEdit (engine, editFile);
            edit->getTransport().addChangeListener (this);
            auto clip = EngineHelpers::loadAudioFileAsClip (*edit, f);
            EngineHelpers::loopAroundClip (*clip);


            editNameLabel.setText ("Demo Song", dontSendNotification);
        }

        playPauseButton.onClick = [this] { EngineHelpers::togglePlay (*edit); };
        loadPluginButton.onClick = [this]
        {
            if (auto track = getAudioTracks (*edit)[0])
            {
                EngineHelpers::removeFXPlugins (*track);
                showMenuAndCreatePluginAsync (track, 0,
                                              [this] (auto plugin)
                                              {
                                                  trackListener.reset (plugin, *this);
                                                  updatePluginParamList();
                                              });
            }
        };
        removePluginButton.onClick = [this]
        {
            if (auto track = getAudioTracks (*edit)[0])
                EngineHelpers::removeFXPlugins (*track);
        };

        updatePlayButtonText();
        editNameLabel.setJustificationType (Justification::centred);
        cpuLabel.setJustificationType (Justification::centred);
        parametersList.setMultiLine (true);
        Helpers::addAndMakeVisible (*this, { &playPauseButton, &loadPluginButton, &removePluginButton, &editNameLabel, &cpuLabel, &parametersList });

        setSize (600, 400);
        startTimerHz (5);
    }

    ~SimplePluginDemo() override
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

        editNameLabel.setBounds (r.removeFromTop (28).reduced (0, 2));
        cpuLabel.setBounds (r.removeFromTop (28).reduced (0, 2));
        playPauseButton.setBounds (r.removeFromTop (28).withWidth (100)
                                   .withX (r.getCentreX() - 50).reduced (0, 2));
        loadPluginButton.setBounds (r.removeFromTop (28).withWidth (100)
                                    .withX (r.getCentreX() - 50).reduced (0, 2));
        removePluginButton.setBounds (r.removeFromTop (28).withWidth (100)
                                     .withX (r.getCentreX() - 50).reduced (0, 2));
        parametersList.setBounds (r.reduced (2));
    }

private:
    //==============================================================================
    te::Engine& engine;
    std::unique_ptr<te::Edit> edit;
    SafeScopedListener trackListener;

    TextButton playPauseButton { "Play" };
    TextButton loadPluginButton { "Load Plugin..." };
    TextButton removePluginButton { "Remove Plugin" };
    Label editNameLabel { "No Edit Loaded" }, cpuLabel;
    TextEditor parametersList;

    //==============================================================================
    void updatePlayButtonText()
    {
        if (edit != nullptr)
            playPauseButton.setButtonText (edit->getTransport().isPlaying() ? "Pause" : "Play");
    }

    void updatePluginParamList()
    {
        if (auto track = getAudioTracks(*edit)[0])
        {
            parametersList.setText ("");

            if (auto p = track->pluginList[0];
                p && p != track->getVolumePlugin()&& p != track->getLevelMeterPlugin())
            {
                juce::StringArray paramList (juce::String ("{} Parameters:").replace("{}", p->getName()));

                for (int i = 1; auto param : p->getAutomatableParameters())
                    paramList.add (juce::String (i++) + ". " + param->getParameterName());

                parametersList.setText (paramList.joinIntoString ("\n"));
            }

        }
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        updatePlayButtonText();
    }

    void timerCallback() override
    {
        cpuLabel.setText (String::formatted ("CPU: %d%%", int (engine.getDeviceManager().getCpuUsage() * 100)), dontSendNotification);
    }

    void selectableObjectAboutToBeDeleted (Selectable*) override
    {}

    void selectableObjectChanged (Selectable*) override
    {
        updatePluginParamList();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimplePluginDemo)
};

//==============================================================================
static DemoTypeBase<SimplePluginDemo> simplePluginDemo ("Simple Plugin ");
