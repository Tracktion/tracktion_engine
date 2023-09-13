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
      localProcessState (editProcessState.playHeadState, *editProcessState.getTempoSequence()),
      inputs (std::move (inputNodes))
{
    assert (getProcessState().getTempoSequence() != nullptr);
    setOptimisations ({ inputs.empty() ? graph::ClearBuffers::yes
                                       : graph::ClearBuffers::no,
                        graph::AllocateAudioBuffer::yes });

    dynamicOffsetNodes.reserve (inputs.size());

    for (auto& i : inputs)
    {
        for (auto n : transformNodes (*i))
        {
            orderedNodes.push_back (n);

            if (n->getDirectInputNodes().empty())
                leafNodes.push_back (n);

            if (auto engineNode = dynamic_cast<TracktionEngineNode*> (n))
                engineNode->setProcessState (localProcessState);

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

    auto section1 = sectionEditBeatRange;
    std::optional<BeatRange> section2;

    if (! loopRange.isEmpty())
    {
        const auto playbackStartBeatRelativeToClip = sectionEditBeatRange.getStart() - editStartBeatOfLocalTimeline;
        const auto loopIteration = loopRange.isEmpty() ? 0
                                                       : static_cast<int> (playbackStartBeatRelativeToClip.inBeats() / loopRange.getLength().inBeats());
        const auto loopEndBeat = editStartBeatOfLocalTimeline + (loopRange.getLength() * (loopIteration + 1));

        if (loopEndBeat > sectionEditBeatRange.getStart()
            && loopEndBeat < sectionEditBeatRange.getEnd())
        {
            section1 = sectionEditBeatRange.withEnd (loopEndBeat);
            section2 = sectionEditBeatRange.withStart (section1.getEnd());

            assert (juce::approximatelyEqual (section1.getLength().inBeats() + section2->getLength().inBeats(),
                                              sectionEditBeatRange.getLength().inBeats()));
            assert (juce::approximatelyEqual (section1.getStart().inBeats(), sectionEditBeatRange.getStart().inBeats()));
            assert (juce::approximatelyEqual (section2->getEnd().inBeats(), sectionEditBeatRange.getEnd().inBeats()));
        }
    }

    // Process the two possible sections
    // N.B. Processing in two sections won't work as the WaveNodeRealTime's TracktionEngineNode base
    //      which references the same ProcessState as everything else will have been updated with the
    //      whole referenceSampleRange for the block. This means you could get up to a block's worth
    //      of audio past the end of the ContainerClip's loop end
    //      There's two potential solutions to this:
    //      1. Use a local ProcessState for the dynamic offset Node and update it
    //         each DynamicOffsetNode::processSection call (This is currently in place and in testing)
    //      2. Convey a point of interest to the main Edit player so it chunks the whole buffer on a
    //         ContainerClip loop boundry
    if (! section2)
    {
        processSection (pc, section1);
    }
    else
    {
        assert (section2->getLength() > 0_bd);
        const juce::NormalisableRange blockRangeBeats (sectionEditBeatRange.getStart().inBeats(),
                                                       sectionEditBeatRange.getEnd().inBeats());

        auto processSubSection = [this, &pc, &blockRangeBeats] (auto section)
        {
            const auto proportion = juce::Range (blockRangeBeats.convertTo0to1 (section.getStart().inBeats()),
                                                 blockRangeBeats.convertTo0to1 (section.getEnd().inBeats()));
            const auto startFrame  = (choc::buffer::FrameCount) std::llround (proportion.getStart() * pc.numSamples);
            const auto endFrame    = (choc::buffer::FrameCount) std::llround (proportion.getEnd() * pc.numSamples);

            const auto sectionNumFrames = endFrame - startFrame;

            if (sectionNumFrames == 0)
                return;

            const auto numRefSamples = pc.referenceSampleRange.getLength();
            const auto startRefSample  = pc.referenceSampleRange.getStart() + (int64_t) std::llround (proportion.getStart() * numRefSamples);
            const auto endRefSample    = pc.referenceSampleRange.getStart() + (int64_t) std::llround (proportion.getEnd() * numRefSamples);

            const juce::Range subSectionReferenceSampleRange (startRefSample, endRefSample);

            for (auto& node : orderedNodes)
                node->prepareForNextBlock (subSectionReferenceSampleRange);

            auto sectionBufferView = pc.buffers.audio.getFrameRange ({ startFrame, endFrame });
            ProcessContext subSection {
                                          sectionNumFrames, subSectionReferenceSampleRange,
                                          { sectionBufferView, pc.buffers.midi }
                                      };
            processSection (subSection, section);
        };

        processSubSection (section1);
        processSubSection (*section2);
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

//==============================================================================
void DynamicOffsetNode::processSection (ProcessContext& pc, BeatRange sectionRange)
{
    const auto editStartBeatOfLocalTimeline = clipPosition.getStart() - clipOffset;

    const auto playbackStartBeatRelativeToClip = sectionRange.getStart() - editStartBeatOfLocalTimeline;
    const auto loopIteration = loopRange.isEmpty() ? 0
                                                   : static_cast<int> (playbackStartBeatRelativeToClip.inBeats() / loopRange.getLength().inBeats());
    const auto loopIterationOffset = loopRange.getLength() * loopIteration;
    const auto dynamicOffsetBeats =  toDuration (editStartBeatOfLocalTimeline) + loopIterationOffset - toDuration (loopRange.getStart());

    const auto offsetStartTime = tempoPosition.set (editStartBeatOfLocalTimeline);
    const auto offsetEndTime = tempoPosition.add (dynamicOffsetBeats);
    const auto dynamicOffsetTime = offsetEndTime - offsetStartTime;

    localProcessState.setPlaybackSpeedRatio (getPlaybackSpeedRatio());
    localProcessState.update (getSampleRate(), pc.referenceSampleRange,
                              ProcessState::UpdateContinuityFlags::no);

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
    }}

}} // namespace tracktion { inline namespace engine
