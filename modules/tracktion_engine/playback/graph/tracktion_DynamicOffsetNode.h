/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
/**
*/
class DynamicOffsetNode final : public tracktion::graph::Node,
                                public TracktionEngineNode,
                                public DynamicallyOffsettableNodeBase
{
public:
    DynamicOffsetNode (ProcessState& editProcessState,
                       EditItemID clipID,
                       BeatRange clipPosition,
                       BeatDuration clipOffset,
                       BeatRange clipLoopRange,
                       std::vector<std::unique_ptr<Node>> inputs);

    //==============================================================================
    /** Sets an offset to be applied to all times in this node, effectively shifting
        it forwards or backwards in time.
    */
    void setDynamicOffsetBeats (BeatDuration) override;

    //==============================================================================
    tracktion::graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    std::vector<Node*> getInternalNodes() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void prefetchBlock (juce::Range<int64_t> /*referenceSampleRange*/) override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    const EditItemID containerClipID;
    const BeatRange clipPosition, loopRange;
    const BeatDuration clipOffset;
    tempo::Sequence::Position tempoPosition;
    std::shared_ptr<BeatDuration> dynamicOffsetBeats = std::make_shared<BeatDuration>();

    ProcessState localProcessState;

    std::vector<DynamicallyOffsettableNodeBase*> dynamicOffsetNodes;
    std::vector<Node*> orderedNodes, leafNodes;
    std::vector<std::unique_ptr<Node>> inputs;

    BeatDuration getOffset() const;
    void processSection (ProcessContext&, BeatRange sectionRange);
};

}} // namespace tracktion { inline namespace engine
