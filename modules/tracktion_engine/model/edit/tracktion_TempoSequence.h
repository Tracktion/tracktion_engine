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
    Holds a list of TempoSetting objects, to form a sequence of tempo changes.

    You can query this at particular points, but it's wise to use a
    TempoSequencePosition object to iterate it.
*/
class TempoSequence  : public Selectable,
                       private juce::AsyncUpdater
{
public:
    //==============================================================================
    TempoSequence (Edit&);
    ~TempoSequence() override;

    Edit& getEdit() const                           { return edit; }

    const juce::ValueTree& getState() const         { return state; }
    void setState (const juce::ValueTree&, bool remapEdit);
    void createEmptyState();

    void copyFrom (const TempoSequence&);
    void freeResources();

    //==============================================================================
    juce::UndoManager* getUndoManager() const noexcept;

    //==============================================================================
    const juce::Array<TimeSigSetting*>& getTimeSigs() const;
    int getNumTimeSigs() const;
    TimeSigSetting* getTimeSig (int index) const;
    TimeSigSetting& getTimeSigAt (TimePosition) const;
    TimeSigSetting& getTimeSigAtBeat (BeatPosition) const;

    int indexOfTimeSigAt (TimePosition time) const;
    int indexOfTimeSig (const TimeSigSetting*) const;

    //==============================================================================
    const juce::Array<TempoSetting*>& getTempos() const;
    int getNumTempos() const;
    TempoSetting* getTempo (int index) const;
    TempoSetting& getTempoAt (TimePosition) const;
    TempoSetting& getTempoAtBeat (BeatPosition) const;
    double getBpmAt (TimePosition) const; // takes ramping into account
    double getBeatsPerSecondAt (TimePosition, bool lengthOfOneBeatDependsOnTimeSignature = false) const;
    bool isTripletsAtTime (TimePosition) const;

    int indexOfTempoAt (TimePosition) const;
    int indexOfNextTempoAt (TimePosition) const;
    int indexOfTempo (const TempoSetting*) const;

    int countTemposInRegion (TimeRange) const;
    HashCode createHashForTemposInRange (TimeRange) const;

    /** inserts a tempo break that can be edited later. */
    TempoSetting::Ptr insertTempo (TimePosition);
    TempoSetting::Ptr insertTempo (BeatPosition, double bpm, float curve);
    TimeSigSetting::Ptr insertTimeSig (TimePosition);

    void removeTempo (int index, bool remapEdit);
    void removeTemposBetween (TimeRange, bool remapEdit);
    void removeTimeSig (int index);
    void removeTimeSigsBetween (TimeRange);

    void moveTempoStart (int index, BeatDuration deltaBeats, bool snapToBeat);
    void moveTimeSigStart (int index, BeatDuration deltaBeats, bool snapToBeat);

    /** Inserts space in to a sequence, shifting TempoSettings and TimeSigs. */
    void insertSpaceIntoSequence (TimePosition time, TimeDuration amountOfSpaceInSeconds, bool snapToBeat);

    /** Removes a region in a sequence, shifting TempoSettings and TimeSigs. */
    void deleteRegion (TimeRange);

    //==============================================================================
    BeatPosition timeToBeats (TimePosition) const;
    BeatRange timeToBeats (TimeRange) const;

    struct BarsAndBeats
    {
        int bars;
        double beats;

        int getWholeBeats() const;
        double getFractionalBeats() const;
    };

    BarsAndBeats timeToBarsBeats (TimePosition) const;
    TimePosition barsBeatsToTime (BarsAndBeats) const;
    BeatPosition barsBeatsToBeats (BarsAndBeats) const;

    TimePosition beatsToTime (BeatPosition) const;
    TimeRange beatsToTime (BeatRange) const;

    //==============================================================================
    struct SectionDetails
    {
        double bpm;
        double startTime;
        double startBeatInEdit;
        double secondsPerBeat, beatsPerSecond, ppqAtStart;
        double timeOfFirstBar, beatsUntilFirstBar;
        int barNumberOfFirstBar, numerator, prevNumerator, denominator;
        bool triplets;
    };

    struct TempoSections
    {
        int size() const;
        const SectionDetails& getReference (int i) const;

        BeatPosition timeToBeats (TimePosition) const;
        TimePosition beatsToTime (BeatPosition) const;

        /** The only modifying operation */
        void swapWith (juce::Array<SectionDetails>& newTempos);

        /** Compare to cheaply determine if any changes have been made. */
        uint32_t getChangeCount() const;

    private:
        uint32_t changeCounter = 0;
        juce::Array<SectionDetails> tempos;
    };

    const TempoSections& getTempoSections() { return internalTempos; }

    //==============================================================================
    juce::String getSelectableDescription() override;

    //==============================================================================
    Edit& edit;

    void updateTempoData(); // internal

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

    friend class TempoSequencePosition;
    TempoSections internalTempos;

    //==============================================================================
    void updateTempoDataIfNeeded() const;
    void handleAsyncUpdate() override;

    TempoSetting::Ptr insertTempo (BeatPosition, double bpm, float curve, juce::UndoManager*);
    TempoSetting::Ptr insertTempo (TimePosition, juce::UndoManager*);
    TimeSigSetting::Ptr insertTimeSig (TimePosition, juce::UndoManager*);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TempoSequence)
};

//==============================================================================
/**
    Iterates a TempoSequence.

    One of these lets you take a position in a tempo sequence, and skip forwards/backwards
    either by absolute times, or by bar/beat amounts.
*/
class TempoSequencePosition
{
public:
    //==============================================================================
    TempoSequencePosition (const TempoSequence&);
    TempoSequencePosition (const TempoSequencePosition&);
    ~TempoSequencePosition();

    //==============================================================================
    TimePosition getTime() const                                { return time; }
    TempoSequence::BarsAndBeats getBarsBeatsTime() const;

    const TempoSequence::SectionDetails& getCurrentTempo() const;

    double getPPQTime() const noexcept;
    double getPPQTimeOfBarStart() const noexcept;

    //==============================================================================
    void setTime (TimePosition);

    void addBars (int bars);
    void addBeats (double beats);
    void addSeconds (double seconds);
    void setBarsBeats (TempoSequence::BarsAndBeats);

    void setPPQTime (double ppq);

private:
    //==============================================================================
    const TempoSequence& sequence;
    TimePosition time;
    int index = 0;

    TempoSequencePosition& operator= (const TempoSequencePosition&);
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
