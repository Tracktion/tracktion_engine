/*******************************************************************************
The block below describes the properties of this PIP. A PIP is a short snippet
of code that can be read by the Projucer and used to generate a JUCE project.

BEGIN_JUCE_PIP_METADATA

name:             IRPluginDemo
version:          0.0.1
vendor:           Tracktion
website:          www.tracktion.com
description:      This example loads an Impulse Response and performs convolution on a looping audio file.

dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_processors, juce_audio_utils,
            juce_core, juce_data_structures, juce_dsp, juce_events, juce_graphics,
            juce_gui_basics, juce_gui_extra, juce_osc, tracktion_engine
exporters:        linux_make, vs2017, xcode_iphone, xcode_mac

moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1, JUCE_PLUGINHOST_AU=1, JUCE_PLUGINHOST_VST3=1, JUCE_MODAL_LOOPS_PERMITTED=1

type:             Component
mainClass:        IRPluginDemo

END_JUCE_PIP_METADATA

*******************************************************************************/

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
    IRPluginDemo()
    {
        // Register our custom plugin with the engine so it can be found using PluginCache::createNewPlugin
        engine.getPluginManager().createBuiltInType<ImpulseResponsePlugin>();
        
        Helpers::addAndMakeVisible (*this, { &preGainSlider, &postGainSlider, &highPassCutoffSlider, &lowPassCutoffSlider,
                                             &settingsButton, &playPauseButton, &IRButton, &preGainLabel, &postGainLabel,
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
                                           { { 0.0, audioFile.getLength() }, 0.0 }, false);
        jassert (clip != nullptr);

        // Creates new instance of IR Loader Plugin and inserts to track 1
        auto plugin = edit.getPluginCache().createNewPlugin (ImpulseResponsePlugin::xmlTypeName, {});
        track->pluginList.insertPlugin (plugin, 0, nullptr);
        
        // Set the loop points to the start/end of the clip, enable looping and start playback
        edit.getTransport().addChangeListener (this);
        EngineHelpers::loopAroundClip (*clip);

        // Setup button callbacks
        playPauseButton.onClick = [this] { EngineHelpers::togglePlay (edit); };
        settingsButton.onClick =  [this] { EngineHelpers::showAudioDeviceSettings (engine); };
        IRButton.onClick =        [this] { showIRButtonMenu(); };

        // Setup slider value source
        auto preGainParam = plugin->getAutomatableParameterByID ("preGain");
        bindSliderToParameter (preGainSlider, *preGainParam);
        preGainSlider.setSkewFactorFromMidPoint (1.0);
        preGainLabel.attachToComponent (&preGainSlider,true);
        preGainLabel.setText ("Pre",sendNotification);

        auto postGainParam = plugin->getAutomatableParameterByID ("postGain");
        bindSliderToParameter (postGainSlider, *postGainParam);
        postGainSlider.setSkewFactorFromMidPoint (1.0);
        postGainLabel.attachToComponent (&postGainSlider, true);
        postGainLabel.setText ("Post", sendNotification);

        auto highPassCutoffParam = plugin->getAutomatableParameterByID ("highPassCutoff");
        bindSliderToParameter (highPassCutoffSlider, *highPassCutoffParam);
        highPassCutoffSlider.setSkewFactorFromMidPoint (2000.0);
        highPassCutoffLabel.attachToComponent (&highPassCutoffSlider, true);
        highPassCutoffLabel.setText ("HPF", sendNotification);

        auto lowPassCutoffParam = plugin->getAutomatableParameterByID ("lowPassCutoff");
        bindSliderToParameter (lowPassCutoffSlider, *lowPassCutoffParam);
        lowPassCutoffSlider.setSkewFactorFromMidPoint (2000.0);
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
        IRButton.setBounds (topR.removeFromLeft (topR.getWidth() / 3).reduced (2));
        settingsButton.setBounds (topR.removeFromLeft (topR.getWidth() / 2).reduced (2));
        playPauseButton.setBounds (topR.reduced (2));

        preGainSlider.setBounds   (50, 50,  500, 50);
        highPassCutoffSlider.setBounds (50, 100, 500, 50);
        lowPassCutoffSlider.setBounds (50, 150, 500, 50);
        postGainSlider.setBounds  (50, 200, 500, 50);
    }

private:
    //==============================================================================
    te::Engine engine { ProjectInfo::projectName };
    te::Edit edit { Edit::Options { engine, te::createEmptyEdit (engine), ProjectItemID::createNewID (0) } };
    std::unique_ptr<TemporaryFile> oggTempFile, IRTempFile;

    TextButton IRButton { "Load IR" }, settingsButton { "Settings" }, playPauseButton { "Play" };
    Slider preGainSlider, postGainSlider, highPassCutoffSlider, lowPassCutoffSlider;

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
