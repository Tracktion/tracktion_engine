/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
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
                     std::function<void (MonotonicBeat)> stopFunction,
                     EditItemID slotID,
                     std::unique_ptr<Node> input);

    const LaunchHandle& getLaunchHandle() const;
    const LaunchHandle* getLaunchHandleIfNotUnique() const;

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
    std::function<void (MonotonicBeat)> stopFunction;
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
