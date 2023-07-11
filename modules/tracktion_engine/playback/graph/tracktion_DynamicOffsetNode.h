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

class WaveNodeRealTime;

//==============================================================================
//==============================================================================
/**
*/
class DynamicOffsetNode final : public tracktion::graph::Node,
                                public TracktionEngineNode
{
public:
    DynamicOffsetNode (ProcessState& editProcessState,
                       EditItemID clipID,
                       BeatRange clipPosition,
                       BeatDuration clipOffset,
                       BeatRange clipLoopRange,
                       std::vector<std::unique_ptr<Node>> inputs);

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

    ProcessState localProcessState;

    std::vector<DynamicallyOffsettableNodeBase*> dynamicOffsetNodes;
    std::vector<Node*> orderedNodes, leafNodes;
    std::vector<std::unique_ptr<Node>> inputs;

    void processSection (ProcessContext&, BeatRange sectionRange);
};

}} // namespace tracktion { inline namespace engine
