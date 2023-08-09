/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include "../../model/clips/tracktion_LaunchHandle.h"

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
      localPlayheadState (ps.playHeadState.playHead),
      localProcessState (localPlayheadState, *ps.getTempoSequence())
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

        if (auto mn = dynamic_cast<LoopingMidiNode*> (n))
            midiNode = mn;
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

    // Find the lastSamples
    const auto numChans = static_cast<size_t> (getNodeProperties().numberOfChannels);

    if (numChans == 0)
        return;

    if (auto oldGraph = info.nodeGraphToReplace)
        if (auto oldNode = findNodeWithID<SlotControlNode> (*oldGraph, (size_t) slotID.getRawID()))
            if (oldNode->lastSamples && oldNode->lastSamples->size() == numChans)
                lastSamples = oldNode->lastSamples;

    if (lastSamples)
        return;

    lastSamples = std::make_shared<std::vector<float>> (numChans, 0.0f);
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
    // If the playhead has just stopped, stop the launch and let it run to fade the audio/stop MIDI notes etc.
    if (wasPlaying && ! getPlayHead().isPlaying())
    {
        wasPlaying = false;
        launchHandle->stop ({});
        processStop (pc);
        return;
    }

    if (const auto editBeatRange = getEditBeatRange(); ! editBeatRange.isEmpty())
    {
        if (auto splitStatus = launchHandle->advance (getProcessState().getSyncPoint(), editBeatRange.getLength());
            ! splitStatus.range1.isEmpty())
           processSplitSection (pc, splitStatus);
    }
}

//==============================================================================
void SlotControlNode::processSplitSection (ProcessContext& pc, LaunchHandle::SplitStatus status)
{
    const auto editBeatRange = getEditBeatRange();
    const auto totalRange = status.range1.isEmpty() ? status.range2
                                                    : (status.range2.isEmpty() ? status.range1
                                                                               : status.range1.getUnionWith (status.range2));
    const juce::NormalisableRange blockRangeBeats (totalRange.getStart().inBeats(),
                                                   totalRange.getEnd().inBeats());

    auto processSubSection = [this, &pc, &editBeatRange, &blockRangeBeats] (auto section, bool isPlaying, auto playStartTime)
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

        auto sectionBufferView = pc.buffers.audio.getFrameRange ({ startFrame, endFrame });
        ProcessContext subSection {
                                      sectionNumFrames, subSectionReferenceSampleRange,
                                      { sectionBufferView, pc.buffers.midi }
                                  };

        const auto startBeat  = editBeatRange.getStart() + editBeatRange.getLength() * proportion.getStart();
        const auto endBeat    = editBeatRange.getStart() + editBeatRange.getLength() * proportion.getEnd();
        processSection (subSection, { startBeat, endBeat }, section, isPlaying, playStartTime);
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

void SlotControlNode::processSection (ProcessContext& pc, BeatRange editBeatRange, BeatRange unloopedClipBeatRange,
                                      bool isPlaying, std::optional<BeatPosition> playStartTime)
{
    const juce::ScopeGuard scope { [this, isPlaying]
                                   {
                                       wasPlaying = isPlaying;
                                       localPlayheadState.playheadJumped = false;
                                   } };

    if (! isPlaying)
    {
        if (wasPlaying != isPlaying)
            processStop (pc);
        else
            pc.buffers.audio.clear();

        return;
    }
    else if (! wasPlaying)
    {
        // Force the playheadJumped state to true in order to resync MIDI streams etc.
        localPlayheadState.playheadJumped = true;
    }

    localProcessState.update (getSampleRate(), pc.referenceSampleRange,
                              ProcessState::UpdateContinuityFlags::no);

    // Update the offset for compatible Nodes
    if (playStartTime)
    {
        const auto clipEditOffset = editBeatRange.getStart() - unloopedClipBeatRange.getStart();
        const auto offset = clipEditOffset - toDuration (*playStartTime);

        for (auto n : offsetNodes)
            n->setDynamicOffsetBeats (offset);
    }

    // Prepare ordered Nodes
    for (auto& node : orderedNodes)
        node->prepareForNextBlock (pc.referenceSampleRange);

    // Process ordered Nodes
    for (auto& node : orderedNodes)
        node->process (pc.numSamples, pc.referenceSampleRange);

    // Copy audio from processed source Node
    auto sourceBuffers = input->getProcessedOutput();
    assert (sourceBuffers.audio.size == pc.buffers.audio.size);
    copyIfNotAliased (pc.buffers.audio, sourceBuffers.audio);
    pc.buffers.midi.copyFrom (sourceBuffers.midi);

    // Update last samples
    if (lastSamples)
    {
        const auto numChannels = pc.buffers.audio.size.numChannels;
        const auto numFrames = pc.buffers.audio.size.numFrames;
        jassert (lastSamples->size() == static_cast<size_t> (numChannels));

        for (choc::buffer::ChannelCount channel = 0; channel < numChannels; ++channel)
        {
            const auto dest = pc.buffers.audio.getIterator (channel).sample;
            auto& lastSample = (*lastSamples)[(size_t) channel];
            lastSample = dest[numFrames - 1];
        }
    }
}

void SlotControlNode::processStop (ProcessContext& pc)
{
    if (midiNode)
    {
        const auto timeForOneSample = TimeDuration::fromSamples (1, getSampleRate ());
        midiNode->killActiveNotes (pc.buffers.midi, (getEditTimeRange ().getLength () - timeForOneSample).inSeconds ());
    }

    // Fade out last sample
    if (lastSamples)
    {
        auto& buffer = pc.buffers.audio;
        const auto numChannels = buffer.size.numChannels;
        const auto numFrames = buffer.size.numFrames;

        if (const auto lastSampleFadeLength = std::min (numFrames, 40u);
            lastSampleFadeLength > 0)
        {
            for (choc::buffer::ChannelCount channel = 0; channel < numChannels; ++channel)
            {
                if (channel < (choc::buffer::ChannelCount) lastSamples->size())
                {
                    const auto dest = buffer.getIterator (channel).sample;
                    auto& lastSample = (*lastSamples)[(size_t) channel];

                    for (uint32_t i = 0; i < lastSampleFadeLength; ++i)
                    {
                        auto alpha = i / (float) lastSampleFadeLength;
                        dest[i] = lastSample * (1.0f - alpha);
                    }

                    lastSample = 0.0f;
                }
                else
                {
                    buffer.getChannel (channel).clear ();
                }
            }
        }
    }
}

}} // namespace tracktion { inline namespace engine
