/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

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

    /** Creates a ProcessState that will update the editBeatRange field. */
    ProcessState (tracktion::graph::PlayHeadState&, const tempo::Sequence&);

    /** An enum to indicate if the PlayHeadState continuity should be updated. */
    enum class UpdateContinuityFlags { no, yes };

    /** Updates the internal state based on a reference sample range.
        @param UpdateContinuityFlags    If yes, the PlayHeadState will be updated, if no it won't be.
                                        If you are calling update more than once in a block, for example
                                        to update the reference sample range for timeline position, updating
                                        the continuity flags more than once will render it useless as it
                                        tracks changes from one call to the next. This flag lets you ensure
                                        it is only called once per block.
    */
    void update (double sampleRate, juce::Range<int64_t> referenceSampleRange, UpdateContinuityFlags);

    /** Sets a playback speed ratio.
        Some Nodes might use this to adjust their playback speeds.
    */
    void setPlaybackSpeedRatio (double newRatio);

    /** Sets the TempoSequence this state utilises. */
    void setTempoSequence (const tempo::Sequence*);

    /** Returns the tempo::Sequence this state has been initialised with one. */
    const tempo::Sequence* getTempoSequence() const;

    /** Returns the tempo::Sequence::Position this state uses. */
    const tempo::Sequence::Position* getTempoSequencePosition() const;

    tracktion::graph::PlayHeadState& playHeadState;
    double sampleRate = 44100.0, playbackSpeedRatio = 1.0;
    int numSamples = 0;
    juce::Range<int64_t> referenceSampleRange, timelineSampleRange;
    TimeRange editTimeRange;
    BeatRange editBeatRange;

private:
    const tempo::Sequence* tempoSequence = nullptr;
    std::unique_ptr<tempo::Sequence::Position> tempoPosition;
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
    int getNumSamples() const                               { return processState->numSamples; }

    /** Returns the sample rate of the current process block. */
    double getSampleRate() const                            { return processState->sampleRate; }

    /** Returns the timeline sample range of the current process block. */
    juce::Range<int64_t> getTimelineSampleRange() const     { return processState->timelineSampleRange; }

    /** Returns the edit time range of the current process block. */
    TimeRange getEditTimeRange() const                      { return processState->editTimeRange; }

    /** Returns the edit beat range of the current process block. */
    BeatRange getEditBeatRange() const                      { return processState->editBeatRange; }

    /** Returns the reference sample range (from the DeviceManager) of the current process block. */
    juce::Range<int64_t> getReferenceSampleRange() const    { return processState->referenceSampleRange; }

    /** Returns the key of the current process block. */
    tempo::Key getKey() const;

    /** Returns the playback speed ratio of the current process block. */
    double getPlaybackSpeedRatio() const;

    //==============================================================================
    /** May return the time of the next tempo or time sig change. */
    std::optional<TimePosition> getTimeOfNextChange() const;

    /** May return the time of the next tempo or time sig change. */
    std::optional<BeatPosition> getBeatOfNextChange() const;

    //==============================================================================
    /** Returns the PlayHeadState in use. */
    tracktion::graph::PlayHeadState& getPlayHeadState()      { return processState->playHeadState; }

    /** Returns the PlayHead in use. */
    tracktion::graph::PlayHead& getPlayHead()                { return getPlayHeadState().playHead; }

    /** Returns the ProcessState in use. */
    ProcessState& getProcessState()                          { return *processState; }

    //==============================================================================
    /** @internal */
    void setProcessState (ProcessState&);

private:
    //==============================================================================
    ProcessState* processState; // Must never be nullptr
};


//==============================================================================
//==============================================================================
class DynamicallyOffsettableNodeBase
{
public:
    DynamicallyOffsettableNodeBase() = default;
    virtual ~DynamicallyOffsettableNodeBase() = default;
    
    /** Sets an offset to be applied to all times in this node, effectively shifting
        it forwards or backwards in time.
     */
    virtual void setDynamicOffsetBeats (BeatDuration) {}

    /** Sets an offset to be applied to all times in this node, effectively shifting
        it forwards or backwards in time.
    */
    virtual void setDynamicOffsetTime (TimeDuration) {}
};


}} // namespace tracktion { inline namespace engine
