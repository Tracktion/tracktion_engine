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
    ~TempoSequence();

    Edit& getEdit() const                           { return edit; }

    const juce::ValueTree& getState() const         { return state; }
    void setState (const juce::ValueTree&);
    void createEmptyState();

    void copyFrom (const TempoSequence&);
    void freeResources();

    //==============================================================================
    juce::UndoManager* getUndoManager() const noexcept;

    //==============================================================================
    const juce::Array<TimeSigSetting*>& getTimeSigs() const;
    int getNumTimeSigs() const;
    TimeSigSetting* getTimeSig (int index) const;
    TimeSigSetting& getTimeSigAt (double time) const;
    TimeSigSetting& getTimeSigAtBeat (double beat) const;

    int indexOfTimeSigAt (double time) const;
    int indexOfTimeSig (const TimeSigSetting*) const;

    //==============================================================================
    const juce::Array<TempoSetting*>& getTempos() const;
    int getNumTempos() const;
    TempoSetting* getTempo (int index) const;
    TempoSetting& getTempoAt (double time) const;
    TempoSetting& getTempoAtBeat (double beat) const;
    double getBpmAt (double time) const; // takes ramping into account
    double getBeatsPerSecondAt (double time) const      { return getBpmAt (time) / 60.0; }
    bool isTripletsAtTime (double time) const;

    int indexOfTempoAt (double t) const;
    int indexOfNextTempoAt (double t) const;
    int indexOfTempo (const TempoSetting*) const;

    int countTemposInRegion (EditTimeRange range) const;
    juce::int64 createHashForTemposInRange (EditTimeRange) const;

    /** inserts a tempo break that can be edited later. */
    TempoSetting::Ptr insertTempo (double time);
    TempoSetting::Ptr insertTempo (double beatNum, double bpm, float curve);
    TimeSigSetting::Ptr insertTimeSig (double time);

    void removeTempo (int index, bool remapEdit);
    void removeTemposBetween (EditTimeRange, bool remapEdit);
    void removeTimeSig (int index);
    void removeTimeSigsBetween (EditTimeRange);

    void moveTempoStart (int index, double deltaBeats, bool snapToBeat);
    void moveTimeSigStart (int index, double deltaBeats, bool snapToBeat);

    /** Inserts space in to a sequence, shifting TempoSettings and TimeSigs. */
    void insertSpaceIntoSequence (double time, double amountOfSpaceInSeconds, bool snapToBeat);

    //==============================================================================
    double timeToBeats (double time) const;
    juce::Range<double> timeToBeats (EditTimeRange timeRange) const;

    struct BarsAndBeats
    {
        int bars;
        double beats;

        int getWholeBeats() const;
        double getFractionalBeats() const;
    };

    BarsAndBeats timeToBarsBeats (double time) const;
    double barsBeatsToTime (BarsAndBeats) const;
    double barsBeatsToBeats (BarsAndBeats) const;

    double beatsToTime (double beats) const;
    EditTimeRange beatsToTime (juce::Range<double> beatsRange) const;

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

        double timeToBeats (double time) const;
        double beatsToTime (double beats) const;

        /** The only modifying operation */
        void swapWith (juce::Array<SectionDetails>& newTempos);

        /** Compare to cheaply determine if any changes have been made. */
        juce::uint32 getChangeCount() const;

    private:
        juce::uint32 changeCounter = 0;
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
    void handleAsyncUpdate() override;

    TempoSetting::Ptr insertTempo (double beatNum, double bpm, float curve, juce::UndoManager*);
    TempoSetting::Ptr insertTempo (double time, juce::UndoManager*);
    TimeSigSetting::Ptr insertTimeSig (double time, juce::UndoManager*);

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
    double getTime() const                                      { return time; }
    TempoSequence::BarsAndBeats getBarsBeatsTime() const;

    const TempoSequence::SectionDetails& getCurrentTempo() const;

    double getPPQTime() const noexcept;
    double getPPQTimeOfBarStart() const noexcept;

    //==============================================================================
    void setTime (double time);

    void addBars (int bars);
    void addBeats (double beats);
    void addSeconds (double seconds);
    void setBarsBeats (TempoSequence::BarsAndBeats);

    void setPPQTime (double ppq);

private:
    //==============================================================================
    const TempoSequence& sequence;
    double time = 0.0;
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
        Clip* clip;
        double startBeat, endBeat, contentStartBeat;
    };

    struct AutomationPos
    {
        AutomationCurve& curve;
        juce::Array<double> beats;
    };

    juce::Array<ClipPos> clips;
    juce::Array<AutomationPos> automation;
    juce::Range<double> loopPositionBeats;
};


} // namespace tracktion_engine
