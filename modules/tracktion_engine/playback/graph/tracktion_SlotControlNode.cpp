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

SlotControlNode::SlotControlNode (ProcessState& ps,
                                  LauncherClipPlaybackHandle playbackHandle_,
                                  std::shared_ptr<LaunchHandle> launchHandle_,
                                  EditItemID slotID_,
                                  std::unique_ptr<Node> input_)
    : TracktionEngineNode (ps),
      playbackHandle (std::move (playbackHandle_)),
      launchHandle (std::move (launchHandle_)),
      slotID (slotID_),
      input (std::move (input_)),
      localProcessState (ps.playHeadState, *ps.getTempoSequence())
{
    assert (getProcessState().getTempoSequence() != nullptr);

    for (auto n : transformNodes (*input))
    {
        orderedNodes.push_back (n);

        if (n->getDirectInputNodes().empty())
            leafNodes.push_back (n);

        if (auto engineNode = dynamic_cast<TracktionEngineNode*> (n))
            engineNode->setProcessState (localProcessState);

        if (auto dynamicNode = dynamic_cast<DynamicallyOffsettableNodeBase*> (n))
            offsetNodes.push_back (dynamicNode);
    }
}

tracktion::graph::NodeProperties SlotControlNode::getNodeProperties()
{
    auto props = input->getNodeProperties();
    props.nodeID = static_cast<size_t> (slotID.getRawID());

    return props;
}

std::vector<Node*> SlotControlNode::getDirectInputNodes()
{
    return {};
}

void SlotControlNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    auto info2 = info;
    info2.allocateAudioBuffer = {};
    info2.deallocateAudioBuffer = {};

    for (auto& i : orderedNodes)
        i->initialise (info2);
}

bool SlotControlNode::isReadyToProcess()
{
    for (auto& i : leafNodes)
        if (! i->isReadyToProcess())
            return false;

    return true;
}

void SlotControlNode::prefetchBlock (juce::Range<int64_t> referenceSampleRange)
{
    for (auto& node : orderedNodes)
        node->prepareForNextBlock (referenceSampleRange);
}

void SlotControlNode::process (ProcessContext& pc)
{
    const auto editBeatRange = getEditBeatRange();

    if (! launchHandle->hasSynced())
        launchHandle->sync (editBeatRange.getStart());

    if (! editBeatRange.isEmpty())
    {
        const auto splitStatus = launchHandle->advance (editBeatRange.getLength());

        if (! splitStatus.range1.isEmpty())
            processSplitSection (pc, splitStatus);
    }
}

//==============================================================================
void SlotControlNode::processSplitSection (ProcessContext& pc, LaunchHandle::SplitStatus status)
{
    const auto editBeatRange = getEditBeatRange();
    const juce::NormalisableRange blockRangeBeats (editBeatRange.getStart().inBeats(),
                                                   editBeatRange.getEnd().inBeats());

    auto processSubSection = [this, &pc, &blockRangeBeats] (auto section, bool isPlaying, auto playStartTime)
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
        processSection (subSection, isPlaying, playStartTime);
    };

    if (status.isSplit)
    {
        processSubSection (status.range1, status.playing1, status.playStartTime1);
        processSubSection (status.range2, status.playing2, status.playStartTime2);
    }
    else
    {
        processSubSection (status.range1, status.playing1, status.playStartTime1);
    }
}

void SlotControlNode::processSection (ProcessContext& pc, bool isPlaying, std::optional<BeatPosition> playStartTime)
{
    if (! isPlaying)
    {
        pc.buffers.audio.clear();
        return;
    }

    localProcessState.update (getSampleRate(), pc.referenceSampleRange,
                              ProcessState::UpdateContinuityFlags::no);

    // Update the offset for compatible Nodes
    if (playStartTime)
    {
        const auto offset = toDuration (*playStartTime);
        playbackHandle.start (toPosition (offset));

        for (auto n : offsetNodes)
            n->setDynamicOffsetBeats (offset);
    }

    // Process ordered Nodes
    for (auto& node : orderedNodes)
        node->process (pc.numSamples, pc.referenceSampleRange);

    // Copy audio from processed source Node
    auto sourceBuffers = input->getProcessedOutput();
    assert (sourceBuffers.audio.size == pc.buffers.audio.size);
    copyIfNotAliased (pc.buffers.audio, sourceBuffers.audio);
    pc.buffers.midi.copyFrom (sourceBuffers.midi);
}

}} // namespace tracktion { inline namespace engine
