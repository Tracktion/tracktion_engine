/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

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
    std::vector<Node*> getInternalNodes() override;

    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void prefetchBlock (juce::Range<int64_t>) override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    std::shared_ptr<LaunchHandle> launchHandle;
    std::optional<BeatDuration> stopDuration;
    std::function<void (MonotonicBeat)> stopFunction;

    // Shared with the node this replaces when the graph is rebuilt, along with
    // the inner nodes' state (e.g. the MIDI node's active notes), so a playing
    // clip carries on rather than restarting or losing its note-offs
    struct PlaybackState
    {
        bool wasPlaying = false;
        BeatDuration lastOffset;
        BeatPosition nextClipStart;     // The clip position the next section continues from
    };

    std::shared_ptr<PlaybackState> playbackState = std::make_shared<PlaybackState>();
    bool offsetNodesNeedOffset = true;

    const EditItemID slotID;
    std::unique_ptr<Node> input;

    PlayHeadState localPlayheadState;
    ProcessState localProcessState;
    std::vector<DynamicallyOffsettableNodeBase*> offsetNodes;
    std::vector<Node*> orderedNodes, leafNodes;
    LoopingMidiNode* midiNode = nullptr;
    std::shared_ptr<std::vector<float>> lastSamples;

    void processSplitSection (ProcessContext&, LaunchHandle::SplitStatus);
    void processSection (ProcessContext&, BeatRange editBeatRange, TimeRange editTimeRange, BeatRange clipBeatRange,
                         bool isPlaying, std::optional<BeatPosition> playStartTime);
    void processStop (ProcessContext&, double timestampForMidiNoteOffs);
};

} // namespace tracktion::inline engine
