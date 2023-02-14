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
ContainerClipNode::ContainerClipNode (ProcessState& editProcessState,
                                      EditItemID clipID,
                                      ClipPosition pos,
                                      TimeRange loopTimeRange,
                                      std::unique_ptr<Node> inputNode)
    : TracktionEngineNode (editProcessState),
      containerClipID (clipID),
      clipPosition (pos),
      loopRange (loopTimeRange),
      input (std::move (inputNode))
{
    assert (input);
    setOptimisations ({ tracktion::graph::ClearBuffers::no,
                        tracktion::graph::AllocateAudioBuffer::yes });
}

//==============================================================================
tracktion::graph::NodeProperties ContainerClipNode::getNodeProperties()
{
    // Reset the NodeID as we need to be findable between graph loads to keep the same internal NodePlayer
    nodeProperties.nodeID = 0;

    // Calculated from hashing a string view of "ContainerClipNode"
    const auto hashSalt = 9088803362895930667;
    hash_combine (nodeProperties.nodeID, hashSalt);
    hash_combine (nodeProperties.nodeID, containerClipID.getRawID());

    return nodeProperties;
}

std::vector<tracktion::graph::Node*> ContainerClipNode::getDirectInputNodes()
{
    return {};
}

std::vector<Node*> ContainerClipNode::getInternalNodes()
{
    return { input.get() };
}

void ContainerClipNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    editPositionInSamples = tracktion::toSamples ({ clipPosition.getStart(), clipPosition.getEnd() }, info.sampleRate);
    loopRangeSamples = tracktion::toSamples (loopRange, info.sampleRate);

    if (info.nodeGraphToReplace != nullptr)
        if (auto oldNode = findNodeWithID<ContainerClipNode> (*info.nodeGraphToReplace, getNodeProperties().nodeID))
            playerContext = oldNode->playerContext;

    if (! playerContext)
    {
        playerContext = std::make_shared<PlayerContext>();

        // We need to create our own Tempo::Position as we'll apply an offset so it stays in sync with the Edit's tempo sequence
        // Make sure we do this before we overrwite the default ProcessState
        if (auto editTempoPosition = getProcessState().tempoPosition.get())
            playerContext->processState.tempoPosition = std::make_unique<tempo::Sequence::Position> (*editTempoPosition);

        // Set the ProcessState used for all the child nodes so they use the local time, not the Edit time
        visitNodes (*input,
                    [localProcessState = &playerContext->processState] (auto& node)
                    {
            if (auto ten = dynamic_cast<TracktionEngineNode*> (&node))
                ten->setProcessState (*localProcessState);

            for (auto internalNode : node.getInternalNodes())
                if (auto ten = dynamic_cast<TracktionEngineNode*> (internalNode))
                    ten->setProcessState (*localProcessState);
        }, true);

        playerContext->player.setNumThreads (0);
    }

    playerContext->player.setNode (std::move (input),
                                   info.sampleRate, info.blockSize);
}

bool ContainerClipNode::isReadyToProcess()
{
    return true;
}

void ContainerClipNode::process (ProcessContext& pc)
{
    const auto sectionEditTimeRange = getEditTimeRange();
    const auto sectionEditSampleRange = getTimelineSampleRange();

    if (sectionEditTimeRange.getEnd() <= clipPosition.getStart()
        || sectionEditTimeRange.getStart() >= clipPosition.getEnd())
       return;

    // Set playHead loop range using loopRange
    // Find ref offset from clip time
    // If playhead position was overriden, pass this on to the PlayHeadState
    // Process buffer
    // Add an offset to ProcessState so the tempo positions can be synced up

    auto& player = playerContext->player;
    const auto sampleRate = getSampleRate();

    auto& editPlayHead = getPlayHead();
    auto& editPlayHeadState = getPlayHeadState();
    auto& localPlayHead = playerContext->playHead;

    // Updated the PlayHead so the position/play setting below is in sync
    localPlayHead.setReferenceSampleRange (pc.referenceSampleRange);

    if (! loopRangeSamples.isEmpty() && localPlayHead.getLoopRange() != loopRangeSamples)
        localPlayHead.setLoopRange (true, loopRangeSamples);

    // Syncronise positions
    const auto playheadOffset = toSamples (clipPosition.getStartOfSource(), sampleRate);
    playerContext->processState.setPlaybackSpeedRatio (getPlaybackSpeedRatio());

    int64_t newPosition = editPlayHead.getPosition() - playheadOffset + loopRangeSamples.getStart();

    if (localPlayHead.isLooping())
        newPosition = localPlayHead.linearPositionToLoopPosition (newPosition, localPlayHead.getLoopRange());

    if (editPlayHeadState.isContiguousWithPreviousBlock())
        localPlayHead.overridePosition (newPosition);
    else
        localPlayHead.setPosition (newPosition);

    // Syncronise playing state
    if (editPlayHead.isStopped() && ! localPlayHead.isStopped())
        localPlayHead.stop();

    if (editPlayHead.isPlaying() && ! localPlayHead.isPlaying())
        localPlayHead.play();

    assert (! localPlayHead.isLooping() || localPlayHead.getLoopRange().contains (localPlayHead.getPosition()));

    // Process
    ProcessContext localPC { pc.numSamples, pc.referenceSampleRange,
                             { pc.buffers.audio, pc.buffers.midi } };
    player.process (localPC);

    // Silence any samples before or after our edit time range
    {
        const auto destBuffer = pc.buffers.audio;
        auto numSamplesToClearAtStart = std::min (editPositionInSamples.getStart() - sectionEditSampleRange.getStart(), (SampleCount) destBuffer.getNumFrames());
        auto numSamplesToClearAtEnd = std::min (sectionEditSampleRange.getEnd() - editPositionInSamples.getEnd(), (SampleCount) destBuffer.getNumFrames());

        if (numSamplesToClearAtStart > 0)
            destBuffer.getStart ((choc::buffer::FrameCount) numSamplesToClearAtStart).clear();

        if (numSamplesToClearAtEnd > 0)
            destBuffer.getEnd ((choc::buffer::FrameCount) numSamplesToClearAtEnd).clear();
    }
}

}} // namespace tracktion { inline namespace engine
