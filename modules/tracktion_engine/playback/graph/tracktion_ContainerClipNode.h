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

//==============================================================================
//==============================================================================
/**
*/
class ContainerClipNode final : public tracktion::graph::Node,
                                public TracktionEngineNode
{
public:
    ContainerClipNode (ProcessState& editProcessState,
                       EditItemID containerClipID,
                       BeatRange clipPosition,
                       BeatDuration clipOffset,
                       BeatRange clipLoopRange,
                       std::unique_ptr<Node>);

    //==============================================================================
    tracktion::graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    std::vector<Node*> getInternalNodes() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    const EditItemID containerClipID;
    const BeatRange clipPosition, loopRange;
    const BeatDuration clipOffset;
    std::unique_ptr<tempo::Sequence::Position> tempoPosition;

    std::unique_ptr<tracktion::graph::Node> input;
    NodeProperties nodeProperties { input->getNodeProperties() };

    struct PlayerContext
    {
        graph::PlayHead playHead;
        graph::PlayHeadState playHeadState { playHead };
        ProcessState processState { playHeadState };
        TracktionNodePlayer player { processState };
    };

    std::shared_ptr<PlayerContext> playerContext;
};

}} // namespace tracktion { inline namespace engine
