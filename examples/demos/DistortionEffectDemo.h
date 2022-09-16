/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "../common/Utilities.h"
#include "../common/Components.h"
#include "../common/PlaybackDemoAudio.h"

using namespace tracktion_engine;

//==============================================================================
class DistortionPlugin  : public Plugin
{
public:
    static const char* getPluginName() { return NEEDS_TRANS ("DistortionEffectDemo"); }
    static const char* xmlTypeName;

    DistortionPlugin (PluginCreationInfo info) : Plugin (info)
    {
        auto um = getUndoManager();
        gainValue.referTo (state, tracktion::IDs::gain, um, 1.0f);

        // Initializes gain parameter and attaches to gain value
        gainParam = addParam ("gain", TRANS("Gain"), { 0.1f, 20.0f });
        gainParam->attachToCurrentValue (gainValue);
    }

    ~DistortionPlugin() override
    {
        notifyListenersOfDeletion();
        gainParam->detachFromCurrentValue();
    }

    juce::String getName() override                                     { return getPluginName(); }
    juce::String getPluginType() override                               { return xmlTypeName; }
    bool needsConstantBufferSize() override                             { return false; }
    juce::String getSelectableDescription() override                    { return getName(); }

    void initialise(const PluginInitialisationInfo&) override           {}
    void deinitialise() override                                        {}

    void applyToBuffer (const PluginRenderContext& fc) override
    {
        // Apply a tanh distortion with a gain of 3.0f
        if (! isEnabled())
            return;

        for (int channel = 0; channel < fc.destBuffer->getNumChannels(); ++channel)
        {
            auto dest = fc.destBuffer->getWritePointer (channel);

            for (int i = 0; i < fc.bufferNumSamples; ++i)
                dest[i] = std::tanh (gainValue * dest[i]);
        }
    }

    void restorePluginStateFromValueTree (const juce::ValueTree& v) override
    {
        CachedValue<float>* cvsFloat[] = { &gainValue, nullptr };
        copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);

        for (auto p : getAutomatableParameters())
            p->updateFromAttachedValue();
    }

    juce::CachedValue<float> gainValue;
    AutomatableParameter::Ptr gainParam;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistortionPlugin)
};

const char* DistortionPlugin::xmlTypeName = "Distortion";


//==============================================================================
//==============================================================================
/**
    Wraps a te::AutomatableParameter as a juce::ValueSource so it can be used as
    a Value for example in a Slider.
*/
class ParameterValueSource  : public juce::Value::ValueSource,
                              private AutomatableParameter::Listener
{
public:
    ParameterValueSource (AutomatableParameter::Ptr p)
        : param (p)
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
inline void bindSliderToParameter (juce::Slider& s, AutomatableParameter& p)
{
    const auto v = p.valueRange;
    const auto range = NormalisableRange<double> (static_cast<double> (v.start),
                                                  static_cast<double> (v.end),
                                                  static_cast<double> (v.interval),
                                                  static_cast<double> (v.skew),
                                                  v.symmetricSkew);

    s.setNormalisableRange (range);
    s.getValueObject().referTo (juce::Value (new ParameterValueSource (p)));
}

//==============================================================================
//==============================================================================
class DistortionEffectDemo  : public Component,
                              private ChangeListener
{
public:
    //==============================================================================
    DistortionEffectDemo (te::Engine& e)
        : engine (e)
    {
        // Register our custom plugin with the engine so it can be found using PluginCache::createNewPlugin
        engine.getPluginManager().createBuiltInType<DistortionPlugin>();
        
        Helpers::addAndMakeVisible (*this, { &gainSlider, &playPauseButton });

        oggTempFile = std::make_unique<TemporaryFile> (".ogg");
        auto f = oggTempFile->getFile();
        f.replaceWithData (PlaybackDemoAudio::guitar_loop_ogg, PlaybackDemoAudio::guitar_loop_oggSize);

        // Creates clip. Loads clip from file f.
        // Creates track. Loads clip into track.
        auto track = EngineHelpers::getOrInsertAudioTrackAt (edit, 0);
        jassert (track != nullptr);
        
        // Add a new clip to this track
        te::AudioFile audioFile (edit.engine, f);

        auto clip = track->insertWaveClip (f.getFileNameWithoutExtension(), f,
                                           { { {}, te::TimePosition::fromSeconds (audioFile.getLength()) }, {} }, false);
        jassert (clip != nullptr);

        // Creates new instance of Distortion Plugin and inserts to track 1
        auto gainPlugin = edit.getPluginCache().createNewPlugin (DistortionPlugin::xmlTypeName, {});
        track->pluginList.insertPlugin (gainPlugin, 0, nullptr);

        // Set the loop points to the start/end of the clip, enable looping and start playback
        edit.getTransport().addChangeListener (this);
        EngineHelpers::loopAroundClip (*clip);

        // Setup button callbacks
        playPauseButton.onClick = [this] { EngineHelpers::togglePlay(edit); };

        // Setup slider value source
        auto gainParam = gainPlugin->getAutomatableParameterByID ("gain");
        bindSliderToParameter (gainSlider, *gainParam);

        updatePlayButtonText();

        setSize (600, 400);
    }

    //==============================================================================
    void paint(Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto topR = r.removeFromTop (30);
        playPauseButton.setBounds (topR.reduced (2));

        gainSlider.setBounds (50, 50, 500, 50);
    }

private:
    //==============================================================================
    te::Engine& engine;
    te::Edit edit { Edit::Options { engine, te::createEmptyEdit (engine), ProjectItemID::createNewID (0) } };
    std::unique_ptr<TemporaryFile> oggTempFile;

    TextButton playPauseButton { "Play" };
    Slider gainSlider;

    //==============================================================================
    void updatePlayButtonText()
    {
        playPauseButton.setButtonText (edit.getTransport().isPlaying() ? "Pause" : "Play");
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        updatePlayButtonText();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistortionEffectDemo)
};

//==============================================================================
static DemoTypeBase<DistortionEffectDemo> distortionEffectDemo ("Distortion Plugin");
