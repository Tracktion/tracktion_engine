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
                                      BeatRange position,
                                      BeatDuration offset,
                                      BeatRange clipLoopRange,
                                      std::unique_ptr<Node> inputNode)
    : TracktionEngineNode (editProcessState),
      containerClipID (clipID),
      clipPosition (position),
      loopRange (clipLoopRange),
      clipOffset (offset),
      input (std::move (inputNode))
{
    if (auto parentTempoPosition = getProcessState().getTempoSequencePosition())
    {
        tempoPosition = std::make_unique<tempo::Sequence::Position> (*parentTempoPosition);

        // At the moment, this won't work correctly with complex tempo changes and the
        // contained clips will use the tempo conversions of the main Edit. This needs
        // to be added by using the ProcessState directly for tempo conversions with a
        // specified offset
//ddd        jassert (getProcessState().getTempoSequence()->getNumTempos() == 1);
    }

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
    if (input)
        return { input.get() };

    return { playerContext->player.getNode() };
}

void ContainerClipNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    if (info.nodeGraphToReplace != nullptr)
        if (auto oldNode = findNodeWithID<ContainerClipNode> (*info.nodeGraphToReplace, getNodeProperties().nodeID))
            playerContext = oldNode->playerContext;

    if (! playerContext)
    {
        playerContext = std::make_shared<PlayerContext>();
        playerContext->player.setNumThreads (0);

        // We need to create our own Tempo::Position as we'll apply an offset so it stays in sync with the Edit's tempo sequence
        // Make sure we do this before we overwrite the default ProcessState
        if (auto tempoSequence = getProcessState().getTempoSequence())
            playerContext->processState.setTempoSequence (tempoSequence);
    }

    if (! input)
    {
        assert (playerContext->player.getSampleRate() == info.sampleRate);
        return;
    }

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

    playerContext->player.setNode (std::move (input),
                                   info.sampleRate, info.blockSize);
}

bool ContainerClipNode::isReadyToProcess()
{
    return true;
}

void ContainerClipNode::process (ProcessContext& pc)
{
    const auto sectionEditBeatRange = getEditBeatRange();
    const auto sectionEditSampleRange = getTimelineSampleRange();

    if (sectionEditBeatRange.getEnd() <= clipPosition.getStart()
        || sectionEditBeatRange.getStart() >= clipPosition.getEnd())
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

    // Calculate sample positions of clip as these will vary when the tempo changes
    const auto editStartBeatOfLocalTimeline = clipPosition.getStart() - clipOffset;
    const auto editStartTimeOfLocalTimeline = tempoPosition->set (editStartBeatOfLocalTimeline);

    const TimeRange loopTimeRange (tempoPosition->set (loopRange.getStart()),
                                   tempoPosition->set (loopRange.getEnd()));
    const auto loopRangeSamples = toSamples (loopTimeRange, sampleRate);


    // Updated the PlayHead so the position/play setting below is in sync
    localPlayHead.setReferenceSampleRange (pc.referenceSampleRange);

    // We don't want to update the playhead position as we'll do that manually below to avoid triggering playhead jumps
    if (! loopRangeSamples.isEmpty() && localPlayHead.getLoopRange() != loopRangeSamples)
        localPlayHead.setLoopRange (true, loopRangeSamples, false);

    // Syncronise positions
    const auto playheadOffset = toSamples (editStartTimeOfLocalTimeline, sampleRate);
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
        const TimeRange clipTimeRange (tempoPosition->set (clipPosition.getStart()),
                                       tempoPosition->set (clipPosition.getEnd()));
        const auto editPositionInSamples = toSamples ({ clipTimeRange.getStart(), clipTimeRange.getEnd() }, sampleRate);

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
