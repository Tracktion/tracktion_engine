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

namespace tracktion_engine
{
    //==============================================================================
    /**
        ImpulseResponsePlugin that loads an impulse response and applies it the audio stream.
        Additionally this has high and low pass filters to shape the sound.
    */
    class ImpulseResponsePlugin    :  public Plugin
    {
    public:
        /** Creates an ImpulseResponsePlugin.
            Initially this will be have no IR loaded, use one of the loadImpulseResponse
            methods to apply it to the audio.
        */
		ImpulseResponsePlugin (PluginCreationInfo info)
            :  Plugin (info)
        {
            auto um = getUndoManager();
            preGainValue.referTo   (state, "preGain", um, 1.0f);
            postGainValue.referTo  (state, "postGain", um, 1.0f);
            highPassCutoffValue.referTo (state, "highPassCutoff", um, 0.1f);
            lowPassCutoffValue.referTo (state, "lowPassCutoff", um, 20000.0f);

            // Initialises parameter and attaches to value
            preGainParam = addParam ("preGain", TRANS("Pre Gain"), { 0.1f, 20.0f });
            preGainParam->attachToCurrentValue (preGainValue);

            postGainParam = addParam ("postGain", TRANS("Post Gain"), { 0.1f, 20.0f });
            postGainParam->attachToCurrentValue (postGainValue);

            highPassCutoffParam = addParam ("highPassCutoff", TRANS("High Pass Filter Cutoff"), { 0.1f, 20000.0f });
            highPassCutoffParam->attachToCurrentValue (highPassCutoffValue);

            lowPassCutoffParam = addParam ("lowPassCutoff", TRANS("Low Pass Filter Cutoff"), { 0.1f, 20000.0f });
            lowPassCutoffParam->attachToCurrentValue (lowPassCutoffValue);
        }

        /** Destructor. */
        ~ImpulseResponsePlugin() override
        {
            notifyListenersOfDeletion();
            preGainParam->detachFromCurrentValue();
            postGainParam->detachFromCurrentValue();
            highPassCutoffParam->detachFromCurrentValue();
            lowPassCutoffParam->detachFromCurrentValue();
        }

        static const char* getPluginName() { return NEEDS_TRANS("Impulse Response"); }
        static inline const char* xmlTypeName = "impulseResponse";

        //==============================================================================
        /** Loads an impulse from binary audio file data i.e. not a block of raw floats.
            @see juce::Convolution::loadImpulseResponse
        */
        void loadImpulseResponse (const void* sourceData, size_t sourceDataSize,
                                  dsp::Convolution::Stereo isStereo, dsp::Convolution::Trim requiresTrimming, size_t size,
                                  dsp::Convolution::Normalise requiresNormalisation)
        {
            processorChain.get<convolutionIndex>().loadImpulseResponse (sourceData, sourceDataSize, isStereo, requiresTrimming, size, requiresNormalisation);
        }

        /** Loads an impulse from a file.
            @see juce::Convolution::loadImpulseResponse
        */
        void loadImpulseResponse (const File& fileImpulseResponse, size_t sizeInBytes, dsp::Convolution::Stereo isStereo, dsp::Convolution::Trim requiresTrimming)
        {
            processorChain.get<convolutionIndex>().loadImpulseResponse (fileImpulseResponse, isStereo, requiresTrimming, sizeInBytes);
            
        }

        /** Loads an impulse from an AudioBuffer<float>.
            @see juce::Convolution::loadImpulseResponse
        */
        void loadImpulseResponse (AudioBuffer<float>&& bufferImpulseResponse, dsp::Convolution::Stereo isStereo, dsp::Convolution::Trim requiresTrimming, dsp::Convolution::Normalise requiresNormalisation)
        {
            processorChain.get<convolutionIndex>().loadImpulseResponse (std::move (bufferImpulseResponse), sampleRate, isStereo, requiresTrimming, requiresNormalisation);
        }

        //==============================================================================
        juce::CachedValue<float> preGainValue, postGainValue;
        juce::CachedValue<float> highPassCutoffValue, lowPassCutoffValue;

        AutomatableParameter::Ptr preGainParam;         /**< Parameter for the Gain to apply before the IR */
        AutomatableParameter::Ptr highPassCutoffParam;  /**< Cutoff frequency for the high pass filter to applied after the IR */
        AutomatableParameter::Ptr lowPassCutoffParam;   /**< Cutoff frequency for the low pass filter to applied after the IR */
        AutomatableParameter::Ptr postGainParam;        /**< Parameter for the Gain to apply after the IR */

        //==============================================================================
        /** @internal */
        juce::String getName() override                                     { return getPluginName(); }
        /** @internal */
        juce::String getPluginType() override                               { return xmlTypeName; }
        /** @internal */
        bool needsConstantBufferSize() override                             { return false; }
        /** @internal */
        juce::String getSelectableDescription() override                    { return getName(); }
        
        /** @internal */
        double getLatencySeconds() override
        {
            return processorChain.get<convolutionIndex>().getLatency() / sampleRate;
        }

        /** @internal */
        void initialise (const PluginInitialisationInfo& info) override
        {
            dsp::ProcessSpec processSpec;
            processSpec.sampleRate          = info.sampleRate;
            processSpec.maximumBlockSize    = (uint32_t) info.blockSizeSamples;
            processSpec.numChannels         = 2;
            processorChain.prepare (processSpec);
        }

        /** @internal */
        void deinitialise() override
        {}

        /** @internal */
        void reset() override
        {
            processorChain.reset();
        }
        
        /** @internal */
        void applyToBuffer (const PluginRenderContext& fc) override
        {
            auto& preGain = processorChain.get<preGainIndex>();
            preGain.setGainLinear (preGainParam->getCurrentValue());

            auto hpf = processorChain.get<HPFIndex>().state;
            *hpf = dsp::IIR::ArrayCoefficients<float>::makeHighPass (sampleRate, highPassCutoffParam->getCurrentValue());

            auto& lpf = processorChain.get<LPFIndex>().state;
            *lpf = dsp::IIR::ArrayCoefficients<float>::makeLowPass (sampleRate, lowPassCutoffParam->getCurrentValue());

            auto& postGain = processorChain.get<postGainIndex>();
            postGain.setGainLinear (postGainParam->getCurrentValue());

            dsp::AudioBlock <float> inoutBlock (*fc.destBuffer);
            dsp::ProcessContextReplacing <float> context (inoutBlock);
            processorChain.process (context);
        }

        /** @internal */
        void restorePluginStateFromValueTree (const juce::ValueTree& v) override
        {
            CachedValue <float>* cvsFloat[] = { &preGainValue, &postGainValue, &highPassCutoffValue, &lowPassCutoffValue, nullptr };
            copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);

            for (auto p : getAutomatableParameters())
                p->updateFromAttachedValue();
        }

    private:
        //==============================================================================
        enum
        {
            preGainIndex,
            convolutionIndex,
            HPFIndex,
            LPFIndex,
            postGainIndex
        };

        dsp::ProcessorChain<dsp::Gain<float>,
                            dsp::Convolution,
                            dsp::ProcessorDuplicator<dsp::IIR::Filter<float>, dsp::IIR::Coefficients<float>>,
                            dsp::ProcessorDuplicator<dsp::IIR::Filter<float>, dsp::IIR::Coefficients<float>>,
                            dsp::Gain<float>> processorChain;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ImpulseResponsePlugin)
    };
}


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
        demoFile.replaceWithData (PlaybackDemoAudio::BITs_Export_2_ogg, PlaybackDemoAudio::BITs_Export_2_oggSize);

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
        plugin->loadImpulseResponse (sourceData, sourceDataSize,
                                     dsp::Convolution::Stereo::yes, dsp::Convolution::Trim::no, 0, dsp::Convolution::Normalise::yes);

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
                plugin->loadImpulseResponse (std::move (fileBuffer), dsp::Convolution::Stereo::yes, dsp::Convolution::Trim::no, dsp::Convolution::Normalise::yes);
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
