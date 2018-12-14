/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class AirWindowsPlugin;
class AirWindowsBase;
    
//==============================================================================
class AirWindowCallback
{
public:
    AirWindowCallback (AirWindowsPlugin&);
    virtual ~AirWindowCallback() = default;
    virtual double getSampleRate();
    
    AirWindowsPlugin& owner;
};
    
//==============================================================================
class AirWindowsPlugin   : public Plugin
{
public:
    AirWindowsPlugin (PluginCreationInfo, std::unique_ptr<AirWindowsBase>);
    ~AirWindowsPlugin();
    
    juce::String getSelectableDescription() override                        { return TRANS("Air Windows Plugin"); }
    int getNumOutputChannelsGivenInputs (int numInputChannels) override;

    void initialise (const PlaybackInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const AudioRenderContext&) override;

    bool takesAudioInput() override                  { return true; }
    bool takesMidiInput() override                   { return true; }
    bool producesAudioWhenNoAudioInput() override    { return true; }
    bool canBeAddedToClip() override                 { return true; }
    bool canBeAddedToRack() override                 { return true; }
    bool needsConstantBufferSize() override          { return false; }

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

protected:
    friend AirWindowCallback;
    
    AirWindowCallback callback;
    std::unique_ptr<AirWindowsBase> impl;
    
    double sampleRate = 44100.0;
    
    juce::ReferenceCountedArray<AutomatableParameter> parameters;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AirWindowsPlugin)
};

//==============================================================================
class AirWindowsDeEss :   public AirWindowsPlugin
{
public:
    AirWindowsDeEss (PluginCreationInfo);
    ~AirWindowsDeEss()                                                      { notifyListenersOfDeletion(); }
    
    virtual juce::String getName() override                                 { return TRANS("DeEss"); }
    juce::String getPluginType() override                                   { return xmlTypeName; }
    
    static const char* getPluginName()                                      { return NEEDS_TRANS("AW DeEss"); }
    static const char* xmlTypeName;
};
    
} // namespace tracktion_engine
