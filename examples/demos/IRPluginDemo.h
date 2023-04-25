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
#include "common/Components.h"
#include "common/PlaybackDemoAudio.h"
#include "common/IRData.h"

// Include Slider Parameter binding classes and functions
#include "DistortionEffectDemo.h"

using namespace tracktion_engine;

//==============================================================================
//==============================================================================
class IRPluginDemo    :  public Component,
                         private ChangeListener
{
public:
    //==============================================================================
    IRPluginDemo (Engine& e)
        : engine (e)
    {
        // Register our custom plugin with the engine so it can be found using PluginCache::createNewPlugin
        engine.getPluginManager().createBuiltInType<ImpulseResponsePlugin>();
        
        Helpers::addAndMakeVisible (*this, { &preGainSlider, &highPassCutoffSlider, &lowPassCutoffSlider,
                                             &playPauseButton, &IRButton, &preGainLabel, &postGainLabel,
                                             &highPassCutoffLabel, &lowPassCutoffLabel });

        // Load demo audio file
        oggTempFile = std::make_unique<TemporaryFile> (".ogg");
        auto demoFile = oggTempFile->getFile();      
        demoFile.replaceWithData (PlaybackDemoAudio::guitar_loop_ogg, PlaybackDemoAudio::guitar_loop_oggSize);

        // Creates clip. Loads clip from file f
        // Creates track. Loads clip into track
        auto track = EngineHelpers::getOrInsertAudioTrackAt (edit, 0);
        jassert (track != nullptr);
        
        // Add a new clip to this track
        te::AudioFile audioFile (edit.engine, demoFile);

        auto clip = track->insertWaveClip (demoFile.getFileNameWithoutExtension(), demoFile,
                                           { { {}, tracktion::TimePosition::fromSeconds (audioFile.getLength()) }, {} }, false);
        jassert (clip != nullptr);

        // Creates new instance of IR Loader Plugin and inserts to track 1
        auto plugin = edit.getPluginCache().createNewPlugin (ImpulseResponsePlugin::xmlTypeName, {});
        track->pluginList.insertPlugin (plugin, 0, nullptr);
        
        // Set the loop points to the start/end of the clip, enable looping and start playback
        edit.getTransport().addChangeListener (this);
        EngineHelpers::loopAroundClip (*clip);

        // Setup button callbacks
        playPauseButton.onClick = [this] { EngineHelpers::togglePlay (edit); };
        IRButton.onClick =        [this] { showIRButtonMenu(); };

        // Setup slider value source
        auto gainParam = plugin->getAutomatableParameterByID ("gain");
        bindSliderToParameter (preGainSlider, *gainParam);
        preGainSlider.setSkewFactorFromMidPoint (1.0);
        preGainLabel.attachToComponent (&preGainSlider,true);
        preGainLabel.setText ("Pre",sendNotification);

        auto highPassCutoffParam = plugin->getAutomatableParameterByID ("highPassMidiNoteNumber");
        bindSliderToParameter (highPassCutoffSlider, *highPassCutoffParam);
        highPassCutoffLabel.attachToComponent (&highPassCutoffSlider, true);
        highPassCutoffLabel.setText ("HPF", sendNotification);

        auto lowPassCutoffParam = plugin->getAutomatableParameterByID ("lowPassMidiNoteNumber");
        bindSliderToParameter (lowPassCutoffSlider, *lowPassCutoffParam);
        lowPassCutoffLabel.attachToComponent (&lowPassCutoffSlider, true);
        lowPassCutoffLabel.setText ("LPF", sendNotification);

        updatePlayButtonText();

        setSize (600, 400);
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
        IRButton.setBounds (topR.removeFromLeft (topR.getWidth() / 2).reduced (2));
        playPauseButton.setBounds (topR.reduced (2));

        preGainSlider.setBounds   (50, 50,  500, 50);
        highPassCutoffSlider.setBounds (50, 100, 500, 50);
        lowPassCutoffSlider.setBounds (50, 150, 500, 50);
    }

private:
    //==============================================================================
    te::Engine& engine;
    te::Edit edit { Edit::Options { engine, te::createEmptyEdit (engine), ProjectItemID::createNewID (0) } };
    std::unique_ptr<TemporaryFile> oggTempFile, IRTempFile;

    TextButton IRButton { "Load IR" }, playPauseButton { "Play" };
    Slider preGainSlider, highPassCutoffSlider, lowPassCutoffSlider;

    Label preGainLabel, postGainLabel, highPassCutoffLabel, lowPassCutoffLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    //==============================================================================
    void showIRButtonMenu()
    {
        PopupMenu m;
        m.addItem ("Guitar Amp",        [this] { loadIRFileIntoPluginBuffer (IRs::guitar_amp_wav, IRs::guitar_amp_wavSize); });
        m.addItem ("Casette Recorder",  [this] { loadIRFileIntoPluginBuffer (IRs::cassette_recorder_wav, IRs::cassette_recorder_wavSize); });
        m.addSeparator();
        m.addItem ("Load from file...", [this] { loadIRFileIntoPluginBuffer(); });
        
        m.showMenuAsync ({});
    }

    void loadIRFileIntoPluginBuffer (const void* sourceData, size_t sourceDataSize)
    {
        auto plugin = EngineHelpers::getOrInsertAudioTrackAt (edit, 0)->pluginList.findFirstPluginOfType<ImpulseResponsePlugin>();
        plugin->loadImpulseResponse (sourceData, sourceDataSize);

    }
    
    void loadIRFileIntoPluginBuffer()
    {
        if (! fileChooser)
            fileChooser = std::make_unique<juce::FileChooser> ("Select an impulse...", juce::File{}, "*.wav");

        const auto chooserFlags = juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles;

        fileChooser->launchAsync (chooserFlags, [this] (const FileChooser& chooser)
        {
            AudioFormatManager formatManager;
            formatManager.registerBasicFormats();

            if (auto reader = std::unique_ptr<juce::AudioFormatReader> (formatManager.createReaderFor (chooser.getResult())))
            {
                juce::AudioSampleBuffer fileBuffer ((int) reader->numChannels, (int) reader->lengthInSamples);
                reader->read (&fileBuffer,
                              0,
                              (int) reader->lengthInSamples,
                              0,
                              true,
                              true);

                auto plugin = EngineHelpers::getOrInsertAudioTrackAt (edit, 0)->pluginList.findFirstPluginOfType<ImpulseResponsePlugin>();
                plugin->loadImpulseResponse (std::move (fileBuffer), reader->sampleRate, (int) reader->bitsPerSample);
            }
        });
    }

    void updatePlayButtonText()
    {
        playPauseButton.setButtonText (edit.getTransport().isPlaying() ? "Pause" : "Play");
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        updatePlayButtonText();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IRPluginDemo)
};

//==============================================================================
static DemoTypeBase<IRPluginDemo> irPluginDemo ("Impulse Response Plugin");
