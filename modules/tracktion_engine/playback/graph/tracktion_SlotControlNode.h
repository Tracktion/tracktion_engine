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
                     LauncherClipPlaybackHandle,
                     std::shared_ptr<LaunchHandle>,
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
    LauncherClipPlaybackHandle playbackHandle;
    std::shared_ptr<LaunchHandle> launchHandle;

    const EditItemID slotID;
    std::unique_ptr<Node> input;

    ProcessState localProcessState;
    std::vector<DynamicallyOffsettableNodeBase*> offsetNodes;
    std::vector<Node*> orderedNodes, leafNodes;

    void processSplitSection (ProcessContext&, LaunchHandle::SplitStatus);
    void processSection (ProcessContext&, BeatRange editBeatRange, BeatRange clipBeatRange,
                         bool isPlaying, std::optional<BeatPosition> playStartTime);
};

}} // namespace tracktion { inline namespace engine
