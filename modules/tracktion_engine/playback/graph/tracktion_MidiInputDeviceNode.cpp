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

MidiInputDeviceNode::MidiInputDeviceNode (InputDeviceInstance& idi, MidiInputDevice& owner, MidiMessageArray::MPESourceID msi,
                                          tracktion::graph::PlayHeadState& phs)
    : instance (idi),
      midiInputDevice (owner),
      midiSourceID (msi),
      playHeadState (phs)
{
    for (int i = 256; --i >= 0;)
        incomingMessages.add (new juce::MidiMessage (0x80, 0, 0));
}

MidiInputDeviceNode::~MidiInputDeviceNode()
{
    instance.removeConsumer (this);
}

tracktion::graph::NodeProperties MidiInputDeviceNode::getNodeProperties()
{
    tracktion::graph::NodeProperties props;
    props.hasMidi = true;
    return props;
}

void MidiInputDeviceNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    lastPlayheadTime = 0.0;
    numMessages = 0;
    maxExpectedMsPerBuffer = (unsigned int) (((info.blockSize * 1000) / info.sampleRate) * 2 + 100);

    {
        const juce::ScopedLock sl (bufferLock);

        auto channelToUse = midiInputDevice.getChannelToUse();
        auto programToUse = midiInputDevice.getProgramToUse();

        if (channelToUse.isValid() && programToUse > 0)
            handleIncomingMidiMessage (juce::MidiMessage::programChange (channelToUse.getChannelNumber(), programToUse - 1));
    }

    {
        const juce::ScopedLock sl (bufferLock);
        liveRecordedMessages.clear();
        numLiveMessagesToPlay = 0;
    }

    instance.addConsumer (this);
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

void MidiInputDeviceNode::handleIncomingMidiMessage (const juce::MidiMessage& message)
{
    auto channelToUse = midiInputDevice.getChannelToUse().getChannelNumber();

    {
        const juce::ScopedLock sl (bufferLock);

        if (numMessages < incomingMessages.size())
        {
            auto& m = *incomingMessages.getUnchecked (numMessages);
            m = message;

            if (channelToUse > 0)
                m.setChannel (channelToUse);

            ++numMessages;
        }
    }

    auto& playHead = playHeadState.playHead;

    if (playHead.isPlaying() && isLivePlayOverActive())
    {
        const auto loopTimes = timeRangeFromSamples (playHead.getLoopRange(), sampleRate);
        const auto messageReferenceSamplePosition = tracktion::graph::timeToSample (message.getTimeStamp(), sampleRate);
        const auto timelinePosition = playHead.referenceSamplePositionToTimelinePosition (messageReferenceSamplePosition);
        auto sourceTime = TimePosition::fromSamples (timelinePosition, sampleRate);

        if (message.isNoteOff())
            sourceTime = midiInputDevice.quantisation.roundUp (sourceTime, instance.edit);
        else if (message.isNoteOn())
            sourceTime = midiInputDevice.quantisation.roundToNearest (sourceTime, instance.edit);

        if (playHead.isLooping())
            if (sourceTime >= loopTimes.getEnd())
                sourceTime = loopTimes.getStart();

        juce::MidiMessage newMess (message, sourceTime.inSeconds());

        if (channelToUse > 0)
            newMess.setChannel (channelToUse);

        const juce::ScopedLock sl (liveInputLock);
        liveRecordedMessages.addMidiMessage (newMess, sourceTime.inSeconds(), midiSourceID);
    }
}

void MidiInputDeviceNode::processSection (ProcessContext& pc, juce::Range<int64_t> timelineRange)
{
    const auto editTime = tracktion::graph::sampleToTime (timelineRange, sampleRate);
    const auto timeNow = juce::Time::getApproximateMillisecondCounter();
    auto& destMidi = pc.buffers.midi;

    const juce::ScopedLock sl (bufferLock);

    if (! playHeadState.isContiguousWithPreviousBlock())
        createProgramChanges (destMidi);

    // if it's been a long time since the last block, clear the buffer because
    // it means we were muted or glitching
    if (timeNow > lastReadTime + maxExpectedMsPerBuffer)
    {
        //jassertfalse
        numMessages = 0;
    }

    lastReadTime = timeNow;

    jassert (numMessages <= incomingMessages.size());

    if (int num = juce::jmin (numMessages, incomingMessages.size()))
    {
        // not quite right as the first event won't be at the start of the buffer, but near enough for live stuff
        auto timeAdjust = incomingMessages.getUnchecked (0)->getTimeStamp();

        for (int i = 0; i < num; ++i)
        {
            auto m = incomingMessages.getUnchecked (i);
            destMidi.addMidiMessage (*m,
                                     juce::jlimit (0.0, juce::jmax (0.0, editTime.getLength()), m->getTimeStamp() - timeAdjust),
                                     midiSourceID);
        }
    }

    numMessages = 0;

    if (lastPlayheadTime > editTime.getStart())
        // when we loop, we can assume all the messages in here are now from the previous time round, so are playable
        numLiveMessagesToPlay = liveRecordedMessages.size();

    lastPlayheadTime = editTime.getStart();

    auto& mi = midiInputDevice;
    const bool createTakes = mi.recordingEnabled && ! (mi.mergeRecordings || mi.replaceExistingClips);

    if ((! createTakes && ! mi.replaceExistingClips)
        && numLiveMessagesToPlay > 0
        && playHeadState.playHead.isPlaying())
    {
        const juce::ScopedLock sl2 (liveInputLock);

        for (int i = 0; i < numLiveMessagesToPlay; ++i)
        {
            auto& m = liveRecordedMessages[i];
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
    return instance.isRecording() && instance.context.transport.looping;
}

void MidiInputDeviceNode::discardRecordings()
{
    const juce::ScopedLock sl (liveInputLock);
    liveRecordedMessages.clear();
}

}} // namespace tracktion { inline namespace engine
