/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
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
    /** Constructs a TempoSetting belonging to a given TempoSequence.
        Don't construct these directly, use the appropriate insert methods of
        TempoSequence.
    */
    TempoSetting (TempoSequence&, const juce::ValueTree&);

    /** Destructor. */
    ~TempoSetting() override;

    using Ptr = juce::ReferenceCountedObjectPtr<TempoSetting>;

    /** Returns the Edit this setting belongs to. */
    Edit& getEdit() const;

    /** Creates a tree to prepresent a TempoSetting. */
    static juce::ValueTree create (BeatPosition startBeat, double bpm, float curve);

    /** Minimum BPM a setting can have. */
    static constexpr double minBPM = 20.0;

    /** Maximum BPM a setting can have. */
    static constexpr double maxBPM = 300.0;

    //==============================================================================
    /** Returns the description of this Selectable. */
    juce::String getSelectableDescription() override;

    //==============================================================================
    /** Returns the start beat of the setting. */
    BeatPosition getStartBeat() const     { return startBeatNumber; }

    /** Returns the BPM of the setting. */
    double getBpm() const           { return bpm; }

    /** Returns the curve of the setting. */
    float getCurve() const          { return curve; }

    /** Returns the start time in seconds of the tempo setting. */
    TimePosition getStartTime() const;

    /** Sets the properties of this tempo setting.
        @param startBeatNum         The new start beat number to set.
        @param newBpm               The new bpm value to set.
        @param newCurve             The new curve to set.
        @param remapEditPositions   If true, this will adjust any Edit items start/end
                                    positions so they fall on the same beat as they
                                    currently do.
    */
    void set (BeatPosition, double newBpm, float newCurve, bool remapEditPositions);

    /** Sets the BPM of this tempo setting. */
    void setBpm (double newBpm);

    /** Sets the curve of this tempo setting.
        <0.0 is a log curve
         0.0 is a linear curve
        >0.0 is an exponential curve
    */
    void setCurve (float curve);

    void setStartBeat (BeatPosition);

    /** Removes the TempoSetting from the sequence. */
    void removeFromEdit();

    //==============================================================================
    /** Returns the approximate length of one beat based on the bpm and matching
        time sig denonimator.
    */
    TimeDuration getApproxBeatLength() const; // doesn't take into account ramping, etc

    /** Returns the previous tempo setting in the sequence. */
    TempoSetting* getPreviousTempo() const;

    /** Returns the time signature at this tempo's time in the sequence. */
    TimeSigSetting& getMatchingTimeSig() const;

    HashCode getHash() const noexcept;

    TempoSequence& ownerSequence;
    juce::ValueTree state;

    juce::CachedValue<BeatPosition> startBeatNumber;
    juce::CachedValue<double> bpm;
    juce::CachedValue<float> curve;

    /** @internal */
    TimePosition startTime; // (updated by TempoSequence)
};

}} // namespace tracktion { inline namespace engine
