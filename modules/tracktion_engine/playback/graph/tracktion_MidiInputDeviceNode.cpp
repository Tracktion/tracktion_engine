/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

MidiInputDeviceNode::MidiInputDeviceNode (InputDeviceInstance& idi, MidiInputDevice& owner, MPESourceID msi,
                                          tracktion::graph::PlayHeadState& phs, EditItemID targetID_)
    : instance (idi),
      midiInputDevice (owner),
      midiSourceID (msi),
      playHeadState (phs),
      targetID (targetID_)
{
}

MidiInputDeviceNode::~MidiInputDeviceNode()
{
    instance.removeConsumer (this);
}

tracktion::graph::NodeProperties MidiInputDeviceNode::getNodeProperties()
{
    tracktion::graph::NodeProperties props;
    props.hasMidi = true;
    props.nodeID = nodeID;
    return props;
}

void MidiInputDeviceNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    maxExpectedMsPerBuffer = (unsigned int) (((info.blockSize * 1000) / info.sampleRate) * 2 + 100);
    assert (getNodeProperties().nodeID == nodeID);

    if (auto oldNode = findNodeWithIDIfNonZero<MidiInputDeviceNode> (info.nodeGraphToReplace, nodeID))
        state = oldNode->state;

    if (! state)
        state = std::make_shared<NodeState>();

    {
        auto channelToUse = midiInputDevice.getChannelToUse();
        auto programToUse = midiInputDevice.getProgramToUse();

        if (channelToUse.isValid() && programToUse > 0)
            handleIncomingMidiMessage (juce::MidiMessage::programChange (channelToUse.getChannelNumber(), programToUse - 1), {});
    }

    {
        const std::lock_guard sl (state->liveMessagesMutex);
        state->liveRecordedMessages.clear();
        state->numLiveMessagesToPlay = 0;
    }

    instance.addConsumer (this);
    loopOverdubsChecker.startTimerHz (1);
}

bool MidiInputDeviceNode::isReadyToProcess()
{
    return true;
}

void MidiInputDeviceNode::process (ProcessContext& pc)
{
    SCOPED_REALTIME_CHECK

    const auto splitTimelineRange = referenceSampleRangeToSplitTimelineRange (playHeadState.playHead, pc.referenceSampleRange);
    jassert (! splitTimelineRange.isSplit); // This should be handled by the NodePlayer

    processSection (pc, splitTimelineRange.timelineRange1);
}

void MidiInputDeviceNode::handleIncomingMidiMessage (const juce::MidiMessage& message, MPESourceID sourceID)
{
    if (state->activeNode.load (std::memory_order_acquire) != this)
        return;

    auto channelToUse = midiInputDevice.getChannelToUse().getChannelNumber();

    {
        const std::lock_guard sl (state->incomingMessagesMutex);

        if (state->numIncomingMessages < state->incomingMessages.size())
        {
            auto& m = *state->incomingMessages[state->numIncomingMessages];
            m = { message, sourceID };

            if (channelToUse > 0)
                m.setChannel (channelToUse);

            ++(state->numIncomingMessages);
        }
    }

    auto& playHead = playHeadState.playHead;

    if (playHead.isPlaying() && isLivePlayOverActive())
    {
        const bool isLooping = state->canLoop && playHead.isLooping();
        const auto loopTimes = timeRangeFromSamples (playHead.getLoopRange(), sampleRate);
        const auto messageReferenceSamplePosition = tracktion::graph::timeToSample (message.getTimeStamp(), sampleRate);
        const auto timelinePosition = playHead.referenceSamplePositionToTimelinePosition (messageReferenceSamplePosition);
        auto sourceTime = TimePosition::fromSamples (timelinePosition, sampleRate);

        if (message.isNoteOff())
            sourceTime = midiInputDevice.quantisation.roundUp (sourceTime, instance.edit);
        else if (message.isNoteOn())
            sourceTime = midiInputDevice.quantisation.roundToNearest (sourceTime, instance.edit);

        if (isLooping)
            if (sourceTime >= loopTimes.getEnd())
                sourceTime = loopTimes.getStart();

        juce::MidiMessage newMess (message, sourceTime.inSeconds());

        if (channelToUse > 0)
            newMess.setChannel (channelToUse);

        const std::lock_guard sl (state->liveMessagesMutex);
        state->liveRecordedMessages.addMidiMessage (newMess, sourceTime.inSeconds(), sourceID);
    }
}

void MidiInputDeviceNode::processSection (ProcessContext& pc, juce::Range<int64_t> timelineRange)
{
    state->activeNode.store (this, std::memory_order_release);

    const auto editTime = tracktion::graph::sampleToTime (timelineRange, sampleRate);
    const auto timeNow = juce::Time::getApproximateMillisecondCounter();
    auto& destMidi = pc.buffers.midi;

    if (! playHeadState.isContiguousWithPreviousBlock())
        createProgramChanges (destMidi);

    {
        const std::lock_guard sl (state->incomingMessagesMutex);

        // if it's been a long time since the last block, clear the buffer because
        // it means we were muted or glitching
        if (timeNow > state->lastReadTime + maxExpectedMsPerBuffer)
        {
            //jassertfalse;
            state->numIncomingMessages = 0;
        }

        state->lastReadTime = timeNow;

        jassert (state->numIncomingMessages <= state->incomingMessages.size());

        if (auto num = juce::jmin ((size_t) state->numIncomingMessages, state->incomingMessages.size()))
        {
            // not quite right as the first event won't be at the start of the buffer, but near enough for live stuff
            const auto timeAdjust = state->incomingMessages.front()->getTimeStamp();

            for (size_t i = 0; i < num; ++i)
            {
                auto& m = *state->incomingMessages[i];

                destMidi.addMidiMessage (m,
                                         juce::jlimit (0.0, juce::jmax (0.0, editTime.getLength()), m.getTimeStamp() - timeAdjust),
                                         m.mpeSourceID);
            }
        }

        state->numIncomingMessages = 0;
    }

    const std::lock_guard sl (state->liveMessagesMutex);

    if (state->lastPlayheadTime > editTime.getStart())
        // when we loop, we can assume all the messages in here are now from the previous time round, so are playable
        state->numLiveMessagesToPlay = state->liveRecordedMessages.size();

    state->lastPlayheadTime = editTime.getStart();

    auto& mi = midiInputDevice;
    const bool createTakes = mi.recordingEnabled && ! (mi.mergeRecordings || mi.replaceExistingClips);

    if ((! createTakes && ! mi.replaceExistingClips)
        && state->numLiveMessagesToPlay > 0
        && playHeadState.playHead.isPlaying())
    {
        for (int i = 0; i < state->numLiveMessagesToPlay; ++i)
        {
            auto& m = state->liveRecordedMessages[i];
            auto t = m.getTimeStamp();

            if (editTime.contains (t))
                destMidi.add (m, t - editTime.getStart());
        }
    }
}

void MidiInputDeviceNode::createProgramChanges (MidiMessageArray& bufferForMidiMessages)
{
    auto channelToUse = midiInputDevice.getChannelToUse();
    auto programToUse = midiInputDevice.getProgramToUse();

    if (programToUse > 0 && channelToUse.isValid())
        bufferForMidiMessages.addMidiMessage (juce::MidiMessage::programChange (channelToUse.getChannelNumber(), programToUse - 1),
                                              0, midiSourceID);
}

bool MidiInputDeviceNode::isLivePlayOverActive()
{
    return instance.isRecording()
        && state->canLoop
        && instance.context.transport.looping;
}

void MidiInputDeviceNode::updateLoopOverdubs()
{
    bool canLoopFlag = false;

    // Only enable overdubs if a track is being recorded
    for (auto tID : instance.getTargets())
        if (instance.isRecording (tID) && instance.edit.trackCache.findItem (tID) != nullptr)
            canLoopFlag = true;

    state->canLoop = canLoopFlag;
}

void MidiInputDeviceNode::discardRecordings (EditItemID targetIDToDiscard)
{
    if (targetIDToDiscard != targetID)
        return;

    const std::lock_guard sl (state->liveMessagesMutex);
    state->liveRecordedMessages.clear();
}

}} // namespace tracktion { inline namespace engine
