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
    enum Type
    {
        delay,
        distortion,
        dynamics,
        emulation,
        eq,
        filter,
        imaging,
        modulation,
        reverb,
        utility,
    };

    //==============================================================================
    AirWindowsPlugin (PluginCreationInfo, std::unique_ptr<AirWindowsBase>);
    ~AirWindowsPlugin();

    virtual Type getPluginCategory() = 0;

    //==============================================================================
    juce::String getSelectableDescription() override                        { return TRANS("Air Windows Plugin"); }
    int getNumOutputChannelsGivenInputs (int numInputChannels) override;

    //==============================================================================
    void initialise (const PlaybackInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const AudioRenderContext&) override;

    void resetToDefault();

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

    void setConversionRange (int param, juce::NormalisableRange<float> range);
    void processBlock (juce::AudioBuffer<float>& buffer);

    juce::CriticalSection lock;
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
    Type getPluginCategory() override                                       { return pluginType; } \
    \
    static const char* getPluginName()                                      { return NEEDS_TRANS(pluginName); } \
    static const char* xmlTypeName; \
    \
    static Type pluginType; \
}; \

AIRWINDOWSPLUGIN(AirWindowsAcceleration, "Acceleration");
AIRWINDOWSPLUGIN(AirWindowsADClip7, "AD Clip 7");
AIRWINDOWSPLUGIN(AirWindowsADT, "ADT");
AIRWINDOWSPLUGIN(AirWindowsAtmosphereBuss, "Atmosphere Buss");
AIRWINDOWSPLUGIN(AirWindowsAtmosphereChannel, "Atmosphere Channel");
AIRWINDOWSPLUGIN(AirWindowsAura, "Aura");
AIRWINDOWSPLUGIN(AirWindowsBassKit, "BassKit");
AIRWINDOWSPLUGIN(AirWindowsBitGlitter, "Bit Glitter");
AIRWINDOWSPLUGIN(AirWindowsButterComp, "Butter Comp");
AIRWINDOWSPLUGIN(AirWindowsButterComp2, "Butter Comp 2");
AIRWINDOWSPLUGIN(AirWindowsChannel4, "Channel 4");
AIRWINDOWSPLUGIN(AirWindowsChannel5, "Channel 5");
AIRWINDOWSPLUGIN(AirWindowsChorusEnsemble, "Chorus Ensemble");
AIRWINDOWSPLUGIN(AirWindowsCrunchyGrooveWear, "Crunchy Groove Wear");
AIRWINDOWSPLUGIN(AirWindowsDeEss, "DeEss");
AIRWINDOWSPLUGIN(AirWindowsDensity, "Density");
AIRWINDOWSPLUGIN(AirWindowsDeRez, "DeRez");
AIRWINDOWSPLUGIN(AirWindowsDesk, "Desk");
AIRWINDOWSPLUGIN(AirWindowsDesk4, "Desk 4");
AIRWINDOWSPLUGIN(AirWindowsDistance2, "Distance 2");
AIRWINDOWSPLUGIN(AirWindowsDrive, "Drive");
AIRWINDOWSPLUGIN(AirWindowsDrumSlam, "Drum Slam");
AIRWINDOWSPLUGIN(AirWindowsDubSub, "Dub Sub");
AIRWINDOWSPLUGIN(AirWindowsEdIsDim, "Ed Is Dim");
AIRWINDOWSPLUGIN(AirWindowsElectroHat, "Electro Hat");
AIRWINDOWSPLUGIN(AirWindowsEnergy, "Energy");
AIRWINDOWSPLUGIN(AirWindowsEnsemble, "Ensemble");
AIRWINDOWSPLUGIN(AirWindowsFathomFive, "Fathom Five");
AIRWINDOWSPLUGIN(AirWindowsFloor, "Floor");
AIRWINDOWSPLUGIN(AirWindowsFromTape, "From Tape");
AIRWINDOWSPLUGIN(AirWindowsGatelope, "Gatelope");
AIRWINDOWSPLUGIN(AirWindowsGolem, "Golem");
AIRWINDOWSPLUGIN(AirWindowsGrooveWear, "Groove Wear");
AIRWINDOWSPLUGIN(AirWindowsGuitarConditioner, "Guitar Conditioner");
AIRWINDOWSPLUGIN(AirWindowsHardVacuum, "Hard Vacuum");
AIRWINDOWSPLUGIN(AirWindowsHombre, "Hombre");
AIRWINDOWSPLUGIN(AirWindowsMelt, "Melt");
AIRWINDOWSPLUGIN(AirWindowsMidSide, "Mid Side");
AIRWINDOWSPLUGIN(AirWindowsNC17, "NC-17");
AIRWINDOWSPLUGIN(AirWindowsNoise, "Noise");
AIRWINDOWSPLUGIN(AirWindowsNonlinearSpace, "Nonlinear Space");
AIRWINDOWSPLUGIN(AirWindowsPoint, "Point");
AIRWINDOWSPLUGIN(AirWindowsPop, "Pop");
AIRWINDOWSPLUGIN(AirWindowsPressure4, "Pressure 4");
AIRWINDOWSPLUGIN(AirWindowsPurestDrive, "Purest Drive");
AIRWINDOWSPLUGIN(AirWindowsPurestWarm, "Purest Warm");
AIRWINDOWSPLUGIN(AirWindowsRighteous4, "Righteous 4");
AIRWINDOWSPLUGIN(AirWindowsSingleEndedTriode, "Single Ended Triode");
AIRWINDOWSPLUGIN(AirWindowsSlewOnly, "Slew Only");
AIRWINDOWSPLUGIN(AirWindowsSpiral2, "Spiral 2");
AIRWINDOWSPLUGIN(AirWindowsStarChild, "Star Child");
AIRWINDOWSPLUGIN(AirWindowsStereoFX, "Stereo FX");
AIRWINDOWSPLUGIN(AirWindowsSubsOnly, "Subs Only");
AIRWINDOWSPLUGIN(AirWindowsSurge, "Surge");
AIRWINDOWSPLUGIN(AirWindowsSwell, "Swell");
AIRWINDOWSPLUGIN(AirWindowsTapeDust, "Tape Dust");
AIRWINDOWSPLUGIN(AirWindowsThunder, "Thunder");
AIRWINDOWSPLUGIN(AirWindowsToTape5, "ToTape 5");
AIRWINDOWSPLUGIN(AirWindowsToVinyl4, "To Vinyl 4");
AIRWINDOWSPLUGIN(AirWindowsTubeDesk, "Tube Desk");
AIRWINDOWSPLUGIN(AirWindowsUnbox, "Unbox");
AIRWINDOWSPLUGIN(AirWindowsVariMu, "Vari Mu");
AIRWINDOWSPLUGIN(AirWindowsVoiceOfTheStarship, "Voice Of The Starship");
AIRWINDOWSPLUGIN(AirWindowsWider, "Wider");


} // namespace tracktion_engine
