/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

MidiNode::MidiNode (juce::MidiMessageSequence sequence,
                    juce::Range<int> midiChannelNumbers,
                    bool useMPE,
                    EditTimeRange editTimeRange,
                    LiveClipLevel liveClipLevel,
                    ProcessState& processStateToUse,
                    EditItemID editItemIDToUse,
                    std::function<bool()> shouldBeMuted)
    : TracktionEngineNode (processStateToUse),
      channelNumbers (midiChannelNumbers),
      useMPEChannelMode (useMPE),
      editSection (editTimeRange),
      clipLevel (liveClipLevel),
      editItemID (editItemIDToUse),
      shouldBeMutedDelegate (std::move (shouldBeMuted)),
      wasMute (liveClipLevel.isMute())
{
    jassert (channelNumbers.getStart() > 0 && channelNumbers.getEnd() <= 16);
    ms.emplace_back (sequence);

    for (auto& s : ms)
        s.updateMatchedPairs();
    
    controllerMessagesScratchBuffer.ensureStorageAllocated (32);
}

MidiNode::MidiNode (std::vector<juce::MidiMessageSequence> sequences,
                    juce::Range<int> midiChannelNumbers,
                    bool useMPE,
                    EditTimeRange editTimeRange,
                    LiveClipLevel liveClipLevel,
                    ProcessState& processStateToUse,
                    EditItemID editItemIDToUse,
                    std::function<bool()> shouldBeMuted)
    : TracktionEngineNode (processStateToUse),
      ms (std::move (sequences)),
      channelNumbers (midiChannelNumbers),
      useMPEChannelMode (useMPE),
      editSection (editTimeRange),
      clipLevel (liveClipLevel),
      editItemID (editItemIDToUse),
      shouldBeMutedDelegate (std::move (shouldBeMuted)),
      wasMute (liveClipLevel.isMute())
{
    jassert (channelNumbers.getStart() > 0 && channelNumbers.getEnd() <= 16);

    for (auto& s : ms)
        s.updateMatchedPairs();

    controllerMessagesScratchBuffer.ensureStorageAllocated (32);
}

tracktion_graph::NodeProperties MidiNode::getNodeProperties()
{
    tracktion_graph::NodeProperties props;
    props.hasMidi = true;
    props.nodeID = (size_t) editItemID.getRawID();
    return props;
}

void MidiNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    
    if (info.rootNodeToReplace != nullptr)
    {
        bool foundNodeToReplace = false;
        const auto nodeIDToLookFor = getNodeProperties().nodeID;
        
        visitNodes (info.rootNode, [&] (Node& n)
                    {
                        if (auto midiNode = dynamic_cast<MidiNode*> (&n))
                        {
                            if (midiNode->getNodeProperties().nodeID == nodeIDToLookFor)
                            {
                                midiSourceID = midiNode->midiSourceID;
                                foundNodeToReplace = true;
                            }
                        }
                    }, true);
        
        shouldCreateMessagesForTime = ! foundNodeToReplace;
    }
}

bool MidiNode::isReadyToProcess()
{
    return true;
}

void MidiNode::process (const ProcessContext& pc)
{
    SCOPED_REALTIME_CHECK
    processSection (pc, getTimelineSampleRange());
}

void MidiNode::processSection (const ProcessContext& pc, juce::Range<int64_t> timelineRange)
{
    if (timelineRange.isEmpty())
        return;
    
    if (shouldBeMutedDelegate && shouldBeMutedDelegate())
        return;

    if (ms.size() > 0 && timelineRange.getStart() < lastStart )
    {
        currentSequence++;
        if (currentSequence >= ms.size())
            currentSequence = 0;
    }
    lastStart = timelineRange.getStart();
    
    const auto sectionEditTime = getEditTimeRange();
    const auto localTime = sectionEditTime - editSection.getStart();

    if (sectionEditTime.getEnd() <= editSection.getStart()
        || sectionEditTime.getStart() >= editSection.getEnd())
       return;

    const bool mute = clipLevel.isMute();

    if (mute)
    {
        if (mute != wasMute)
        {
            wasMute = mute;
            createNoteOffs (pc.buffers.midi, ms[currentSequence], localTime.getStart(), 0.0, getPlayHead().isPlaying());
        }

        return;
    }

    if (! getPlayHeadState().isContiguousWithPreviousBlock() || localTime.getStart() <= 0.00001 || shouldCreateMessagesForTime)
    {
        createMessagesForTime (localTime.getStart(), pc.buffers.midi);
        shouldCreateMessagesForTime = false;
    }

    auto numEvents = ms[currentSequence].getNumEvents();

    if (numEvents != 0)
    {
        currentIndex = juce::jlimit (0, numEvents - 1, currentIndex);

        if (ms[currentSequence].getEventTime (currentIndex) >= localTime.getStart())
        {
            while (currentIndex > 0 && ms[currentSequence].getEventTime (currentIndex - 1) >= localTime.getStart())
                --currentIndex;
        }
        else
        {
            while (currentIndex < numEvents && ms[currentSequence].getEventTime (currentIndex) < localTime.getStart())
                ++currentIndex;
        }
    }

    auto volScale = clipLevel.getGain();

    for (;;)
    {
        if (auto meh = ms[currentSequence].getEventPointer (currentIndex++))
        {
            auto eventTime = meh->message.getTimeStamp();

            if (eventTime >= localTime.getEnd())
                break;

            eventTime -= localTime.getStart();

            if (eventTime >= 0)
            {
                juce::MidiMessage m (meh->message);
                m.multiplyVelocity (volScale);
                pc.buffers.midi.addMidiMessage (m, eventTime, midiSourceID);
            }
        }
        else
        {
            break;
        }
    }

    if (getPlayHeadState().isLastBlockOfLoop())
        createNoteOffs (pc.buffers.midi, ms[currentSequence], localTime.getEnd(), localTime.getLength(), getPlayHead().isPlaying());
}

void MidiNode::createMessagesForTime (double time, MidiMessageArray& buffer)
{
    if (useMPEChannelMode)
    {
        const int indexOfTime = ms[currentSequence].getNextIndexAtTime (time);

        controllerMessagesScratchBuffer.clearQuick();

        for (int i = channelNumbers.getStart(); i <= channelNumbers.getEnd(); ++i)
            MPEStartTrimmer::reconstructExpression (controllerMessagesScratchBuffer, ms[currentSequence], indexOfTime, i);

        for (auto& m : controllerMessagesScratchBuffer)
            buffer.addMidiMessage (m, 0.0001, midiSourceID);
    }
    else
    {
        {
            controllerMessagesScratchBuffer.clearQuick();

            for (int i = channelNumbers.getStart(); i <= channelNumbers.getEnd(); ++i)
            ms[currentSequence].createControllerUpdatesForTime (i, time, controllerMessagesScratchBuffer);

            for (auto& m : controllerMessagesScratchBuffer)
                buffer.addMidiMessage (m, midiSourceID);
        }

        if (! clipLevel.isMute())
        {
            auto volScale = clipLevel.getGain();

            for (int i = 0; i < ms[currentSequence].getNumEvents(); ++i)
            {
                if (auto meh = ms[currentSequence].getEventPointer (i))
                {
                    if (meh->noteOffObject != nullptr
                        && meh->message.isNoteOn())
                    {
                        if (meh->message.getTimeStamp() >= time)
                            break;

                        // don't play very short notes or ones that have already finished
                        if (meh->noteOffObject->message.getTimeStamp() > time + 0.0001)
                        {
                            juce::MidiMessage m (meh->message);
                            m.multiplyVelocity (volScale);

                            // give these a tiny offset to make sure they're played after the controller updates
                            buffer.addMidiMessage (m, 0.0001, midiSourceID);
                        }
                    }
                }
            }
        }
    }
}

void MidiNode::createNoteOffs (MidiMessageArray& destination, const juce::MidiMessageSequence& source,
                               double time, double midiTimeOffset, bool isPlaying)
{
    int activeChannels = 0;

    for (int i = 0; i < source.getNumEvents(); ++i)
    {
        if (auto meh = source.getEventPointer (i))
        {
            if (meh->message.isNoteOn())
            {
                activeChannels |= (1 << meh->message.getChannel());

                if (meh->message.getTimeStamp() < time
                     && meh->noteOffObject != nullptr
                     && meh->noteOffObject->message.getTimeStamp() > time)
                    destination.addMidiMessage (meh->noteOffObject->message, midiTimeOffset, midiSourceID);
            }
        }
        else
        {
            break;
        }
    }

    for (int i = 1; i <= 16; ++i)
    {
        if ((activeChannels & (1 << i)) != 0)
        {
            destination.addMidiMessage (juce::MidiMessage::controllerEvent (i, 66 /* sustain pedal off */, 0), midiTimeOffset, midiSourceID);
            destination.addMidiMessage (juce::MidiMessage::controllerEvent (i, 64 /* hold pedal off */, 0), midiTimeOffset, midiSourceID);

			// NB: Some buggy plugins seem to fail to respond to note-ons if they are preceded
            // by an all-notes-off, so avoid this while playing.
			if (! isPlaying)
	            destination.addMidiMessage (juce::MidiMessage::allNotesOff (i), midiTimeOffset, midiSourceID);
        }
    }
}

}
