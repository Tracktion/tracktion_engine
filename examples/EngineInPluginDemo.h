/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

  name:             EngineInPluginDemo
  version:          0.0.1
  vendor:           Tracktion
  website:          www.tracktion.com
  description:      Example of how to use the engine in a plugin

  dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_plugin_client, 
                    juce_audio_processors, juce_audio_utils, juce_core, juce_data_structures, juce_events, 
                    juce_graphics, juce_gui_basics, juce_gui_extra, juce_dsp, juce_osc, tracktion_engine
  exporters:        vs2017, xcode_mac, linux_make

  moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1
  defines:          JucePlugin_IsSynth=1, JucePlugin_WantsMidiInput=1, JucePlugin_ProducesMidiOutput=1
                    JucePlugin_Vst3Category="Instrument", JucePlugin_AUMainType='aumu', JucePlugin_VSTCategory=kPlugCategSynth

  type:             AudioProcessor
  mainClass:        EngineInPluginDemo

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include "common/Utilities.h"


//==============================================================================
//==============================================================================
static String organPatch = "<PLUGIN type=\"4osc\" windowLocked=\"1\" id=\"1069\" enabled=\"1\" filterType=\"1\" presetDirty=\"0\" presetName=\"4OSC: Organ\" filterFreq=\"127.00000000000000000000\" ampAttack=\"0.60000002384185791016\" ampDecay=\"10.00000000000000000000\" ampSustain=\"100.00000000000000000000\" ampRelease=\"0.40000000596046447754\" waveShape1=\"4\" tune2=\"-24.00000000000000000000\" waveShape2=\"4\"> <MACROPARAMETERS id=\"1069\"/> <MODIFIERASSIGNMENTS/> <MODMATRIX/> </PLUGIN>";

//==============================================================================
class EngineInPluginDemo  : public AudioProcessor
{
public:
    //==============================================================================
    EngineInPluginDemo()
        : AudioProcessor (BusesProperties().withInput  ("Input",  AudioChannelSet::stereo())
                                           .withOutput ("Output", AudioChannelSet::stereo()))
    {
    }

    //==============================================================================
    void prepareToPlay (double sampleRate, int expectedBlockSize) override
    {
        // On Linux the plugin and prepareToPlay may not be called on the message thread.
        // Engine needs to be created on the message thread so we'll do that now
        ensureEngineCreatedOnMessageThread();
        
        setLatencySamples (expectedBlockSize);
        ensurePrepareToPlayCalledOnMessageThread (sampleRate, expectedBlockSize);
    }

    void releaseResources() override {}

    using AudioProcessor::processBlock;
    void processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi) override
    {
        // Update position info first
        engineWrapper->playheadSynchroniser.synchronise (*this);

        ScopedNoDenormals noDenormals;
        
        auto totalNumInputChannels  = getTotalNumInputChannels();
        auto totalNumOutputChannels = getTotalNumOutputChannels();

        for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
            buffer.clear (i, 0, buffer.getNumSamples());

        engineWrapper->audioInterface.processBlock (buffer, midi);
    }

    //==============================================================================
    AudioProcessorEditor* createEditor() override          { return new PluginEditor (*this); }
    bool hasEditor() const override                        { return true;   }

    //==============================================================================
    const String getName() const override                  { return "EngineInPluginDemo"; }
    bool acceptsMidi() const override                      { return true; }
    bool producesMidi() const override                     { return true; }
    double getTailLengthSeconds() const override           { return 0; }

    //==============================================================================
    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const String getProgramName (int) override             { return {}; }
    void changeProgramName (int, const String&) override   {}

    //==============================================================================
    void getStateInformation (MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    //==============================================================================
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        if (layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
            return false;

        if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
            return false;

        return true;
    }

private:
    //==============================================================================
    static void setupInputs (te::Edit& edit)
    {
        auto& dm = edit.engine.getDeviceManager();
        
        for (int i = 0; i < dm.getNumMidiInDevices(); i++)
        {
            auto dev = dm.getMidiInDevice (i);
            dev->setEnabled (true);
            dev->setEndToEndEnabled (true);
        }
        
        edit.playInStopEnabled = true;
        edit.getTransport().ensureContextAllocated (true);

        // Add the midi input to track 1
        if (auto t = EngineHelpers::getOrInsertAudioTrackAt (edit, 0))
            if (auto dev = dm.getMidiInDevice (0))
                for (auto instance : edit.getAllInputDevices())
                    if (&instance->getInputDevice() == dev)
                        instance->setTargetTrack (*t, 0, true);

        // Also add the same midi input to track 2
        if (auto t = EngineHelpers::getOrInsertAudioTrackAt (edit, 1))
            if (auto dev = dm.getMidiInDevice (0))
                for (auto instance : edit.getAllInputDevices())
                    if (&instance->getInputDevice() == dev)
                        instance->setTargetTrack (*t, 0, false);


        edit.restartPlayback();
    }

    static void setupOutputs (te::Edit& edit)
    {
        auto& dm = edit.engine.getDeviceManager();

        for (int i = 0; i < dm.getNumMidiOutDevices(); i++)
        {
            auto dev = dm.getMidiOutDevice (i);
            dev->setEnabled (true);
        }

        edit.playInStopEnabled = true;
        edit.getTransport().ensureContextAllocated (true);

        // Set track 2 to send to midi output
        if (auto t = EngineHelpers::getOrInsertAudioTrackAt (edit, 1))
        {
            auto& output = t->getOutput();
            output.setOutputToDefaultDevice (true);
        }

        edit.restartPlayback();
    }

    static void create4OSCPlugin (te::Edit& edit)
    {
        if (auto synth = dynamic_cast<te::FourOscPlugin*> (edit.getPluginCache().createNewPlugin (te::FourOscPlugin::xmlTypeName, {}).get()))
        {
            auto vt = ValueTree::fromXml (organPatch);
            
            if (vt.isValid())
                synth->restorePluginStateFromValueTree (vt);
            
            if (auto t = EngineHelpers::getOrInsertAudioTrackAt (edit, 0))
                t->pluginList.insertPlugin (*synth, 0, nullptr);
        }
    }
    
    //==============================================================================
    class PluginEngineBehaviour : public te::EngineBehaviour
    {
    public:
        bool autoInitialiseDeviceManager() override { return false; }
    };
    
    //==============================================================================
    struct EngineWrapper
    {
        EngineWrapper()
            : audioInterface (engine.getDeviceManager().getHostedAudioDeviceInterface())
        {
            JUCE_ASSERT_MESSAGE_THREAD
            audioInterface.initialise ({});

            setupInputs (edit);
            setupOutputs (edit);
            create4OSCPlugin (edit);
        }

        te::Engine engine { ProjectInfo::projectName, nullptr, std::make_unique<PluginEngineBehaviour>() };
        te::Edit edit { engine, te::createEmptyEdit (engine), te::Edit::forEditing, nullptr, 0 };
        te::TransportControl& transport { edit.getTransport() };
        te::HostedAudioDeviceInterface& audioInterface;
        te::ExternalPlayheadSynchroniser playheadSynchroniser { edit };
    };
    
    template<typename Function>
    void callFunctionOnMessageThread (Function&& func)
    {
        if (MessageManager::getInstance()->isThisTheMessageThread())
        {
            func();
        }
        else
        {
            jassert (! MessageManager::getInstance()->currentThreadHasLockedMessageManager());
            WaitableEvent finishedSignal;
            MessageManager::callAsync ([&]
                                       {
                                           func();
                                           finishedSignal.signal();
                                       });
            finishedSignal.wait (-1);
        }
    }
    
    void ensureEngineCreatedOnMessageThread()
    {
        if (! engineWrapper)
            callFunctionOnMessageThread ([&] { engineWrapper = std::make_unique<EngineWrapper>(); });
    }
    
    void ensurePrepareToPlayCalledOnMessageThread (double sampleRate, int expectedBlockSize)
    {
        jassert (engineWrapper);
        callFunctionOnMessageThread ([&] { engineWrapper->audioInterface.prepareToPlay (sampleRate, expectedBlockSize); });
    }
    
    //==============================================================================
    std::unique_ptr<EngineWrapper> engineWrapper;

    //==============================================================================
    //==============================================================================
    class PluginEditor : public AudioProcessorEditor
    {
    public:
        PluginEditor (EngineInPluginDemo& p)
            : AudioProcessorEditor (p),
              plugin (p)
        {
            addAndMakeVisible (clickTrackButton);

            pluginPositionInfo.resetToDefault();
            editPositionInfo.resetToDefault();
            
            repaintTimer.startTimerHz (25);
            update();

            setSize (400, 300);
        }
        
        void paint (Graphics& g) override
        {
            g.fillAll (Colours::darkgrey);
            
            auto r = getLocalBounds();
            String text;
            text << "Host Info:" << newLine
                 << PlayHeadHelpers::getTimecodeDisplay (pluginPositionInfo) << newLine
                 << newLine
                 << "Tracktion Engine Info:" << newLine
                 << PlayHeadHelpers::getTimecodeDisplay (editPositionInfo) << newLine
                 << "Build:" << newLine
                 << Time::getCompilationDate().toString (true, true);
            g.setColour (Colours::white);
            g.setFont (15.0f);
            g.drawFittedText (text, r.reduced (10), Justification::topLeft, 5);
        }
        
        void resized() override
        {
            auto r = getLocalBounds();
            clickTrackButton.setBounds (r.reduced (10).removeFromBottom (26));
        }
        
    private:
        EngineInPluginDemo& plugin;
        AudioPlayHead::CurrentPositionInfo pluginPositionInfo, editPositionInfo;
        te::LambdaTimer repaintTimer { [this] { update(); } };
        ToggleButton clickTrackButton { "Enable Click Track" };
        
        void update()
        {
            if (plugin.engineWrapper)
            {
                pluginPositionInfo = plugin.engineWrapper->playheadSynchroniser.getPositionInfo();
                editPositionInfo = getCurrentPositionInfo (plugin.engineWrapper->edit);
                clickTrackButton.getToggleStateValue().referTo (plugin.engineWrapper->edit.clickTrackEnabled.getPropertyAsValue());
            }
            
            repaint();
        }
    };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineInPluginDemo)
};

