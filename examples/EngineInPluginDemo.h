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

  type:             AudioProcessor
  mainClass:        EngineInPluginDemo

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include "common/Utilities.h"

static String organPatch = "<PLUGIN type=\"4osc\" windowLocked=\"1\" id=\"1069\" enabled=\"1\" filterType=\"1\" presetDirty=\"0\" presetName=\"4OSC: Organ\" filterFreq=\"127.00000000000000000000\" ampAttack=\"0.60000002384185791016\" ampDecay=\"10.00000000000000000000\" ampSustain=\"100.00000000000000000000\" ampRelease=\"0.40000000596046447754\" waveShape1=\"4\" tune2=\"-24.00000000000000000000\" waveShape2=\"4\"> <MACROPARAMETERS id=\"1069\"/> <MODIFIERASSIGNMENTS/> <MODMATRIX/> </PLUGIN>";

//==============================================================================
class PluginEditor : public AudioProcessorEditor
{
public:
    PluginEditor (AudioProcessor& p) : AudioProcessorEditor (p)
    {
        setSize (400, 300);
    }
    
    void paint (Graphics& g) override
    {
        g.fillAll (Colours::darkgrey);
    }
};

//==============================================================================
class EngineInPluginDemo  : public AudioProcessor
{
public:
    //==============================================================================
    EngineInPluginDemo()
        : AudioProcessor (BusesProperties().withInput  ("Input",  AudioChannelSet::stereo())
                                           .withOutput ("Output", AudioChannelSet::stereo())),
        audioInterface (engine.getDeviceManager().getHostedAudioDeviceInterface())
    {
        audioInterface.initialise ({});
        
        setupInputs();
        create4OSCPlugin();
    }

    ~EngineInPluginDemo()
    {
    }

    //==============================================================================
    void prepareToPlay (double sampleRate, int expectedBlockSize) override
    {
        setLatencySamples (expectedBlockSize);
        audioInterface.prepareToPlay (sampleRate, expectedBlockSize);
    }

    void releaseResources() override {}

    void processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi) override
    {
        ScopedNoDenormals noDenormals;
        
        auto totalNumInputChannels  = getTotalNumInputChannels();
        auto totalNumOutputChannels = getTotalNumOutputChannels();

        for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
            buffer.clear (i, 0, buffer.getNumSamples());

        audioInterface.processBlock (buffer, midi);
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
    void setupInputs()
    {
        auto& dm = engine.getDeviceManager();
        for (int i = 0; i < dm.getNumMidiInDevices(); i++)
        {
            auto dev = dm.getMidiInDevice (i);
            dev->setEnabled (true);
            dev->setEndToEndEnabled (true);
        }
        
        edit.playInStopEnabled = true;
        edit.getTransport().ensureContextAllocated (true);
        
        if (auto t = EngineHelpers::getOrInsertAudioTrackAt (edit, 0))
            if (auto dev = dm.getMidiInDevice (0))
                for (auto instance : edit.getAllInputDevices())
                    if (&instance->getInputDevice() == dev)
                        instance->setTargetTrack (*t, 0, true);
        
        edit.restartPlayback();
    }
    
    void create4OSCPlugin()
    {
        //==============================================================================
        if (auto synth = dynamic_cast<te::FourOscPlugin*> (edit.getPluginCache().createNewPlugin (te::FourOscPlugin::xmlTypeName, {}).get()))
        {
            XmlDocument doc (organPatch);
            if (auto e = doc.getDocumentElement())
            {
                auto vt = ValueTree::fromXml (*e);
                if (vt.isValid())
                    synth->restorePluginStateFromValueTree (vt);
            }
            
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
    te::Engine engine { ProjectInfo::projectName, nullptr, std::make_unique<PluginEngineBehaviour>() };
    te::Edit edit { engine, te::createEmptyEdit(), te::Edit::forEditing, nullptr, 0 };
    te::TransportControl& transport { edit.getTransport() };
    te::HostedAudioDeviceInterface& audioInterface;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineInPluginDemo)
};

