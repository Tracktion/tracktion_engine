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
    void flushPluginStateToValueTree() override;
    
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

AIRWINDOWSPLUGIN(AirWindowsADClip7, "ADClip 7");
AIRWINDOWSPLUGIN(AirWindowsAcceleration, "Acceleration");
AIRWINDOWSPLUGIN(AirWindowsAura, "Aura");
AIRWINDOWSPLUGIN(AirWindowsBitGlitter, "BitGlitter");
AIRWINDOWSPLUGIN(AirWindowsButterComp2, "Butter Comp 2");
AIRWINDOWSPLUGIN(AirWindowsChorusEnsemble, "Chorus Ensemble");
AIRWINDOWSPLUGIN(AirWindowsDeEss, "DeEss")
AIRWINDOWSPLUGIN(AirWindowsDeRez, "DeRez");
AIRWINDOWSPLUGIN(AirWindowsDensity, "Density");
AIRWINDOWSPLUGIN(AirWindowsDistance2, "Distance 2");
AIRWINDOWSPLUGIN(AirWindowsDrive, "Drive")
AIRWINDOWSPLUGIN(AirWindowsFathomFive, "Fathom Five");
AIRWINDOWSPLUGIN(AirWindowsFloor, "Floor");
AIRWINDOWSPLUGIN(AirWindowsGatelope, "Gatelope");
AIRWINDOWSPLUGIN(AirWindowsGrooveWear, "Groove Wear");
AIRWINDOWSPLUGIN(AirWindowsGuitarConditioner, "Guitar Conditioner");
AIRWINDOWSPLUGIN(AirWindowsHardVacuum, "Hard Vacuum")
AIRWINDOWSPLUGIN(AirWindowsHombre, "Hombre");
AIRWINDOWSPLUGIN(AirWindowsNC17, "NC-17");
AIRWINDOWSPLUGIN(AirWindowsNonlinearSpace, "Nonlinear Space")
AIRWINDOWSPLUGIN(AirWindowsPoint, "Point");
AIRWINDOWSPLUGIN(AirWindowsPurestDrive, "Purest Drive")
AIRWINDOWSPLUGIN(AirWindowsPurestWarm, "Purest Warm");
AIRWINDOWSPLUGIN(AirWindowsSingleEndedTriode, "Single Ended Triode");
AIRWINDOWSPLUGIN(AirWindowsStereoFX, "Stereo FX");
AIRWINDOWSPLUGIN(AirWindowsSurge, "Surge");
AIRWINDOWSPLUGIN(AirWindowsToTape5, "To Tape 5");
AIRWINDOWSPLUGIN(AirWindowsToVinyl4, "Vinyl 4");
AIRWINDOWSPLUGIN(AirWindowsTubeDesk, "Tube Desk")
AIRWINDOWSPLUGIN(AirWindowsUnbox, "Unbox");
AIRWINDOWSPLUGIN(AirWindowsWider, "Wider");
    
} // namespace tracktion_engine
