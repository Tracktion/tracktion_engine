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

/** An Node that mixes a sequence of clips of other nodes.

    This node takes a set of input Nodes with associated start + end times,
    and mixes together their output.

    It initialises and releases its inputs as required according to its current
    play position.
*/
class CombiningNode final : public tracktion::graph::Node,
                            public TracktionEngineNode
{
public:
    CombiningNode (EditItemID, ProcessState&);
    ~CombiningNode() override;

    //==============================================================================
    /** Adds an input node to be played at a given time range.

        The offset is relative to the combining node's zero-time, so the input node's
        time of 0 is equal to its (start + offset) relative to the combiner node's start.

        Any nodes passed-in will be deleted by this node when required.
    */
    void addInput (std::unique_ptr<Node>, TimeRange);

    /** Returns the number of inputs added. */
    int getNumInputs() const;

    /** Returns the inputs that have been added.
        N.B. This is a bit of a temporary hack to ensure WaveNodes can access previous
        Nodes that have been added via a CombinngNode. This will be cleaned up in the future.
    */
    std::vector<Node*> getInternalNodes() override;

    //==============================================================================
    std::vector<Node*> getDirectInputNodes() override;
    tracktion::graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void prefetchBlock (juce::Range<int64_t> /*referenceSampleRange*/) override;
    void process (ProcessContext&) override;

    size_t getAllocatedBytes() const override;

private:
    const EditItemID itemID;
    
    struct TimedNode;
    juce::OwnedArray<TimedNode> inputs;
    juce::OwnedArray<juce::Array<TimedNode*>> groups;
    std::atomic<bool> isReadyToProcessBlock { false };
    choc::buffer::ChannelArrayBuffer<float> tempAudioBuffer;
    MidiMessageArray noteOffEventsToSend;

    tracktion::graph::NodeProperties nodeProperties;

    void prefetchGroup (juce::Range<int64_t>, TimeRange);
    void queueNoteOffsForClipsNoLongerPresent (const CombiningNode& oldNode);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CombiningNode)
};

}} // namespace tracktion { inline namespace engine
