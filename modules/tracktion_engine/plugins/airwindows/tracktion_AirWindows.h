/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion { inline namespace engine
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
        dither,
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
    void initialise (const PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const PluginRenderContext&) override;

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

AIRWINDOWSPLUGIN(AirWindowsADClip7, "ADClip 7");
AIRWINDOWSPLUGIN(AirWindowsADT, "ADT");
AIRWINDOWSPLUGIN(AirWindowsAQuickVoiceClip, "A Quick Voice Clip");
AIRWINDOWSPLUGIN(AirWindowsAcceleration, "Acceleration");
AIRWINDOWSPLUGIN(AirWindowsAir, "Air");
AIRWINDOWSPLUGIN(AirWindowsAtmosphereBuss, "Atmosphere Buss");
AIRWINDOWSPLUGIN(AirWindowsAtmosphereChannel, "Atmosphere Channel");
AIRWINDOWSPLUGIN(AirWindowsAura, "Aura");
AIRWINDOWSPLUGIN(AirWindowsAverage, "Average");
AIRWINDOWSPLUGIN(AirWindowsBassDrive, "Bass Drive");
AIRWINDOWSPLUGIN(AirWindowsBassKit, "Bass Kit");
AIRWINDOWSPLUGIN(AirWindowsBiquad, "Biquad");
AIRWINDOWSPLUGIN(AirWindowsBiquad2, "Biquad 2");
AIRWINDOWSPLUGIN(AirWindowsBitGlitter, "Bit Glitter");
AIRWINDOWSPLUGIN(AirWindowsBitShiftGain, "Bit Shift Gain");
AIRWINDOWSPLUGIN(AirWindowsBite, "Bite");
AIRWINDOWSPLUGIN(AirWindowsBlockParty, "Block Party");
AIRWINDOWSPLUGIN(AirWindowsBrassRider, "Brass Rider");
AIRWINDOWSPLUGIN(AirWindowsBuildATPDF, "Build ATPDF");
AIRWINDOWSPLUGIN(AirWindowsBussColors4, "Buss Colors 4");
AIRWINDOWSPLUGIN(AirWindowsButterComp, "Butter Comp");
AIRWINDOWSPLUGIN(AirWindowsButterComp2, "Butter Comp 2");
AIRWINDOWSPLUGIN(AirWindowsC5RawBuss, "C5 Raw Buss");
AIRWINDOWSPLUGIN(AirWindowsC5RawChannel, "C5 Raw Channel");
AIRWINDOWSPLUGIN(AirWindowsCStrip, "C Strip");
AIRWINDOWSPLUGIN(AirWindowsCapacitor, "Capacitor");
AIRWINDOWSPLUGIN(AirWindowsChannel4, "Channel 4");
AIRWINDOWSPLUGIN(AirWindowsChannel5, "Channel 5");
AIRWINDOWSPLUGIN(AirWindowsChannel6, "Channel 6");
AIRWINDOWSPLUGIN(AirWindowsChannel7, "Channel 7");
AIRWINDOWSPLUGIN(AirWindowsChorus, "Chorus");
AIRWINDOWSPLUGIN(AirWindowsChorusEnsemble, "Chorus Ensemble");
AIRWINDOWSPLUGIN(AirWindowsClipOnly, "Clip Only");
AIRWINDOWSPLUGIN(AirWindowsCoils, "Coils");
AIRWINDOWSPLUGIN(AirWindowsCojones, "Cojones");
AIRWINDOWSPLUGIN(AirWindowsCompresaturator, "Compresaturator");
AIRWINDOWSPLUGIN(AirWindowsConsole4Buss, "Console 4 Buss");
AIRWINDOWSPLUGIN(AirWindowsConsole4Channel, "Console 4 Channel");
AIRWINDOWSPLUGIN(AirWindowsConsole5Buss, "Console 5 Buss");
AIRWINDOWSPLUGIN(AirWindowsConsole5Channel, "Console 5 Channel");
AIRWINDOWSPLUGIN(AirWindowsConsole5DarkCh, "Console 5 Dark Ch");
AIRWINDOWSPLUGIN(AirWindowsConsole6Buss, "Console 6 Buss");
AIRWINDOWSPLUGIN(AirWindowsConsole6Channel, "Console 6 Channel");
AIRWINDOWSPLUGIN(AirWindowsCrunchyGrooveWear, "Crunchy Groove Wear");
AIRWINDOWSPLUGIN(AirWindowsCrystal, "Crystal");
AIRWINDOWSPLUGIN(AirWindowsDCVoltage, "DC Voltage");
AIRWINDOWSPLUGIN(AirWindowsDeBess, "De Bess");
AIRWINDOWSPLUGIN(AirWindowsDeEss, "De Ess");
AIRWINDOWSPLUGIN(AirWindowsDeHiss, "De Hiss");
AIRWINDOWSPLUGIN(AirWindowsDeRez, "De Rez");
AIRWINDOWSPLUGIN(AirWindowsDeRez2, "De Rez 2");
AIRWINDOWSPLUGIN(AirWindowsDeckwrecka, "Deckwrecka");
AIRWINDOWSPLUGIN(AirWindowsDensity, "Density");
AIRWINDOWSPLUGIN(AirWindowsDesk, "Desk");
AIRWINDOWSPLUGIN(AirWindowsDesk4, "Desk 4");
AIRWINDOWSPLUGIN(AirWindowsDistance, "Distance");
AIRWINDOWSPLUGIN(AirWindowsDistance2, "Distance 2");
AIRWINDOWSPLUGIN(AirWindowsDitherFloat, "Dither Float");
AIRWINDOWSPLUGIN(AirWindowsDitherMeDiskers, "Dither Me Diskers");
AIRWINDOWSPLUGIN(AirWindowsDitherMeTimbers, "Dither Me Timbers");
AIRWINDOWSPLUGIN(AirWindowsDitherbox, "Ditherbox");
AIRWINDOWSPLUGIN(AirWindowsDoublePaul, "Double Paul");
AIRWINDOWSPLUGIN(AirWindowsDrive, "Drive");
AIRWINDOWSPLUGIN(AirWindowsDrumSlam, "Drum Slam");
AIRWINDOWSPLUGIN(AirWindowsDubCenter, "Dub Center");
AIRWINDOWSPLUGIN(AirWindowsDubSub, "Dub Sub");
AIRWINDOWSPLUGIN(AirWindowsDustBunny, "Dust Bunny");
AIRWINDOWSPLUGIN(AirWindowsDyno, "Dyno");
AIRWINDOWSPLUGIN(AirWindowsEQ, "EQ");
AIRWINDOWSPLUGIN(AirWindowsEdIsDim, "Ed Is Dim");
AIRWINDOWSPLUGIN(AirWindowsElectroHat, "Electro Hat");
AIRWINDOWSPLUGIN(AirWindowsEnergy, "Energy");
AIRWINDOWSPLUGIN(AirWindowsEnsemble, "Ensemble");
AIRWINDOWSPLUGIN(AirWindowsEveryTrim, "Every Trim");
AIRWINDOWSPLUGIN(AirWindowsFacet, "Facet");
AIRWINDOWSPLUGIN(AirWindowsFathomFive, "Fathom Five");
AIRWINDOWSPLUGIN(AirWindowsFloor, "Floor");
AIRWINDOWSPLUGIN(AirWindowsFocus, "Focus");
AIRWINDOWSPLUGIN(AirWindowsFracture, "Fracture");
AIRWINDOWSPLUGIN(AirWindowsFromTape, "From Tape");
AIRWINDOWSPLUGIN(AirWindowsGatelope, "Gatelope");
AIRWINDOWSPLUGIN(AirWindowsGolem, "Golem");
AIRWINDOWSPLUGIN(AirWindowsGringer, "Gringer");
AIRWINDOWSPLUGIN(AirWindowsGrooveWear, "Groove Wear");
AIRWINDOWSPLUGIN(AirWindowsGuitarConditioner, "Guitar Conditioner");
AIRWINDOWSPLUGIN(AirWindowsHardVacuum, "Hard Vacuum");
AIRWINDOWSPLUGIN(AirWindowsHermeTrim, "Herme Trim");
AIRWINDOWSPLUGIN(AirWindowsHermepass, "Hermepass");
AIRWINDOWSPLUGIN(AirWindowsHighGlossDither, "High Gloss Dither");
AIRWINDOWSPLUGIN(AirWindowsHighImpact, "High Impact");
AIRWINDOWSPLUGIN(AirWindowsHighpass, "Highpass");
AIRWINDOWSPLUGIN(AirWindowsHighpass2, "Highpass 2");
AIRWINDOWSPLUGIN(AirWindowsHolt, "Holt");
AIRWINDOWSPLUGIN(AirWindowsHombre, "Hombre");
AIRWINDOWSPLUGIN(AirWindowsInterstage, "Interstage");
AIRWINDOWSPLUGIN(AirWindowsIronOxide5, "Iron Oxide 5");
AIRWINDOWSPLUGIN(AirWindowsIronOxideClassic, "Iron Oxide Classic");
AIRWINDOWSPLUGIN(AirWindowsLeftoMono, "Lefto Mono");
AIRWINDOWSPLUGIN(AirWindowsLogical4, "Logical 4");
AIRWINDOWSPLUGIN(AirWindowsLoud, "Loud");
AIRWINDOWSPLUGIN(AirWindowsLowpass, "Lowpass");
AIRWINDOWSPLUGIN(AirWindowsLowpass2, "Lowpass 2");
AIRWINDOWSPLUGIN(AirWindowsMV, "MV");
AIRWINDOWSPLUGIN(AirWindowsMelt, "Melt");
AIRWINDOWSPLUGIN(AirWindowsMidSide, "Mid Side");
AIRWINDOWSPLUGIN(AirWindowsMoNoam, "Mo Noam");
AIRWINDOWSPLUGIN(AirWindowsMojo, "Mojo");
AIRWINDOWSPLUGIN(AirWindowsMonitoring, "Monitoring");
AIRWINDOWSPLUGIN(AirWindowsNCSeventeen, "NC-17");
AIRWINDOWSPLUGIN(AirWindowsNaturalizeDither, "Naturalize Dither");
AIRWINDOWSPLUGIN(AirWindowsNodeDither, "Node Dither");
AIRWINDOWSPLUGIN(AirWindowsNoise, "Noise");
AIRWINDOWSPLUGIN(AirWindowsNonlinearSpace, "Nonlinear Space");
AIRWINDOWSPLUGIN(AirWindowsNotJustAnotherCD, "Not Just Another CD");
AIRWINDOWSPLUGIN(AirWindowsNotJustAnotherDither, "Not Just Another Dither");
AIRWINDOWSPLUGIN(AirWindowsOneCornerClip, "One Corner Clip");
AIRWINDOWSPLUGIN(AirWindowsPDBuss, "PD Buss");
AIRWINDOWSPLUGIN(AirWindowsPDChannel, "PD Channel");
AIRWINDOWSPLUGIN(AirWindowsPafnuty, "Pafnuty");
AIRWINDOWSPLUGIN(AirWindowsPaulDither, "Paul Dither");
AIRWINDOWSPLUGIN(AirWindowsPeaksOnly, "Peaks Only");
AIRWINDOWSPLUGIN(AirWindowsPhaseNudge, "Phase Nudge");
AIRWINDOWSPLUGIN(AirWindowsPocketVerbs, "Pocket Verbs");
AIRWINDOWSPLUGIN(AirWindowsPodcast, "Podcast");
AIRWINDOWSPLUGIN(AirWindowsPodcastDeluxe, "Podcast Deluxe");
AIRWINDOWSPLUGIN(AirWindowsPoint, "Point");
AIRWINDOWSPLUGIN(AirWindowsPop, "Pop");
AIRWINDOWSPLUGIN(AirWindowsPowerSag, "Power Sag");
AIRWINDOWSPLUGIN(AirWindowsPowerSag2, "Power Sag 2");
AIRWINDOWSPLUGIN(AirWindowsPressure4, "Pressure 4");
AIRWINDOWSPLUGIN(AirWindowsPurestAir, "Purest Air");
AIRWINDOWSPLUGIN(AirWindowsPurestConsoleBuss, "Purest Console Buss");
AIRWINDOWSPLUGIN(AirWindowsPurestConsoleChannel, "Purest Console Channel");
AIRWINDOWSPLUGIN(AirWindowsPurestDrive, "Purest Drive");
AIRWINDOWSPLUGIN(AirWindowsPurestEcho, "Purest Echo");
AIRWINDOWSPLUGIN(AirWindowsPurestGain, "Purest Gain");
AIRWINDOWSPLUGIN(AirWindowsPurestSquish, "Purest Squish");
AIRWINDOWSPLUGIN(AirWindowsPurestWarm, "Purest Warm");
AIRWINDOWSPLUGIN(AirWindowsPyewacket, "Pyewacket");
AIRWINDOWSPLUGIN(AirWindowsRawGlitters, "Raw Glitters");
AIRWINDOWSPLUGIN(AirWindowsRawTimbers, "Raw Timbers");
AIRWINDOWSPLUGIN(AirWindowsRecurve, "Recurve");
AIRWINDOWSPLUGIN(AirWindowsRemap, "Remap");
AIRWINDOWSPLUGIN(AirWindowsResEQ, "ResEQ");
AIRWINDOWSPLUGIN(AirWindowsRighteous4, "Righteous 4");
AIRWINDOWSPLUGIN(AirWindowsRightoMono, "Righto Mono");
AIRWINDOWSPLUGIN(AirWindowsSideDull, "Side Dull");
AIRWINDOWSPLUGIN(AirWindowsSidepass, "Sidepass");
AIRWINDOWSPLUGIN(AirWindowsSingleEndedTriode, "Single Ended Triode");
AIRWINDOWSPLUGIN(AirWindowsSlew, "Slew");
AIRWINDOWSPLUGIN(AirWindowsSlew2, "Slew 2");
AIRWINDOWSPLUGIN(AirWindowsSlewOnly, "Slew Only");
AIRWINDOWSPLUGIN(AirWindowsSmooth, "Smooth");
AIRWINDOWSPLUGIN(AirWindowsSoftGate, "Soft Gate");
AIRWINDOWSPLUGIN(AirWindowsSpatializeDither, "Spatialize Dither");
AIRWINDOWSPLUGIN(AirWindowsSpiral, "Spiral");
AIRWINDOWSPLUGIN(AirWindowsSpiral2, "Spiral 2");
AIRWINDOWSPLUGIN(AirWindowsStarChild, "Star Child");
AIRWINDOWSPLUGIN(AirWindowsStereoFX, "Stereo FX");
AIRWINDOWSPLUGIN(AirWindowsStudioTan, "Studio Tan");
AIRWINDOWSPLUGIN(AirWindowsSubsOnly, "Subs Only");
AIRWINDOWSPLUGIN(AirWindowsSurge, "Surge");
AIRWINDOWSPLUGIN(AirWindowsSurgeTide, "Surge Tide");
AIRWINDOWSPLUGIN(AirWindowsSwell, "Swell");
AIRWINDOWSPLUGIN(AirWindowsTPDFDither, "TPDF Dither");
AIRWINDOWSPLUGIN(AirWindowsTapeDelay, "Tape Delay");
AIRWINDOWSPLUGIN(AirWindowsTapeDither, "Tape Dither");
AIRWINDOWSPLUGIN(AirWindowsTapeDust, "Tape Dust");
AIRWINDOWSPLUGIN(AirWindowsTapeFat, "Tape Fat");
AIRWINDOWSPLUGIN(AirWindowsThunder, "Thunder");
AIRWINDOWSPLUGIN(AirWindowsToTape5, "To Tape 5");
AIRWINDOWSPLUGIN(AirWindowsToVinyl4, "To Vinyl 4");
AIRWINDOWSPLUGIN(AirWindowsToneSlant, "Tone Slant");
AIRWINDOWSPLUGIN(AirWindowsTransDesk, "Trans Desk");
AIRWINDOWSPLUGIN(AirWindowsTremolo, "Tremolo");
AIRWINDOWSPLUGIN(AirWindowsTubeDesk, "Tube Desk");
AIRWINDOWSPLUGIN(AirWindowsUnBox, "Un Box");
AIRWINDOWSPLUGIN(AirWindowsVariMu, "Vari Mu");
AIRWINDOWSPLUGIN(AirWindowsVibrato, "Vibrato");
AIRWINDOWSPLUGIN(AirWindowsVinylDither, "Vinyl Dither");
AIRWINDOWSPLUGIN(AirWindowsVoiceOfTheStarship, "Voice Of The Starship");
AIRWINDOWSPLUGIN(AirWindowsVoiceTrick, "Voice Trick");
AIRWINDOWSPLUGIN(AirWindowsWider, "Wider");
AIRWINDOWSPLUGIN(AirWindowscurve, "curve");
AIRWINDOWSPLUGIN(AirWindowsuLawDecode, "u Law Decode");
AIRWINDOWSPLUGIN(AirWindowsuLawEncode, "u Law Encode");

}} // namespace tracktion { inline namespace engine
