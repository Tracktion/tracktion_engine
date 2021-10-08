/*******************************************************************************
The block below describes the properties of this PIP. A PIP is a short snippet
of code that can be read by the Projucer and used to generate a JUCE project.

BEGIN_JUCE_PIP_METADATA

name:             PitchShiftDemo
version:          0.0.1
vendor:           Tracktion
website:          www.tracktion.com
description:      This example demonstrates the application of a pitch-shift effect to a looping audio sample.

dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_processors, juce_audio_utils,
            juce_core, juce_data_structures, juce_dsp, juce_events, juce_graphics,
            juce_gui_basics, juce_gui_extra, juce_osc, tracktion_engine
exporters:        linux_make, vs2017, xcode_iphone, xcode_mac

moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1, JUCE_PLUGINHOST_AU=1, JUCE_PLUGINHOST_VST3=1, JUCE_MODAL_LOOPS_PERMITTED=1, TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1

type:             Component
mainClass:        PitchShiftDemo

END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include "common/Utilities.h"
#include "common/Components.h"
#include "common/PlaybackDemoAudio.h"

using namespace tracktion_engine;

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
void bindSliderToParameter (juce::Slider& s, AutomatableParameter& p)
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
class PitchShiftDemo  : public Component,
                        private ChangeListener
{
public:
    //==============================================================================
    PitchShiftDemo()
    {
        // Register our custom plugin with the engine so it can be found using PluginCache::createNewPlugin
        engine.getPluginManager().createBuiltInType<PitchShiftPlugin>();
        
        Helpers::addAndMakeVisible (*this, { &pitchShiftSlider, &settingsButton, &playPauseButton });

        oggTempFile = std::make_unique<TemporaryFile> (".ogg");
        auto f = oggTempFile->getFile();
        f.replaceWithData (PlaybackDemoAudio::BITs_Export_2_ogg, PlaybackDemoAudio::BITs_Export_2_oggSize);

        // Creates clip. Loads clip from file f.
        // Creates track. Loads clip into track.
        auto track = EngineHelpers::getOrInsertAudioTrackAt (edit, 0);
        jassert (track != nullptr);
        
        // Add a new clip to this track
        te::AudioFile audioFile (edit.engine, f);

        auto clip = track->insertWaveClip (f.getFileNameWithoutExtension(), f,
                                           { { 0.0, audioFile.getLength() }, 0.0 }, false);
        jassert (clip != nullptr);

        // Creates new instance of Distortion Plugin and inserts to track 1
        auto pitchShiftPlugin = edit.getPluginCache().createNewPlugin (PitchShiftPlugin::xmlTypeName, {});
        track->pluginList.insertPlugin (pitchShiftPlugin, 0, nullptr);

        // Set the loop points to the start/end of the clip, enable looping and start playback
        edit.getTransport().addChangeListener (this);
        EngineHelpers::loopAroundClip (*clip);

        // Setup button callbacks
        playPauseButton.onClick = [this] { EngineHelpers::togglePlay (edit); };
        settingsButton.onClick = [this] { EngineHelpers::showAudioDeviceSettings (engine); };

        // Setup slider value source
        auto pitchShiftParam = pitchShiftPlugin->getAutomatableParameterByID ("semitones up");
        bindSliderToParameter (pitchShiftSlider, *pitchShiftParam);
        pitchShiftSlider.setSkewFactorFromMidPoint (0.0);

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
        pluginsButton.setBounds (topR.removeFromLeft (topR.getWidth() / 3).reduced (2));
        settingsButton.setBounds (topR.removeFromLeft (topR.getWidth() / 2).reduced (2));
        playPauseButton.setBounds (topR.reduced (2));

        pitchShiftSlider.setBounds (50, 50, 500, 50);
    }

private:
    //==============================================================================
    te::Engine engine { ProjectInfo::projectName };
    te::Edit edit { Edit::Options { engine, te::createEmptyEdit (engine), ProjectItemID::createNewID (0) } };
    std::unique_ptr<TemporaryFile> oggTempFile;

    TextButton pluginsButton { "Plugins" }, settingsButton { "Settings" }, playPauseButton { "Play" };
    Slider pitchShiftSlider;

    //==============================================================================
    void updatePlayButtonText()
    {
        playPauseButton.setButtonText (edit.getTransport().isPlaying() ? "Pause" : "Play");
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        updatePlayButtonText();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchShiftDemo)
};

