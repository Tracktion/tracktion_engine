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

namespace MidiNodeHelpers
{
    void createMessagesForTime (MidiMessageArray& destBuffer,
                                juce::MidiMessageSequence& sourceSequence, double time,
                                juce::Range<int> channelNumbers,
                                LiveClipLevel& clipLevel,
                                bool useMPEChannelMode, MidiMessageArray::MPESourceID midiSourceID,
                                juce::Array<juce::MidiMessage>& controllerMessagesScratchBuffer)
    {
        if (useMPEChannelMode)
        {
            const int indexOfTime = sourceSequence.getNextIndexAtTime (time);

            controllerMessagesScratchBuffer.clearQuick();

            for (int i = channelNumbers.getStart(); i <= channelNumbers.getEnd(); ++i)
                MPEStartTrimmer::reconstructExpression (controllerMessagesScratchBuffer, sourceSequence, indexOfTime, i);

            for (auto& m : controllerMessagesScratchBuffer)
                destBuffer.addMidiMessage (m, 0.0001, midiSourceID);
        }
        else
        {
            {
                controllerMessagesScratchBuffer.clearQuick();

                for (int i = channelNumbers.getStart(); i <= channelNumbers.getEnd(); ++i)
                    sourceSequence.createControllerUpdatesForTime (i, time, controllerMessagesScratchBuffer);

                for (auto& m : controllerMessagesScratchBuffer)
                    destBuffer.addMidiMessage (m, midiSourceID);
            }

            if (! clipLevel.isMute())
            {
                auto volScale = clipLevel.getGain();

                for (int i = 0; i < sourceSequence.getNumEvents(); ++i)
                {
                    if (auto meh = sourceSequence.getEventPointer (i))
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
                                destBuffer.addMidiMessage (m, 0.0001, midiSourceID);
                            }
                        }
                    }
                }
            }
        }
    }

    void createNoteOffs (MidiMessageArray& destination, const juce::MidiMessageSequence& sourceSequence,
                         MidiMessageArray::MPESourceID midiSourceID,
                         double time, double midiTimeOffset, bool isPlaying)
    {
        int activeChannels = 0;

        for (int i = 0; i < sourceSequence.getNumEvents(); ++i)
        {
            if (auto meh = sourceSequence.getEventPointer (i))
            {
                if (meh->message.isNoteOn())
                {
                    activeChannels |= (1 << meh->message.getChannel());

                    if (meh->message.getTimeStamp() < time
                         && meh->noteOffObject != nullptr
                         && meh->noteOffObject->message.getTimeStamp() >= time)
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


//==============================================================================
//==============================================================================
MidiNode::MidiNode (std::vector<juce::MidiMessageSequence> sequences,
                    MidiList::TimeBase tb,
                    juce::Range<int> midiChannelNumbers,
                    bool useMPE,
                    juce::Range<double> editTimeRange,
                    LiveClipLevel liveClipLevel,
                    ProcessState& processStateToUse,
                    EditItemID editItemIDToUse,
                    std::function<bool()> shouldBeMuted)
    : TracktionEngineNode (processStateToUse),
      ms (std::move (sequences)),
      timeBase (tb),
      channelNumbers (midiChannelNumbers),
      useMPEChannelMode (useMPE),
      editRange (editTimeRange),
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

tracktion::graph::NodeProperties MidiNode::getNodeProperties()
{
    tracktion::graph::NodeProperties props;
    props.hasMidi = true;
    props.nodeID = (size_t) editItemID.getRawID();
    return props;
}

void MidiNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
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

void MidiNode::process (ProcessContext& pc)
{
    SCOPED_REALTIME_CHECK
    const auto timelineRange = getTimelineSampleRange();

    if (timelineRange.isEmpty())
        return;

    if (shouldBeMutedDelegate && shouldBeMutedDelegate())
        return;

    if (ms.size() > 0 && timelineRange.getStart() < lastStart)
        if (++currentSequence >= ms.size())
            currentSequence = 0;

    lastStart = timelineRange.getStart();

    if (timeBase == MidiList::TimeBase::beats)
    {
        const auto sectionEditTime = getEditBeatRange();

        if (sectionEditTime.isEmpty()
            || sectionEditTime.getEnd().inBeats() <= editRange.getStart()
            || sectionEditTime.getStart().inBeats() >= editRange.getEnd())
           return;

        const auto secondsPerBeat = getEditTimeRange().getLength().inSeconds() / sectionEditTime.getLength().inBeats();

        processSection (pc,
                        { sectionEditTime.getStart().inBeats(), sectionEditTime.getEnd().inBeats() },
                        secondsPerBeat, ms[currentSequence]);
    }
    else
    {
        const auto sectionEditTime = getEditTimeRange();

        if (sectionEditTime.isEmpty()
            || sectionEditTime.getEnd().inSeconds() <= editRange.getStart()
            || sectionEditTime.getStart().inSeconds() >= editRange.getEnd())
           return;

        processSection (pc,
                        { sectionEditTime.getStart().inSeconds(), sectionEditTime.getEnd().inSeconds() },
                        1.0, ms[currentSequence]);
    }
}

void MidiNode::processSection (Node::ProcessContext& pc,
                               juce::Range<double> sectionEditRange,
                               double secondsPerTimeBase,
                               juce::MidiMessageSequence& sequence)
{
    if (sectionEditRange.isEmpty()
        || sectionEditRange.getEnd() <= editRange.getStart()
        || sectionEditRange.getStart() >= editRange.getEnd())
       return;

    const auto localTime = sectionEditRange - editRange.getStart();
    const bool mute = clipLevel.isMute();

    if (mute)
    {
        if (mute != wasMute)
        {
            wasMute = mute;
            MidiNodeHelpers::createNoteOffs (pc.buffers.midi, sequence, midiSourceID, localTime.getStart(), {}, getPlayHead().isPlaying());
        }

        return;
    }

    if (! getPlayHeadState().isContiguousWithPreviousBlock() || localTime.getStart() <= 0.00001 || shouldCreateMessagesForTime)
    {
        MidiNodeHelpers::createMessagesForTime (pc.buffers.midi, sequence, localTime.getStart(),
                                                channelNumbers, clipLevel, useMPEChannelMode, midiSourceID,
                                                controllerMessagesScratchBuffer);
        shouldCreateMessagesForTime = false;
    }

    auto numEvents = sequence.getNumEvents();

    if (numEvents != 0)
    {
        currentIndex = juce::jlimit (0, numEvents - 1, currentIndex);

        if (sequence.getEventTime (currentIndex) >= localTime.getStart())
        {
            while (currentIndex > 0 && sequence.getEventTime (currentIndex - 1) >= localTime.getStart())
                --currentIndex;
        }
        else
        {
            while (currentIndex < numEvents && sequence.getEventTime (currentIndex) < localTime.getStart())
                ++currentIndex;
        }
    }

    auto volScale = clipLevel.getGain();
    const auto lastBlockOfLoop = getPlayHeadState().isLastBlockOfLoop();
    const double durationOfOneSample = sectionEditRange.getLength() / pc.numSamples;

    for (;;)
    {
        if (auto meh = sequence.getEventPointer (currentIndex++))
        {
            auto eventTime = meh->message.getTimeStamp();

            // This correction here is to avoid rounding errors converting to and from sample position and times
            const auto timeCorrection = lastBlockOfLoop ? (meh->message.isNoteOff() ? 0.0 : durationOfOneSample) : 0.0;

            if (eventTime >= (localTime.getEnd() - timeCorrection))
                break;

            eventTime -= localTime.getStart();

            if (eventTime >= 0.0)
            {
                juce::MidiMessage m (meh->message);
                m.multiplyVelocity (volScale);
                const auto eventTimeSeconds = eventTime * secondsPerTimeBase;
                pc.buffers.midi.addMidiMessage (m, eventTimeSeconds, midiSourceID);
            }
        }
        else
        {
            break;
        }
    }

    // N.B. if the note-off is added on the last time it may not be sent to the plugin which can break the active note-state.
    // To avoid this, make sure any added messages are nudged back by 0.00001s
    if (getPlayHeadState().isLastBlockOfLoop())
        MidiNodeHelpers::createNoteOffs (pc.buffers.midi, sequence, midiSourceID,
                                         localTime.getEnd(),
                                         localTime.getLength() - 0.00001,
                                         getPlayHead().isPlaying());
}



}} // namespace tracktion { inline namespace engine
