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

AIRWINDOWSPLUGIN(AirWindowsAcceleration, "Acceleration");
AIRWINDOWSPLUGIN(AirWindowsADClip7, "ADClip7");
AIRWINDOWSPLUGIN(AirWindowsADT, "ADT");
AIRWINDOWSPLUGIN(AirWindowsAtmosphere, "Atmosphere");
AIRWINDOWSPLUGIN(AirWindowsAura, "Aura");
AIRWINDOWSPLUGIN(AirWindowsBassKit, "BassKit");
AIRWINDOWSPLUGIN(AirWindowsBitGlitter, "BitGlitter");
AIRWINDOWSPLUGIN(AirWindowsButterComp, "ButterComp");
AIRWINDOWSPLUGIN(AirWindowsButterComp2, "ButterComp2");
AIRWINDOWSPLUGIN(AirWindowsChannel4, "Channel4");
AIRWINDOWSPLUGIN(AirWindowsChannel5, "Channel5");
AIRWINDOWSPLUGIN(AirWindowsChorusEnsemble, "ChorusEnsemble");
AIRWINDOWSPLUGIN(AirWindowsCrunchyGrooveWear, "CrunchyGrooveWear");
AIRWINDOWSPLUGIN(AirWindowsDeEss, "DeEss");
AIRWINDOWSPLUGIN(AirWindowsDensity, "Density");
AIRWINDOWSPLUGIN(AirWindowsDeRez, "DeRez");
AIRWINDOWSPLUGIN(AirWindowsDesk, "Desk");
AIRWINDOWSPLUGIN(AirWindowsDesk4, "Desk4");
AIRWINDOWSPLUGIN(AirWindowsDistance2, "Distance2");
AIRWINDOWSPLUGIN(AirWindowsDrive, "Drive");
AIRWINDOWSPLUGIN(AirWindowsDrumSlam, "DrumSlam");
AIRWINDOWSPLUGIN(AirWindowsDubSub, "DubSub");
AIRWINDOWSPLUGIN(AirWindowsEdIsDim, "EdIsDim");
AIRWINDOWSPLUGIN(AirWindowsElectroHat, "ElectroHat");
AIRWINDOWSPLUGIN(AirWindowsEnergy, "Energy");
AIRWINDOWSPLUGIN(AirWindowsEnsemble, "Ensemble");
AIRWINDOWSPLUGIN(AirWindowsFathomFive, "FathomFive");
AIRWINDOWSPLUGIN(AirWindowsFloor, "Floor");
AIRWINDOWSPLUGIN(AirWindowsFromTape, "FromTape");
AIRWINDOWSPLUGIN(AirWindowsGatelope, "Gatelope");
AIRWINDOWSPLUGIN(AirWindowsGolem, "Golem");
AIRWINDOWSPLUGIN(AirWindowsGrooveWear, "GrooveWear");
AIRWINDOWSPLUGIN(AirWindowsGuitarConditioner, "Guitar Conditioner");
AIRWINDOWSPLUGIN(AirWindowsHardVacuum, "Hard Vacuum");
AIRWINDOWSPLUGIN(AirWindowsHombre, "Hombre");
AIRWINDOWSPLUGIN(AirWindowsMelt, "Melt");
AIRWINDOWSPLUGIN(AirWindowsNC17, "NC-17");
AIRWINDOWSPLUGIN(AirWindowsNoise, "Noise");
AIRWINDOWSPLUGIN(AirWindowsNonlinearSpace, "NonlinearSpace");
AIRWINDOWSPLUGIN(AirWindowsPoint, "Point");
AIRWINDOWSPLUGIN(AirWindowsPop, "Pop");
AIRWINDOWSPLUGIN(AirWindowsPressure4, "Pressure4");
AIRWINDOWSPLUGIN(AirWindowsPurestDrive, "PurestDrive");
AIRWINDOWSPLUGIN(AirWindowsPurestWarm, "PurestWarm");
AIRWINDOWSPLUGIN(AirWindowsRighteous4, "Righteous4");
AIRWINDOWSPLUGIN(AirWindowsSingleEndedTriode, "SingleEndedTriode");
AIRWINDOWSPLUGIN(AirWindowsSlewOnly, "SlewOnly");
AIRWINDOWSPLUGIN(AirWindowsSpiral2, "Spiral2");
AIRWINDOWSPLUGIN(AirWindowsStarChild, "StarChild");
AIRWINDOWSPLUGIN(AirWindowsStereoFX, "StereoFX");
AIRWINDOWSPLUGIN(AirWindowsSubsOnly, "SubsOnly");
AIRWINDOWSPLUGIN(AirWindowsSurge, "Surge");
AIRWINDOWSPLUGIN(AirWindowsSwell, "Swell");
AIRWINDOWSPLUGIN(AirWindowsTapeDust, "TapeDust");
AIRWINDOWSPLUGIN(AirWindowsThunder, "Thunder");
AIRWINDOWSPLUGIN(AirWindowsToTape5, "ToTape 5");
AIRWINDOWSPLUGIN(AirWindowsToVinyl4, "ToVinyl4");
AIRWINDOWSPLUGIN(AirWindowsTubeDesk, "TubeDesk");
AIRWINDOWSPLUGIN(AirWindowsUnbox, "Unbox");
AIRWINDOWSPLUGIN(AirWindowsVariMu, "VariMu");
AIRWINDOWSPLUGIN(AirWindowsVoiceOfTheStarship, "Voice Of The Starship");
AIRWINDOWSPLUGIN(AirWindowsWider, "Wider");

    
} // namespace tracktion_engine
