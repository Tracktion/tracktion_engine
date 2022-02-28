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

/**
    Holds the state of a process call.
*/
struct ProcessState
{
    /** Creates a ProcessState. */
    ProcessState (tracktion::graph::PlayHeadState&);

    /** Creates a ProcessState that will update the editBeatRange field. */
    ProcessState (tracktion::graph::PlayHeadState&, const TempoSequence&);

    /** Updates the internal state based on a reference sample range and PlayHeadState. */
    void update (double sampleRate, juce::Range<int64_t> referenceSampleRange);

    tracktion::graph::PlayHeadState& playHeadState;
    std::unique_ptr<tempo::Sequence::Position> tempoPosition;
    double sampleRate = 44100.0;
    int numSamples = 0;
    juce::Range<int64_t> referenceSampleRange, timelineSampleRange;
    TimeRange editTimeRange;
    BeatRange editBeatRange;
};


//==============================================================================
//==============================================================================
/**
    Base class for Nodes that provides information about the current process call.
 */
class TracktionEngineNode
{
public:
    //==============================================================================
    /** Creates a TracktionEngineNode. */
    TracktionEngineNode (ProcessState&);
    
    /** Destructor. */
    virtual ~TracktionEngineNode() = default;
    
    //==============================================================================
    /** Returns the number of samples in the current process block. */
    int getNumSamples() const                               { return processState.numSamples; }

    /** Returns the sample rate of the current process block. */
    double getSampleRate() const                            { return processState.sampleRate; }

    /** Returns the timeline sample range of the current process block. */
    juce::Range<int64_t> getTimelineSampleRange() const     { return processState.timelineSampleRange; }

    /** Returns the edit time range of the current process block. */
    TimeRange getEditTimeRange() const                      { return processState.editTimeRange; }

    /** Returns the edit beat range of the current process block. */
    BeatRange getEditBeatRange() const                      { return processState.editBeatRange; }

    /** Returns the reference sample range (from the DeviceManager) of the current process block. */
    juce::Range<int64_t> getReferenceSampleRange() const    { return processState.referenceSampleRange; }

    //==============================================================================
    /** May return the time of the next tempo or time sig change. */
    std::optional<TimePosition> getTimeOfNextChange() const;

    /** May return the time of the next tempo or time sig change. */
    std::optional<BeatPosition> getBeatOfNextChange() const;

    //==============================================================================
    /** Returns the PlayHeadState in use. */
    tracktion::graph::PlayHeadState& getPlayHeadState()      { return processState.playHeadState; }

    /** Returns the PlayHead in use. */
    tracktion::graph::PlayHead& getPlayHead()                { return getPlayHeadState().playHead; }

protected:
    //==============================================================================
    ProcessState& processState;
};

}} // namespace tracktion { inline namespace engine
