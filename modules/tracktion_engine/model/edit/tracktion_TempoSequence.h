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
//==============================================================================
/** Creates a Position to iterate over the given TempoSequence.
    One of these lets you take a position in a tempo sequence, and skip forwards/backwards
    either by absolute times, or by bar/beat amounts.
    This is dynamic and if the TempoSequence changes, the position will update to reflect this.

    N.B. It is only safe to call this from the message thread or during audio callbacks.
    Access at any other time could incur data races.
*/
tempo::Sequence::Position createPosition (const TempoSequence&);


//==============================================================================
//==============================================================================
/**
    Holds a list of TempoSetting objects, to form a sequence of tempo changes.

    You can query this at particular points, but it's wise to use a
    tempo::Sequence::Position object to iterate it. @see createPosition
*/
class TempoSequence  : public Selectable,
                       private juce::AsyncUpdater
{
public:
    //==============================================================================
    /** Creates a TempoSequence for an Edit. */
    TempoSequence (Edit&);

    /** Destructor */
    ~TempoSequence() override;

    /** Returns the Edit this TempoSequence refers to. */
    Edit& getEdit() const                           { return edit; }

    /** Returns the state this TempoSequence models. */
    const juce::ValueTree& getState() const         { return state; }

    /** Sets the state this TempoSequence should refer to. */
    void setState (const juce::ValueTree&, bool remapEdit);

    /** Resets this to a default, empty state. */
    void createEmptyState();

    /** Copies the contents of another TempoSequence. */
    void copyFrom (const TempoSequence&);

    //==============================================================================
    /** Returns an array of the TimeSigSetting. */
    const juce::Array<TimeSigSetting*>& getTimeSigs() const;

    /** Returns the number of TimeSigSetting[s] in the sequence. */
    int getNumTimeSigs() const;

    /** Returns the TimeSigSetting at a given index. */
    TimeSigSetting* getTimeSig (int index) const;

    /** Returns the TimeSigSetting at a given position. */
    TimeSigSetting& getTimeSigAt (TimePosition) const;

    /** Returns the TimeSigSetting at a given position. */
    TimeSigSetting& getTimeSigAt (BeatPosition) const;

    /** Returns the index of TimeSigSetting at a given position. */
    int indexOfTimeSigAt (TimePosition) const;

    /** Returns the index of a given TimeSigSetting. */
    int indexOfTimeSig (const TimeSigSetting*) const;

    //==============================================================================
    /** Returns the TempoSettings. */
    const juce::Array<TempoSetting*>& getTempos() const;

    /** Returns the current number of TempoSettings. */
    int getNumTempos() const;

    /** Returns the TempoSetting at the given index. */
    TempoSetting* getTempo (int index) const;

    /** Returns the TempoSetting at the given position. */
    TempoSetting& getTempoAt (TimePosition) const;

    /** Returns the TempoSetting at the given position. */
    TempoSetting& getTempoAt (BeatPosition) const;

    /** Returns the tempo at a given position.
        N.B. This is the actual tempo at the time, including any curves.
        I.e. it is not just the bpm of the previous TempoSetting
    */
    double getBpmAt (TimePosition) const; // takes ramping into account

    /** Returns the tempo at a given position.
        N.B. This is the actual tempo at the time, including any curves.
        I.e. it is not just the bpm of the previous TempoSetting
    */
    double getBeatsPerSecondAt (TimePosition, bool lengthOfOneBeatDependsOnTimeSignature = false) const;

    /** Returns true if the TempoSetting is triplets at the given time. */
    bool isTripletsAtTime (TimePosition) const;

    /** Returns the index of the TempoSetting at the given position. */
    int indexOfTempoAt (TimePosition) const;

    /** Returns the index of the TempoSetting after the given position. */
    int indexOfNextTempoAt (TimePosition) const;

    /** Returns the index of the given TempoSetting. */
    int indexOfTempo (const TempoSetting*) const;

    /** Returns the number of TempoSetting[s] in the given range. */
    int countTemposInRegion (TimeRange) const;

    //==============================================================================
    /** Inserts a tempo break that can be edited later. */
    TempoSetting::Ptr insertTempo (TimePosition);

    /** Inserts a tempo with a bpm and curve value. @see TempoSetting. */
    TempoSetting::Ptr insertTempo (BeatPosition, double bpm, float curve);

    /** Inserts a new TimeSigSetting at the given position. */
    TimeSigSetting::Ptr insertTimeSig (TimePosition);

    /** Inserts a new TimeSigSetting at the given position. */
    TimeSigSetting::Ptr insertTimeSig (BeatPosition);

    /** Removes the TempoSetting at a given index.
        @param remapEdit    If true, this will update the positions of Edit content
    */
    void removeTempo (int index, bool remapEdit);

    /** Removes any TempoSetting[s] within the range.
        @param remapEdit    If true, this will update the positions of Edit content
    */
    void removeTemposBetween (TimeRange, bool remapEdit);

    /** Removes the TimeSigSetting at a given index. */
    void removeTimeSig (int index);

    /** Removes any TimeSigSetting[s] within the range. */
    void removeTimeSigsBetween (TimeRange);

    /** Moves the TempoSetting at a given index by a number of beats. */
    void moveTempoStart (int index, BeatDuration deltaBeats, bool snapToBeat);

    /** Moves the TimeSigSetting at a given index by a number of beats. */
    void moveTimeSigStart (int index, BeatDuration deltaBeats, bool snapToBeat);

    /** Inserts space in to a sequence, shifting TempoSettings and TimeSigs. */
    void insertSpaceIntoSequence (TimePosition time, TimeDuration amountOfSpaceInSeconds, bool snapToBeat);

    /** Removes a region in a sequence, shifting TempoSettings and TimeSigs. */
    void deleteRegion (TimeRange);

    //==============================================================================
    /** Converts a time to a number of beats. */
    BeatPosition toBeats (TimePosition) const;

    /** Converts a time range to a beat range. */
    BeatRange toBeats (TimeRange) const;

    /** Converts a number of BarsAndBeats to a position. */
    BeatPosition toBeats (tempo::BarsAndBeats) const;

    /** Converts a number of beats a time. */
    TimePosition toTime (BeatPosition) const;

    /** Converts a beat range to a time range. */
    TimeRange toTime (BeatRange) const;

    /** Converts a number of BarsAndBeats to a position. */
    TimePosition toTime (tempo::BarsAndBeats) const;

    /** Converts a time to a number of BarsAndBeats. */
    tempo::BarsAndBeats toBarsAndBeats (TimePosition) const;

    //==============================================================================
    /** N.B. It is only safe to call this from the message thread or during audio callbacks.
        Access at any other time could incur data races.
    */
    const tempo::Sequence& getInternalSequence() const;

    //==============================================================================
    Edit& edit; /**< The Edit this sequence belongs to. */

    //==============================================================================
    [[ deprecated ("Use new overload set above") ]] tempo::BarsAndBeats timeToBarsBeats (TimePosition) const;
    [[ deprecated ("Use new overload set above") ]] TimePosition barsBeatsToTime (tempo::BarsAndBeats) const;
    [[ deprecated ("Use new overload set above") ]] BeatPosition barsBeatsToBeats (tempo::BarsAndBeats) const;
    [[ deprecated ("Use new overload set above") ]] BeatPosition timeToBeats (TimePosition time) const;
    [[ deprecated ("Use new overload set above") ]] BeatRange timeToBeats (TimeRange range) const;
    [[ deprecated ("Use new overload set above") ]] TimePosition beatsToTime (BeatPosition beats) const;
    [[ deprecated ("Use new overload set above") ]] TimeRange beatsToTime (BeatRange range) const;
    [[ deprecated ("Use new overload set above") ]] TimeSigSetting& getTimeSigAtBeat (BeatPosition) const;
    [[ deprecated ("Use new overload set above") ]] TempoSetting& getTempoAtBeat (BeatPosition) const;

    /** @internal */
    juce::UndoManager* getUndoManager() const noexcept;
    /** @internal */
    juce::String getSelectableDescription() override;
    /** @internal */
    void freeResources();
    /** @internal */
    void updateTempoData();

private:
    //==============================================================================
    friend class TempoSetting;
    friend class TimeSigSetting;

    //==============================================================================
    juce::ValueTree state;

    struct TempoSettingList;
    struct TimeSigList;
    std::unique_ptr<TempoSettingList> tempos;
    std::unique_ptr<TimeSigList> timeSigs;

    tempo::Sequence internalSequence { {{ BeatPosition(), 120.0, 0.0f }},
                                       {{ BeatPosition(), 4, 4, false }},
                                       tempo::LengthOfOneBeat::dependsOnTimeSignature };

    //==============================================================================
    void updateTempoDataIfNeeded() const;
    void handleAsyncUpdate() override;

    TempoSetting::Ptr insertTempo (BeatPosition, double bpm, float curve, juce::UndoManager*);
    TempoSetting::Ptr insertTempo (TimePosition, juce::UndoManager*);
    TimeSigSetting::Ptr insertTimeSig (TimePosition, juce::UndoManager*);
    HashCode createHashForTemposInRange (TimeRange) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TempoSequence)
};


//==============================================================================
/** Takes a copy of all the beat related things in an edit in terms of bars/beats
    and then remaps these after a tempo change.
*/
struct EditTimecodeRemapperSnapshot
{
    void savePreChangeState (Edit&);
    void remapEdit (Edit&);

private:
    struct ClipPos
    {
        Selectable::WeakRef clip;
        BeatPosition startBeat, endBeat;
        BeatDuration contentStartBeat;
    };

    struct AutomationPos
    {
        AutomationCurve& curve;
        juce::Array<BeatPosition> beats;
    };

    juce::Array<ClipPos> clips;
    juce::Array<AutomationPos> automation;
    BeatRange loopPositionBeats;
};


}} // namespace tracktion { inline namespace engine
