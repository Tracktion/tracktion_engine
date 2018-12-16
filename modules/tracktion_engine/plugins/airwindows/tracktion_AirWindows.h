/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class AirWindowsBase;
class AirWindowsPlugin;
class AirWindowsAutomatableParameter;
    
//==============================================================================
class AirWindowsCallback
{
public:
    AirWindowsCallback (AirWindowsPlugin&);
    virtual ~AirWindowsCallback() = default;
    virtual double getSampleRate();
    
    AirWindowsPlugin& owner;
};
    
//==============================================================================
class AirWindowsPlugin   : public Plugin
{
public:
    //==============================================================================
    AirWindowsPlugin (PluginCreationInfo, std::unique_ptr<AirWindowsBase>);
    ~AirWindowsPlugin();
    
    //==============================================================================
    juce::String getSelectableDescription() override                        { return TRANS("Air Windows Plugin"); }
    int getNumOutputChannelsGivenInputs (int numInputChannels) override;

    //==============================================================================
    void initialise (const PlaybackInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const AudioRenderContext&) override;

    //==============================================================================
    bool takesAudioInput() override                  { return true; }
    bool takesMidiInput() override                   { return true; }
    bool producesAudioWhenNoAudioInput() override    { return true; }
    bool canBeAddedToClip() override                 { return true; }
    bool canBeAddedToRack() override                 { return true; }
    bool needsConstantBufferSize() override          { return false; }

    //==============================================================================
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;
    
protected:
    //==============================================================================
    friend AirWindowsAutomatableParameter;
    friend AirWindowsCallback;
    
    void processBlock (juce::AudioBuffer<float>& buffer);
    
    AirWindowsCallback callback;
    std::unique_ptr<AirWindowsBase> impl;
    
    double sampleRate = 44100.0;
    
public:
    //==============================================================================
    juce::ReferenceCountedArray<AutomatableParameter> parameters;
    juce::OwnedArray<juce::CachedValue<float>> values;
    
    juce::CachedValue<float> dryValue, wetValue;
    AutomatableParameter::Ptr dryGain, wetGain;
    
private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AirWindowsPlugin)
};

//==============================================================================
#define AIRWINDOWSPLUGIN(className, pluginName) \
class className :   public AirWindowsPlugin \
{ \
public: \
    className (PluginCreationInfo); \
    ~className()                                                            { notifyListenersOfDeletion(); } \
    \
    virtual juce::String getName() override                                 { return TRANS(pluginName); } \
    juce::String getPluginType() override                                   { return xmlTypeName; } \
    \
    static const char* getPluginName()                                      { return NEEDS_TRANS(pluginName); } \
    static const char* xmlTypeName; \
}; \

AIRWINDOWSPLUGIN(AirWindowsDeEss, "DeEss")
AIRWINDOWSPLUGIN(AirWindowsDrive, "Drive")
AIRWINDOWSPLUGIN(AirWindowsHardVacuum, "Hard Vacuum")
AIRWINDOWSPLUGIN(AirWindowsNonlinearSpace, "Nonlinear Space")
AIRWINDOWSPLUGIN(AirWindowsPurestDrive, "Purest Drive")
AIRWINDOWSPLUGIN(AirWindowsTubeDesk, "Tube Desk")

    
} // namespace tracktion_engine
