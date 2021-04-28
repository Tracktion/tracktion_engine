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

/** An AudioNode that mixes a sequence of clips of other nodes.

    This node takes a set of input AudioNodes with associated start + end times,
    and mixes together their output.

    It initialises and releases its inputs as required according to its current
    play position.
*/
class CombiningNode final : public tracktion_graph::Node,
                            public TracktionEngineNode
{
public:
    CombiningNode (ProcessState&);
    ~CombiningNode() override;

    //==============================================================================
    /** Adds an input node to be played at a given time range.

        The offset is relative to the combining node's zero-time, so the input node's
        time of 0 is equal to its (start + offset) relative to the combiner node's start.

        Any nodes passed-in will be deleted by this node when required.
    */
    void addInput (std::unique_ptr<Node>, EditTimeRange);

    //==============================================================================
    std::vector<Node*> getDirectInputNodes() override;
    tracktion_graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void prefetchBlock (juce::Range<int64_t> /*referenceSampleRange*/) override;
    void process (ProcessContext&) override;

    size_t getAllocatedBytes() const override;

private:
    struct TimedNode;
    juce::OwnedArray<TimedNode> inputs;
    juce::OwnedArray<juce::Array<TimedNode*>> groups;
    std::atomic<bool> isReadyToProcessBlock { false };
    choc::buffer::ChannelArrayBuffer<float> tempAudioBuffer;

    tracktion_graph::NodeProperties nodeProperties;

    void prefetchGroup (juce::Range<int64_t>, double time);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CombiningNode)
};

} // namespace tracktion_engine
