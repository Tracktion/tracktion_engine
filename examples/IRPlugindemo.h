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

using namespace tracktion_engine;

namespace tracktion_engine
{
    //==============================================================================
    class IRPlugin    :  public Plugin
    {
    public:
        static const char* getPluginName() { return NEEDS_TRANS ("IRPluginDemo"); }
        static const char* xmlTypeName;

		IRPlugin (PluginCreationInfo info)    :  Plugin (info)
        {
            auto um = getUndoManager();
            preGainValue.referTo   (state, "preGain", um, 1.0f);
            postGainValue.referTo  (state, "postGain", um, 1.0f);
            HPFCutoffValue.referTo (state, "HPFCutoff", um, 0);
            LPFCutoffValue.referTo (state, "LPFCutoff", um, 20000);

            //Initializes parameter and attaches to value
            preGainParam = addParam ("preGain", TRANS ("PreGain"), { 0.1f, 20.0f });
            preGainParam->attachToCurrentValue (preGainValue);

            postGainParam = addParam ("postGain", TRANS ("PostGain"), { 0.1f, 20.0f });
            postGainParam->attachToCurrentValue (postGainValue);

            HPFCutoffParam = addParam ("HPFCutoff", TRANS ("HPFCutoff"), { 0.1f, 20000.0f });
            HPFCutoffParam->attachToCurrentValue (HPFCutoffValue);

            LPFCutoffParam = addParam ("LPFCutoff", TRANS ("LPFCutoff"), { 0.1f, 20000.0f });
            LPFCutoffParam->attachToCurrentValue (LPFCutoffValue);

            // Load IR file
            auto& HPF = processorChain.get <HPFIndex>();
            HPF.setMode (dsp::LadderFilterMode::HPF24);

            auto& LPF = processorChain.get <LPFIndex>();
            LPF.setMode (dsp::LadderFilterMode::LPF24);
        }

        ~IRPlugin() override
        {
            notifyListenersOfDeletion();
            preGainParam->detachFromCurrentValue();
            postGainParam->detachFromCurrentValue();
            HPFCutoffParam->detachFromCurrentValue();
            LPFCutoffParam->detachFromCurrentValue();
        }

        juce::String getName() override                                     { return getPluginName(); }
        juce::String getPluginType() override                               { return xmlTypeName; }
        bool needsConstantBufferSize() override                             { return false; }
        juce::String getSelectableDescription() override                    { return getName(); }

        void initialise (const PluginInitialisationInfo&) override          
        {
            processSpec.maximumBlockSize = blockSizeSamples;
            processSpec.numChannels = 2;
            processSpec.sampleRate = sampleRate;
            prepare (processSpec);
        }

        void deinitialise() override                                        { }

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            processorChain.prepare (spec);
        }

        enum
        {
            preGainIndex,
            convolutionIndex,
            HPFIndex,
            LPFIndex,
            postGainIndex
        };

        void loadIRFile (const File& fileImpulseResponse, size_t sizeInBytes, dsp::Convolution::Stereo isStereo, dsp::Convolution::Trim requiresTrimming)
        {
            processorChain.get<convolutionIndex>().loadImpulseResponse (fileImpulseResponse, isStereo, requiresTrimming, sizeInBytes);
        }

        void loadIRFile(AudioSampleBuffer&& bufferImpulseResponse, dsp::Convolution::Stereo isStereo, dsp::Convolution::Trim requiresTrimming, dsp::Convolution::Normalise requiresNormalisation)
        {
            processorChain.get<convolutionIndex>().loadImpulseResponse (std::move (bufferImpulseResponse), sampleRate, isStereo, requiresTrimming, requiresNormalisation);
        }

        void reset() noexcept
        {
            processorChain.reset();
        }
        
        void applyToBuffer (const PluginRenderContext& fc) override
        {
            auto& preGain = processorChain.get <preGainIndex>();
            preGain.setGainLinear (preGainValue);

            auto& HPF = processorChain.get <HPFIndex>();
            HPF.setCutoffFrequencyHz (HPFCutoffValue);
            HPF.setResonance (0.7f);

            auto& LPF = processorChain.get <LPFIndex>();
            LPF.setCutoffFrequencyHz (LPFCutoffValue);
            LPF.setResonance (0.7f);

            auto& postGain = processorChain.get <postGainIndex>();
            postGain.setGainLinear (postGainValue);

            dsp::AudioBlock <float> inoutBlock (*fc.destBuffer);
            dsp::ProcessContextReplacing <float> context (inoutBlock);
            processorChain.process (context);
        }

        void restorePluginStateFromValueTree (const juce::ValueTree& v) override
        {
            CachedValue <float>* cvsFloat[] = { &preGainValue, &postGainValue, &HPFCutoffValue, &LPFCutoffValue, nullptr };
            copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);

            for (auto p    :  getAutomatableParameters())
                p->updateFromAttachedValue();
        }

        juce::CachedValue <float> preGainValue, postGainValue;
        AutomatableParameter::Ptr preGainParam, postGainParam;

        juce::CachedValue <float> HPFCutoffValue, LPFCutoffValue;
        AutomatableParameter::Ptr HPFCutoffParam, LPFCutoffParam;

    private:
        dsp::ProcessorChain <dsp::Gain<float>, 
                             dsp::Convolution,
                             dsp::LadderFilter<float>,
                             dsp::LadderFilter<float>,
                             dsp::Gain<float>> processorChain;

        dsp::ProcessSpec processSpec;

        std::unique_ptr<TemporaryFile> IRTempFile;

        AudioSampleBuffer IRBuffer;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IRPlugin)
    };

    const char* IRPlugin::xmlTypeName = "IRPlugin";


}


//==============================================================================
//==============================================================================
/**
    Wraps a te::AutomatableParameter as a juce::ValueSource so it can be used as
    a Value for example in a Slider.
*/
class ParameterValueSource    :  public juce::Value::ValueSource,
                                 private AutomatableParameter::Listener
{
public:
    ParameterValueSource (AutomatableParameter::Ptr p)    :  param (p)
    {
        param->addListener (this);
    }
    
    ~ParameterValueSource() override
    {
        param->removeListener (this);
    }
    
    var getValue() const override
    {
        return param->getCurrentValue();
    }

    void setValue (const var& newValue) override
    {
        param->setParameter (static_cast<float> (newValue), juce::sendNotification);
    }

private:
    AutomatableParameter::Ptr param;
    
    void curveHasChanged (AutomatableParameter&) override
    {
        sendChangeMessage (false);
    }
    
    void currentValueChanged (AutomatableParameter&, float /*newValue*/) override
    {
        sendChangeMessage (false);
    }
};

//==============================================================================
/** Binds an te::AutomatableParameter to a juce::Slider so changes in either are
    reflected across the other.
*/
void bindSliderToParameter (juce::Slider& s, AutomatableParameter& p)
{
    const auto v = p.valueRange;
    const auto range = NormalisableRange <double> (static_cast<double> (v.start),
                                                  static_cast<double> (v.end),
                                                  static_cast<double> (v.interval),
                                                  static_cast<double> (v.skew),
                                                  v.symmetricSkew);

    s.setNormalisableRange (range);
    s.getValueObject().referTo (juce::Value (new ParameterValueSource (p)));
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
        engine.getPluginManager().createBuiltInType <IRPlugin>();
        
        Helpers::addAndMakeVisible (*this, { &preGainSlider, &postGainSlider, &HPFCutoffSlider, &LPFCutoffSlider, &settingsButton, &playPauseButton, &IRButton, &preGainLabel, &postGainLabel, &HPFCutoffLabel, &LPFCutoffLabel });

        // Load demo audio file
        oggTempFile = std::make_unique <TemporaryFile> (".ogg");
        auto demoFile = oggTempFile->getFile();      
        demoFile.replaceWithData (PlaybackDemoAudio::BITs_Export_2_ogg, PlaybackDemoAudio::BITs_Export_2_oggSize);

        // Creates clip. Loads clip from file f.
        // Creates track. Loads clip into track.
        auto track = EngineHelpers::getOrInsertAudioTrackAt (edit, 0);
        jassert (track != nullptr);
        
        // Add a new clip to this track
        te::AudioFile audioFile (edit.engine, demoFile);

        auto clip = track->insertWaveClip (demoFile.getFileNameWithoutExtension(), demoFile,
                                           { { 0.0, audioFile.getLength() }, 0.0 }, false);
        jassert (clip != nullptr);

        // Creates new instance of IR Loader Plugin and inserts to track 1
        auto plugin = edit.getPluginCache().createNewPlugin (IRPlugin::xmlTypeName, {});
                
        track->pluginList.insertPlugin (plugin, 0, nullptr);
        // Set the loop points to the start/end of the clip, enable looping and start playback
        edit.getTransport().addChangeListener (this);
        EngineHelpers::loopAroundClip (*clip);

        // Setup button callbacks
        playPauseButton.onClick = [this] { EngineHelpers::togglePlay (edit); };
        settingsButton.onClick =  [this] { EngineHelpers::showAudioDeviceSettings (engine); };
        IRButton.onClick =        [this] { loadIRFileIntoPluginBuffer(); };

        // Setup slider value source
        auto preGainParam = plugin->getAutomatableParameterByID ("preGain");
        bindSliderToParameter (preGainSlider, *preGainParam);
        preGainLabel.attachToComponent (&preGainSlider,true);
        preGainLabel.setText ("Pre",sendNotification);

        auto postGainParam = plugin->getAutomatableParameterByID ("postGain");
        bindSliderToParameter (postGainSlider, *postGainParam);
        postGainLabel.attachToComponent (&postGainSlider, true);
        postGainLabel.setText ("Post", sendNotification);

        auto HPFCutoffParam = plugin->getAutomatableParameterByID ("HPFCutoff");
        bindSliderToParameter (HPFCutoffSlider, *HPFCutoffParam);
        HPFCutoffLabel.attachToComponent (&HPFCutoffSlider, true);
        HPFCutoffLabel.setText ("HPF", sendNotification);

        auto LPFCutoffParam = plugin->getAutomatableParameterByID ("LPFCutoff");
        bindSliderToParameter (LPFCutoffSlider, *LPFCutoffParam);
        LPFCutoffLabel.attachToComponent (&LPFCutoffSlider, true);
        LPFCutoffLabel.setText ("LPF", sendNotification);

        updatePlayButtonText();

        setSize (600, 400);
    }

    std::unique_ptr <FileChooser> myChooser;
    File wavFile;
    AudioFormatManager formatManager;
    AudioSampleBuffer fileBuffer;

    void loadIRFileIntoPluginBuffer()
    {
        myChooser = std::make_unique <juce::FileChooser> ("Select an impulse...",
                                                          juce::File{},
                                                          "*.wav");

        auto chooserFlags = juce::FileBrowserComponent::openMode
                          | juce::FileBrowserComponent::canSelectFiles;

        formatManager.registerBasicFormats();

        myChooser->launchAsync (chooserFlags, [this] (const FileChooser& chooser)
        {
                wavFile = File (chooser.getResult());
                std::unique_ptr <juce::AudioFormatReader> reader (formatManager.createReaderFor (wavFile));

                fileBuffer.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
                reader->read (&fileBuffer,
                              0,
                              (int) reader->lengthInSamples,
                              0,
                              true,
                              true);

                auto plugin = EngineHelpers::getOrInsertAudioTrackAt (edit, 0)->pluginList.getPluginsOfType <IRPlugin>()[0];
                plugin->loadIRFile (std::move (fileBuffer), dsp::Convolution::Stereo::yes, dsp::Convolution::Trim::no,dsp::Convolution::Normalise::yes);
        });
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
        HPFCutoffSlider.setBounds (50, 100, 500, 50);
        LPFCutoffSlider.setBounds (50, 150, 500, 50);
        postGainSlider.setBounds  (50, 200, 500, 50);
    }

private:
    //==============================================================================
    te::Engine engine { ProjectInfo::projectName };
    te::Edit edit { Edit::Options { engine, te::createEmptyEdit (engine), ProjectItemID::createNewID (0) } };
    std::unique_ptr<TemporaryFile> oggTempFile,IRTempFile;

    TextButton IRButton { "Load IR" }, settingsButton { "Settings" }, playPauseButton { "Play" };
    Slider preGainSlider, postGainSlider, HPFCutoffSlider, LPFCutoffSlider;

    Label preGainLabel, postGainLabel, HPFCutoffLabel, LPFCutoffLabel;

    //==============================================================================
    void updatePlayButtonText()
    {
        playPauseButton.setButtonText (edit.getTransport().isPlaying() ? "Pause"    : "Play");
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        updatePlayButtonText();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IRPluginDemo)
};

