/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

/**
    Holds the state of a process call.
*/
struct ProcessState
{
    /** Creates a ProcessState. */
    ProcessState (tracktion_graph::PlayHeadState&);

    /** Updates the internal state based on a reference sample range and PlayHeadState. */
    void update (double sampleRate, juce::Range<int64_t> referenceSampleRange);
    
    tracktion_graph::PlayHeadState& playHeadState;
    double sampleRate = 44100.0;
    int numSamples = 0;
    juce::Range<int64_t> referenceSampleRange, timelineSampleRange;
    EditTimeRange editTimeRange;
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
    //[[expects: processState]]
    int getNumSamples() const                               { return processState.numSamples; }

    /** Returns the sample rate of the current process block. */
    //[[expects: processState]]
    double getSampleRate() const                            { return processState.sampleRate; }

    /** Returns the timeline sample range of the current process block. */
    //[[expects: processState]]
    juce::Range<int64_t> getTimelineSampleRange() const     { return processState.timelineSampleRange; }

    /** Returns the edit time range (in seconds) of the current process block. */
    //[[expects: processState]]
    EditTimeRange getEditTimeRange() const                  { return processState.editTimeRange; }

    /** Returns the reference sample range (from the DeviceManager) of the current process block. */
    //[[expects: processState]]
    juce::Range<int64_t> getReferenceSampleRange() const    { return processState.referenceSampleRange; }

    //==============================================================================
    /** Returns the PlayHeadState in use. */
    tracktion_graph::PlayHeadState& getPlayHeadState()      { return processState.playHeadState; }

    /** Returns the PlayHead in use. */
    tracktion_graph::PlayHead& getPlayHead()                { return getPlayHeadState().playHead; }

protected:
    //==============================================================================
    ProcessState& processState;
};

} // namespace tracktion_engine
