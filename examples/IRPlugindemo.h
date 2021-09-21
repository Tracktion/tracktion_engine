/*******************************************************************************
The block below describes the properties of this PIP. A PIP is a short snippet
of code that can be read by the Projucer and used to generate a JUCE project.

BEGIN_JUCE_PIP_METADATA

name:             IRPluginDemo
version:          0.0.1
vendor:           Tracktion
website:          www.tracktion.com
description:      This example simply loads a project from the command line and plays it back in a loop.

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
    class IRPlugin  : public Plugin
    {
    public:
        static const char* getPluginName() { return NEEDS_TRANS("IRPluginDemo"); }
        static const char* xmlTypeName;

		IRPlugin (PluginCreationInfo info)  : Plugin (info)
        {
   //         auto um = getUndoManager();


            // Load IR file
            IRTempFile = std::make_unique <TemporaryFile>(".wav");
            auto IRFile = IRTempFile->getFile();
            IRFile.replaceWithData(IRs::AC30_brilliant_bx44_neve_close_dc_wav, IRs::AC30_brilliant_bx44_neve_close_dc_wavSize);
            
            // Load IR file into convolver
            loadIRFile (IRFile, IRFile.getSize(), dsp::Convolution::Stereo::yes, 
                                                  dsp::Convolution::Trim::no);
        }

        ~IRPlugin() override
        {
            notifyListenersOfDeletion();
            gainParam->detachFromCurrentValue();
        }

        juce::String getName() override                                     { return getPluginName(); }
        juce::String getPluginType() override                               { return xmlTypeName; }
        bool needsConstantBufferSize() override                             { return false; }
        juce::String getSelectableDescription() override                    { return getName(); }

        void initialise (const PluginInitialisationInfo&) override           
        {
            convolver.reset();
        }

        void deinitialise() override         {}

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            convolver.prepare (spec);
        }

        virtual void loadIRFile (const File& fileImpulseResponse,size_t size_t, dsp::Convolution::Stereo isStereo, dsp::Convolution::Trim requiresTrimming)
        {
            convolver.reset();
            convolver.loadImpulseResponse (File(fileImpulseResponse), isStereo, requiresTrimming, size_t);
        }

        void reset() noexcept
        {
            convolver.reset();
        }
        
        void applyToBuffer (const PluginRenderContext& fc) override
        {
            dsp::AudioBlock<float> inoutBlock (*fc.destBuffer);
            dsp::ProcessContextReplacing<float> context (inoutBlock);
            convolver.process (context);       
        }

        void restorePluginStateFromValueTree (const juce::ValueTree& v) override
        {
            CachedValue<float>* cvsFloat[] = { &gainValue, nullptr };
            copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);

            for (auto p  : getAutomatableParameters())
                p->updateFromAttachedValue();
        }

        juce::CachedValue<float> gainValue;
        AutomatableParameter::Ptr gainParam;




    private:


        juce::dsp::Convolution convolver;

        std::unique_ptr<TemporaryFile> IRTempFile;


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
class IRPluginDemo  : public Component,
                      private ChangeListener
{
public:
    //==============================================================================
    IRPluginDemo()
    {
        // Register our custom plugin with the engine so it can be found using PluginCache::createNewPlugin
        engine.getPluginManager().createBuiltInType <IRPlugin>();
        
        Helpers::addAndMakeVisible (*this, { &gainSlider, &settingsButton, &playPauseButton });

        // Load demo audio file
        oggTempFile = std::make_unique <TemporaryFile> (".ogg");
        auto demoFile = oggTempFile->getFile();      
        demoFile.replaceWithData (PlaybackDemoAudio::BITs_Export_2_ogg, PlaybackDemoAudio::BITs_Export_2_oggSize);

        // TODO: Set up dropdown menu for IR selection

        // Creates clip. Loads clip from file f.
        // Creates track. Loads clip into track.
        auto track = EngineHelpers::getOrInsertAudioTrackAt (edit, 0);
        jassert (track != nullptr);
        
        // Add a new clip to this track
        te::AudioFile audioFile (edit.engine, demoFile);

        auto clip = track->insertWaveClip (demoFile.getFileNameWithoutExtension(), demoFile,
                                           { { 0.0, audioFile.getLength() }, 0.0 }, false);
        jassert (clip != nullptr);

        // Creates new instance of Distortion Plugin and inserts to track 1
        auto plugin = edit.getPluginCache().createNewPlugin (IRPlugin::xmlTypeName, {});
                
        track->pluginList.insertPlugin (plugin, 0, nullptr);

        // Set the loop points to the start/end of the clip, enable looping and start playback
        edit.getTransport().addChangeListener (this);
        EngineHelpers::loopAroundClip (*clip);

        // Setup button callbacks
        playPauseButton.onClick = [this] { EngineHelpers::togglePlay (edit); };
        settingsButton.onClick =  [this] { EngineHelpers::showAudioDeviceSettings (engine); };

        // Setup slider value source
        // TODO: Add sliders and IR loader parameters.

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
        pluginsButton.setBounds (topR.removeFromLeft (topR.getWidth() / 3).reduced (2));
        settingsButton.setBounds (topR.removeFromLeft (topR.getWidth() / 2).reduced (2));
        playPauseButton.setBounds (topR.reduced (2));

        gainSlider.setBounds (50, 50, 500, 50);
    }

private:
    //==============================================================================
    te::Engine engine { ProjectInfo::projectName };
    te::Edit edit { Edit::Options { engine, te::createEmptyEdit (engine), ProjectItemID::createNewID (0) } };
    std::unique_ptr<TemporaryFile> oggTempFile,IRTempFile;

    TextButton pluginsButton { "Plugins" }, settingsButton { "Settings" }, playPauseButton { "Play" };
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IRPluginDemo)
};

