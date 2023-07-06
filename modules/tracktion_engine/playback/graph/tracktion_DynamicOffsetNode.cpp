/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include "tracktion_WaveNode.h"

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
DynamicOffsetNode::DynamicOffsetNode (ProcessState& editProcessState,
                                      EditItemID clipID,
                                      BeatRange position,
                                      BeatDuration offset,
                                      BeatRange clipLoopRange,
                                      std::vector<std::unique_ptr<Node>> inputNodes)
    : TracktionEngineNode (editProcessState),
      containerClipID (clipID),
      clipPosition (position),
      loopRange (clipLoopRange),
      clipOffset (offset),
      tempoPosition (*getProcessState().getTempoSequence()),
      inputs (std::move (inputNodes))
{
    assert (getProcessState().getTempoSequence() != nullptr);
    setOptimisations ({ tracktion::graph::ClearBuffers::yes,
                        tracktion::graph::AllocateAudioBuffer::yes });

    dynamicOffsetNodes.reserve (inputs.size());

    for (auto& i : inputs)
    {
        for (auto n : transformNodes (*i))
        {
            orderedNodes.push_back (n);

            if (n->getDirectInputNodes().empty())
                leafNodes.push_back (n);

            if (auto dynamicNode = dynamic_cast<DynamicallyOffsettableNodeBase*> (n))
                dynamicOffsetNodes.push_back (dynamicNode);
        }
    }
}

//==============================================================================
tracktion::graph::NodeProperties DynamicOffsetNode::getNodeProperties()
{
    NodeProperties props;
    props.hasAudio = false;
    props.hasMidi = false;
    props.numberOfChannels = 0;

    for (auto& node : inputs)
    {
        auto nodeProps = node->getNodeProperties();
        props.hasAudio = props.hasAudio || nodeProps.hasAudio;
        props.hasMidi = props.hasMidi || nodeProps.hasMidi;
        props.numberOfChannels = std::max (props.numberOfChannels, nodeProps.numberOfChannels);
        props.latencyNumSamples = std::max (props.latencyNumSamples, nodeProps.latencyNumSamples);
        hash_combine (props.nodeID, nodeProps.nodeID);
    }

    props.nodeID = 0;

    // Calculated from hashing a string view of "DynamicOffsetNode"
    const auto hashSalt = 8507534508343435306;
    hash_combine (props.nodeID, hashSalt);
    hash_combine (props.nodeID, containerClipID.getRawID());

    return props;
}

std::vector<tracktion::graph::Node*> DynamicOffsetNode::getDirectInputNodes()
{
    return {};
}

std::vector<Node*> DynamicOffsetNode::getInternalNodes()
{
    return orderedNodes;
}

void DynamicOffsetNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    auto info2 = info;
    info2.allocateAudioBuffer = {};
    info2.deallocateAudioBuffer = {};

    for (auto& i : orderedNodes)
        i->initialise (info2);
}

bool DynamicOffsetNode::isReadyToProcess()
{
    // This should really be leaf nodes...
    for (auto& i : leafNodes)
        if (! i->isReadyToProcess())
            return false;

    return true;
}

void DynamicOffsetNode::prefetchBlock (juce::Range<int64_t> referenceSampleRange)
{
    for (auto& node : orderedNodes)
        node->prepareForNextBlock (referenceSampleRange);
}

void DynamicOffsetNode::process (ProcessContext& pc)
{
    const auto sectionEditBeatRange = getEditBeatRange();
    const auto sectionEditSampleRange = getTimelineSampleRange();

    if (sectionEditBeatRange.getEnd() <= clipPosition.getStart()
        || sectionEditBeatRange.getStart() >= clipPosition.getEnd())
       return;

    const auto editStartBeatOfLocalTimeline = clipPosition.getStart() - clipOffset;
    const auto playbackStartBeatRelativeToClip = sectionEditBeatRange.getStart() - editStartBeatOfLocalTimeline;
    const auto loopIteration = loopRange.isEmpty() ? 0
                                                   : static_cast<int> (playbackStartBeatRelativeToClip.inBeats() / loopRange.getLength().inBeats());
    const auto loopIterationOffset = loopRange.getLength() * loopIteration;
    const auto dynamicOffsetBeats =  toDuration (editStartBeatOfLocalTimeline) + loopIterationOffset - toDuration (loopRange.getStart());

    const auto offsetStartTime = tempoPosition.set (editStartBeatOfLocalTimeline);
    const auto offsetEndTime = tempoPosition.add (dynamicOffsetBeats);
    const auto dynamicOffsetTime = offsetEndTime - offsetStartTime;

    // Update the offset for compatible Nodes
    for (auto n : dynamicOffsetNodes)
    {
        n->setDynamicOffsetBeats (dynamicOffsetBeats);
        n->setDynamicOffsetTime (dynamicOffsetTime);
    }

    // Process ordered Nodes
    {
        for (auto& node : orderedNodes)
            node->process (pc.numSamples, pc.referenceSampleRange);
    }

    // Get output from root Nodes
    {
        const auto numChannels = pc.buffers.audio.getNumChannels();
        int nodesWithMidi = pc.buffers.midi.isEmpty() ? 0 : 1;

        // Get each of the inputs and add them to dest
        for (auto& node : inputs)
        {
            auto inputFromNode = node->getProcessedOutput();

            if (auto numChannelsToAdd = std::min (inputFromNode.audio.getNumChannels(), numChannels))
                add (pc.buffers.audio.getFirstChannels (numChannelsToAdd),
                     inputFromNode.audio.getFirstChannels (numChannelsToAdd));
            
            if (inputFromNode.midi.isNotEmpty())
                nodesWithMidi++;
            
            pc.buffers.midi.mergeFrom (inputFromNode.midi);
        }
        
        if (nodesWithMidi > 1)
            pc.buffers.midi.sortByTimestamp();
    }

    // Silence any samples before or after our edit time range
    {
        const TimeRange clipTimeRange (tempoPosition.set (clipPosition.getStart()),
                                       tempoPosition.set (clipPosition.getEnd()));
        const auto editPositionInSamples = toSamples ({ clipTimeRange.getStart(), clipTimeRange.getEnd() }, getSampleRate());

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
