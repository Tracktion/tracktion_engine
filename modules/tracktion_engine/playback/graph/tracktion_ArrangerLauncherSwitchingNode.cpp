/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include "tracktion_MidiNodeHelpers.h"

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
ArrangerLauncherSwitchingNode::ArrangerLauncherSwitchingNode (ProcessState& ps,
                                                              AudioTrack& at,
                                                              std::unique_ptr<Node> arrangerNode_,
                                                              std::vector<std::unique_ptr<SlotControlNode>> launcherNodes_)
    : TracktionEngineNode (ps),
      track (at),
      arrangerNode (std::move (arrangerNode_)),
      launcherNodes (std::move (launcherNodes_))
{
    assert (arrangerNode || ! launcherNodes.empty());
    setOptimisations ({ tracktion::graph::ClearBuffers::yes,
                        tracktion::graph::AllocateAudioBuffer::yes });

    launcherNodesCopy.reserve (launcherNodes.size());
    std::transform (launcherNodes.begin(), launcherNodes.end(), std::back_inserter (launcherNodesCopy),
        [] (auto& n) { return n.get(); });
    assert (launcherNodesCopy.size() == launcherNodes.size());
    assert (! contains_v (launcherNodesCopy, nullptr));
}

//==============================================================================
tracktion::graph::NodeProperties ArrangerLauncherSwitchingNode::getNodeProperties()
{
    constexpr size_t seed = 7653239033668669842; // std::hash<std::string_view>{} ("ArrangerLauncherSwitchingNode"sv)
    NodeProperties props = { .nodeID = hash (seed, track->itemID) };

    std::vector<Node*> nodes;
    for (auto n : getDirectInputNodes())    nodes.push_back (n);
    for (auto n : getInternalNodes())       nodes.push_back (n);

    for (auto n : nodes)
    {
        auto nodeProps = n->getNodeProperties();
        assert (nodeProps.nodeID != 0);
        props.hasAudio = props.hasAudio || nodeProps.hasAudio;
        props.hasMidi = props.hasMidi || nodeProps.hasMidi;
        props.numberOfChannels = std::max (props.numberOfChannels, nodeProps.numberOfChannels);
        props.latencyNumSamples = std::max (props.latencyNumSamples, nodeProps.latencyNumSamples);
        hash_combine (props.nodeID, nodeProps.nodeID);
    }

    return props;
}

std::vector<tracktion::graph::Node*> ArrangerLauncherSwitchingNode::getDirectInputNodes()
{
    std::vector<tracktion::graph::Node*> nodes;

    if (arrangerNode)
        nodes.push_back (arrangerNode.get());

    return nodes;
}

std::vector<Node*> ArrangerLauncherSwitchingNode::getInternalNodes()
{
    std::vector<tracktion::graph::Node*> nodes;

    for (auto& n : launcherNodes)
        nodes.push_back (static_cast<Node*> (n.get()));

    return nodes;
}

void ArrangerLauncherSwitchingNode::prepareToPlay (const PlaybackInitialisationInfo& info)
{
    const auto props = getNodeProperties();
    const int numChannels = props.numberOfChannels;

    if (auto oldGraph = info.nodeGraphToReplace)
    {
        if (auto oldNode = findNodeWithID<ArrangerLauncherSwitchingNode> (*oldGraph, props.nodeID))
        {
            if (oldNode->launcherSampleFader && oldNode->launcherSampleFader->getNumChannels() == static_cast<size_t> (numChannels))
               launcherSampleFader = oldNode->launcherSampleFader;

            if (oldNode->arrangerSampleFader && oldNode->arrangerSampleFader->getNumChannels() == static_cast<size_t> (numChannels))
                arrangerSampleFader = oldNode->arrangerSampleFader;

            if (oldNode->arrangerActiveNoteList)
                arrangerActiveNoteList = oldNode->arrangerActiveNoteList;

            if (oldNode->activeNode)
                activeNode = oldNode->activeNode;

            midiSourceID = oldNode->midiSourceID;
        }
    }

    if (! launcherSampleFader)
        launcherSampleFader = std::make_shared<SampleFader> (numChannels);

    if (! arrangerSampleFader)
        arrangerSampleFader = std::make_shared<SampleFader> (numChannels);

    if (! arrangerActiveNoteList)
        arrangerActiveNoteList = std::make_shared<ActiveNoteList>();

    if (! activeNode)
        activeNode = std::make_shared<std::atomic<ArrangerLauncherSwitchingNode*>> (this);

    for (auto& launcherNode : launcherNodes)
        launcherNode->initialise (info);
}

bool ArrangerLauncherSwitchingNode::isReadyToProcess()
{
    return ! arrangerNode || arrangerNode->hasProcessed();
}

void ArrangerLauncherSwitchingNode::prefetchBlock (juce::Range<int64_t> referenceSampleRange)
{
    for (auto& launcherNode : launcherNodes)
        launcherNode->prepareForNextBlock (referenceSampleRange);
}

void ArrangerLauncherSwitchingNode::process (ProcessContext& pc)
{
    activeNode->store (this, std::memory_order_release);

    auto destAudioView = pc.buffers.audio;
    assert (destAudioView.getNumChannels() == launcherSampleFader->getNumChannels());

    // Logic for determining what slots/arranger to play
    // - Iterate the slots to see if any are playing or are queued to play this block
    // - If they are playing, only play the slots and skip the arranger
    // - If a slot is queued, and its start position is within this block:
    //      - Play the slot
    //      - If the playSlotClips prop is false, play the arranger, but fade out at the slot start position
    // - If no slots are playing or queued AND the playSlotClips prop is false
    //      - Play the arranger
    //      - If we've just started playing the arranger, fade out the last slot samples and fade in the arranger
    const auto editBeatRange = getEditBeatRange();
    const auto playArranger = ! track->playSlotClips.get();
    const auto slotStatus = getSlotsStatus (launcherNodes,
                                            editBeatRange,
                                            getProcessState().getSyncPoint().monotonicBeat);

    launcherSampleFader->apply (destAudioView, SampleFader::FadeType::fadeOut);
    arrangerSampleFader->apply (destAudioView, SampleFader::FadeType::fadeOut);

    processLauncher (pc, slotStatus);

     if (playArranger)
        processArranger (pc, slotStatus);
}

//==============================================================================
void ArrangerLauncherSwitchingNode::processLauncher (ProcessContext& pc, const SlotClipStatus& slotStatus)
{
    auto destAudioView = pc.buffers.audio;
    const auto numFrames = destAudioView.getNumFrames();
    const auto editBeatRange = getEditBeatRange();

    if (! launcherNodes.empty())
    {
        sortPlayingOrQueuedClipsFirst();

        if (slotStatus.anyClipsPlaying || slotStatus.anyClipsQueued)
        {
            for (auto& launcherNode : launcherNodes)
            {
                using enum LaunchHandle::PlayState;
                using enum LaunchHandle::QueueState;
                const auto& lh = launcherNode->getLaunchHandle();
                const bool slotWasPlaying = lh.getPlayingStatus() == playing;
                const bool slotWasQueued = lh.getQueuedStatus() == playQueued;

                if (! (slotWasPlaying || slotWasQueued))
                    continue;

                launcherNode->Node::process (pc.numSamples, pc.referenceSampleRange);

                const bool slotIsPlaying = lh.getPlayingStatus() == playing;
                auto sourceBuffers = launcherNode->getProcessedOutput();
                const auto numSourceChannels = sourceBuffers.audio.getNumChannels();

                // We can add the whole block here as if the slot is stopped, part of the buffer will just be silent
                choc::buffer::add (destAudioView.getFirstChannels (numSourceChannels), sourceBuffers.audio);
                pc.buffers.midi.mergeFrom (sourceBuffers.midi);

                if (slotWasPlaying && ! slotIsPlaying)
                {
                    // Ramp out last 10 samples
                    const auto endFrame = beatToSamplePosition (slotStatus.beatsUntilQueuedStopTrimmedToBlock,
                                                                editBeatRange.getLength(), numFrames);
                    launcherSampleFader->trigger (10);
                    launcherSampleFader->applyAt (destAudioView,  endFrame, SampleFader::FadeType::fadeOut);
                }
            }
        }
    }

    launcherSampleFader->push (destAudioView);
}

void ArrangerLauncherSwitchingNode::processArranger (ProcessContext& pc, const SlotClipStatus& slotStatus)
{
    if (! arrangerNode)
        return;

    auto destAudioView = pc.buffers.audio;
    const auto editBeatRange = getEditBeatRange();
    const auto numFrames = destAudioView.getNumFrames();

    auto sourceBuffers = arrangerNode->getProcessedOutput();
    const auto numSourceChannels = sourceBuffers.audio.getNumChannels();

    if (slotStatus.beatsUntilQueuedStartTrimmedToBlock)
    {
        // Arranger about to stop so only use some of the buffer and trigger fade out
        if (numSourceChannels > 0)
        {
            const auto endFrame = beatToSamplePosition (slotStatus.beatsUntilQueuedStartTrimmedToBlock,
                                                        editBeatRange.getLength(), numFrames);

            auto destSubView = destAudioView.getFirstChannels (numSourceChannels).getStart (endFrame);
            auto sourceSubView = sourceBuffers.audio.getStart (endFrame);
            arrangerSampleFader->trigger (10);

            if (sourceSubView.getNumFrames() > 0)
            {
                arrangerSampleFader->push (sourceSubView);

                choc::buffer::add (destSubView, sourceSubView);
                launcherSampleFader->applyAt (destAudioView,  endFrame, SampleFader::FadeType::fadeOut);
            }

            const auto endTime = TimePosition::fromSamples (endFrame, getSampleRate());

            if (sourceBuffers.midi.isNotEmpty())
            {
                pc.buffers.midi.isAllNotesOff = sourceBuffers.midi.isAllNotesOff;

                for (auto& m : sourceBuffers.midi)
                {
                    if (m.getTimeStamp() > endTime.inSeconds())
                        continue;

                    pc.buffers.midi.add (m);

                    if (m.isNoteOn())
                        arrangerActiveNoteList->startNote (m.getChannel(), m.getNoteNumber());
                    else if (m.isNoteOff())
                        arrangerActiveNoteList->clearNote (m.getChannel(), m.getNoteNumber());
                }
            }

            MidiNodeHelpers::createNoteOffs (*arrangerActiveNoteList,
                                             pc.buffers.midi,
                                             midiSourceID,
                                             endTime.inSeconds(),
                                             getPlayHead().isPlaying());
        }
    }
    else
    {
        if (numSourceChannels > 0)
        {
            arrangerSampleFader->push (sourceBuffers.audio);
            choc::buffer::add (destAudioView.getFirstChannels (numSourceChannels), sourceBuffers.audio);
        }

        if (sourceBuffers.midi.isNotEmpty())
        {
            pc.buffers.midi.isAllNotesOff = sourceBuffers.midi.isAllNotesOff;

            for (auto& m : sourceBuffers.midi)
            {
                pc.buffers.midi.add (m);

                if (m.isNoteOn())
                    arrangerActiveNoteList->startNote (m.getChannel(), m.getNoteNumber());
                else if (m.isNoteOff())
                    arrangerActiveNoteList->clearNote (m.getChannel(), m.getNoteNumber());
            }
        }
    }
}

void ArrangerLauncherSwitchingNode::sortPlayingOrQueuedClipsFirst()
{
    using enum LaunchHandle::PlayState;
    using enum LaunchHandle::QueueState;
    sort (launcherNodes,
          [](auto& n1, auto& n2)
              {
                  auto stateToValue = [] (const LaunchHandle& lh)
                  {
                      if (lh.getPlayingStatus() == playing) return 1;

                      if (auto q1 = lh.getQueuedStatus())
                      {
                          if (q1 == playQueued) return 2;
                          if (q1 == stopQueued) return 3;
                      }

                      return 4;
                  };

                  auto v1 = stateToValue (n1->getLaunchHandle());
                  auto v2 = stateToValue (n2->getLaunchHandle());

                  return v1 < v2;
              });
}

void ArrangerLauncherSwitchingNode::updatePlaySlotsState()
{
    for (auto& n : launcherNodesCopy)
    {
        if (auto lh = n->getLaunchHandleIfNotUnique())
        {
            if (lh->getPlayingStatus() == LaunchHandle::PlayState::playing)
            {
                track->playSlotClips = true;
                return;
            }
        }
    }
}

//==============================================================================
choc::buffer::FrameCount ArrangerLauncherSwitchingNode::beatToSamplePosition (std::optional<BeatDuration> beat, BeatDuration numBeats, choc::buffer::FrameCount numFrames)
{
    if (! beat)
        return 0;

    if (numBeats == 0_bd)
        return 0;

    const auto framesPerBeats = numFrames / numBeats.inBeats();
    return static_cast<choc::buffer::FrameCount> (std::round (beat->inBeats() * framesPerBeats));
}

ArrangerLauncherSwitchingNode::SlotClipStatus ArrangerLauncherSwitchingNode::getSlotsStatus (const std::vector<std::unique_ptr<SlotControlNode>>& launcherNodes,
                                                                                             BeatRange editBeatRange, MonotonicBeat monotonicBeat)
{
    SlotClipStatus status;

    const BeatRange blockRange (monotonicBeat.v, editBeatRange.getLength());

    for (auto& n : launcherNodes)
    {
        const auto& lh = n->getLaunchHandle();

        if (lh.getPlayingStatus() == LaunchHandle::PlayState::playing)
            status.anyClipsPlaying = true;

        if (lh.getQueuedStatus() == LaunchHandle::QueueState::playQueued)
        {
            status.anyClipsQueued = true;

            const auto queuedPos = lh.getQueuedEventPosition();

            if (! queuedPos)
            {
                status.beatsUntilQueuedStartTrimmedToBlock = 0_bd;
                status.beatsUntilQueuedStart = 0_bd;
            }

            if (blockRange.contains (queuedPos->v))
                status.beatsUntilQueuedStartTrimmedToBlock = queuedPos->v - blockRange.getStart();
        }

        if (lh.getQueuedStatus() == LaunchHandle::QueueState::stopQueued)
        {
            status.anyClipsQueued = true;
            const auto queuedPos = lh.getQueuedEventPosition();

            if (! queuedPos)
                status.beatsUntilQueuedStopTrimmedToBlock = 0_bd;

            if (queuedPos && blockRange.contains (queuedPos->v))
                status.beatsUntilQueuedStopTrimmedToBlock = queuedPos->v - blockRange.getStart();
        }
    }

    return status;
}

//==============================================================================
void ArrangerLauncherSwitchingNode::sharedTimerCallback()
{
    if (activeNode && activeNode->load (std::memory_order_acquire) == this)
        updatePlaySlotsState();
}

}} // namespace tracktion { inline namespace engine
