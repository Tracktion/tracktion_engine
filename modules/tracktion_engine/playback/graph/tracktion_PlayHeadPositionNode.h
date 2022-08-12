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

/**
    A Node that calculates a position to show visually what time is currently being processed by the graph based on its internal latency.
*/
class PlayHeadPositionNode final    : public tracktion::graph::Node,
                                      public TracktionEngineNode
{
public:
    PlayHeadPositionNode (ProcessState& processStateToUse, std::unique_ptr<tracktion::graph::Node> inputNode,
                          std::atomic<double>& playHeadTimeToUpdate)
        : TracktionEngineNode (processStateToUse),
          input (std::move (inputNode)),
          playHeadTime (playHeadTimeToUpdate)
    {
    }
    
    tracktion::graph::NodeProperties getNodeProperties() override
    {
        auto props = input->getNodeProperties();
        
        constexpr size_t playHeadPositionNodeMagicHash = 0x706c617948656164;
        
        if (props.nodeID != 0)
            hash_combine (props.nodeID, playHeadPositionNodeMagicHash);

        return props;
    }
    
    std::vector<tracktion::graph::Node*> getDirectInputNodes() override  { return { input.get() }; }
    bool isReadyToProcess() override                                    { return input->hasProcessed(); }
        
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info) override
    {
        latencyNumSamples = info.nodeGraph.rootNode->getNodeProperties().latencyNumSamples;
        
        // Member variables have to be updated from the previous Node or if the graph gets
        // rebuilt during the countdown period, the playhead time will jump back
        updateFromPreviousNode (info.nodeGraphToReplace);
    }
    
    void process (ProcessContext& pc) override
    {
        // Copy the input buffers to the outputs
        auto sourceBuffers = input->getProcessedOutput();
        jassert (sourceBuffers.audio.getNumChannels() == pc.buffers.audio.getNumChannels());

        pc.buffers.midi.copyFrom (sourceBuffers.midi);
        copy (pc.buffers.audio, sourceBuffers.audio);
        
        updatePlayHeadTime (pc.referenceSampleRange.getLength());
    }

private:
    std::unique_ptr<tracktion::graph::Node> input;
    std::atomic<double>& playHeadTime;

    int latencyNumSamples = 0;
    bool updateReferencePositionOnJump = true;

    struct State
    {
        int64_t numLatencySamplesToCountDown = 0;
        int64_t referencePositionOnJump = 0;
    };
    
    std::shared_ptr<State> state { std::make_shared<State>() };
    
    void updatePlayHeadTime (int64_t numSamples)
    {
        int64_t referenceSamplePosition = getReferenceSampleRange().getStart();
        
        if (getPlayHeadState().didPlayheadJump() || updateReferencePositionOnJump)
        {
            state->numLatencySamplesToCountDown = latencyNumSamples;
            state->referencePositionOnJump = referenceSamplePosition;
        }
        
        if (state->numLatencySamplesToCountDown > 0)
        {
            const int64_t numSamplesToDecrement = std::min (state->numLatencySamplesToCountDown, numSamples);
            state->numLatencySamplesToCountDown -= numSamplesToDecrement;
        }

        if (getPlayHead().isPlaying() && state->numLatencySamplesToCountDown <= 0)
        {
            referenceSamplePosition -= latencyNumSamples;
        }
        else
        {
            referenceSamplePosition = state->referencePositionOnJump;
        }

        const int64_t timelinePosition = getPlayHead().referenceSamplePositionToTimelinePosition (referenceSamplePosition);
        const double time = tracktion::graph::sampleToTime (timelinePosition, getSampleRate());

        playHeadTime = time;
        updateReferencePositionOnJump = false;
    }

    void updateFromPreviousNode (NodeGraph* nodeGraphToReplace)
    {
        if (auto oldNode = findNodeWithIDIfNonZero<PlayHeadPositionNode> (nodeGraphToReplace, getNodeProperties().nodeID))
        {
            state = oldNode->state;
            updateReferencePositionOnJump = false;
        }
    }
};

}} // namespace tracktion { inline namespace engine
