/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

//==============================================================================
/**
    A tempo value, as used in a TempoSequence.

    This specifies the BPM, time sig, etc. at a particular time
*/
class TempoSetting   : public juce::ReferenceCountedObject,
                       public CurveEditorPoint
{
public:
    //==============================================================================
    TempoSetting (TempoSequence&, const juce::ValueTree&);
    ~TempoSetting();

    using Ptr = juce::ReferenceCountedObjectPtr<TempoSetting>;

    void selectionStatusChanged (bool isNowSelected) override;

    Edit& getEdit() const;

    static juce::ValueTree create (double startBeat, double bpm, float curve);

    static constexpr double minBPM = 20.0;
    static constexpr double maxBPM = 300.0;

    //==============================================================================
    juce::String getSelectableDescription() override;

    //==============================================================================
    double getStartBeat() const     { return startBeatNumber; }
    double getBpm() const           { return bpm; }
    float getCurve() const          { return curve; }

    double getStartTime() const     { return startTime; }

    void set (double startBeatNum, double newBpm, float newCurve, bool remapEditPositions);

    void setBpm (double newBpm);
    void setCurve (float curve);
    void setStartBeat (double newStartBeat);

    void removeFromEdit();

    //==============================================================================
    double getApproxBeatLength() const; // doesn't take into account ramping, etc

    TempoSetting* getPreviousTempo() const;
    TimeSigSetting& getMatchingTimeSig() const;

    juce::int64 getHash() const noexcept;

    TempoSequence& ownerSequence;
    juce::ValueTree state;

    juce::CachedValue<double> startBeatNumber;
    juce::CachedValue<double> bpm;
    juce::CachedValue<float> curve;

    double startTime = 0; // (updated by TempoSequence)
};

} // namespace tracktion_engine
