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

class DynamicOffsetNode;

//==============================================================================
//==============================================================================
/**
*/
class SlotControlNode final : public tracktion::graph::Node,
                              public TracktionEngineNode
{
public:
    SlotControlNode (ProcessState& editProcessState,
                     std::shared_ptr<LaunchHandle>,
                     std::optional<BeatDuration> stopDuration,
                     EditItemID slotID,
                     std::unique_ptr<Node> input);

    //==============================================================================
    tracktion::graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;

    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void prefetchBlock (juce::Range<int64_t>) override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    std::shared_ptr<LaunchHandle> launchHandle;
    std::optional<BeatDuration> stopDuration;
    bool wasPlaying = false;

    const EditItemID slotID;
    std::unique_ptr<Node> input;

    PlayHeadState localPlayheadState;
    ProcessState localProcessState;
    std::vector<DynamicallyOffsettableNodeBase*> offsetNodes;
    std::vector<Node*> orderedNodes, leafNodes;
    LoopingMidiNode* midiNode = nullptr;
    std::shared_ptr<std::vector<float>> lastSamples;
    BeatDuration lastOffset;

    void processSplitSection (ProcessContext&, LaunchHandle::SplitStatus);
    void processSection (ProcessContext&, BeatRange editBeatRange, BeatRange clipBeatRange,
                         bool isPlaying, std::optional<BeatPosition> playStartTime);
    void processStop (ProcessContext&);
};

}} // namespace tracktion { inline namespace engine
