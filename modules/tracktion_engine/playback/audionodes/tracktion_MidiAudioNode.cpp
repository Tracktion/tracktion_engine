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

MidiAudioNode::MidiAudioNode (MidiMessageSequence sequence,
                              Range<int> chans,
                              EditTimeRange editPos,
                              CachedValue<float>& volumeDb_,
                              CachedValue<bool>& mute_,
                              Clip& sourceClip, const MidiAudioNode* nodeToReplace)
    : editSection (editPos),
      channelNumbers (chans),
      volumeDb (volumeDb_),
      mute (mute_),
      clip (&sourceClip),
      wasMute (mute_),
      shouldCreateMessagesForTime (nodeToReplace == nullptr)
{
    jassert (channelNumbers.getStart() > 0 && channelNumbers.getEnd() <= 16);

    if (nodeToReplace != nullptr)
        midiSourceID = nodeToReplace->midiSourceID;

    ms.push_back (std::move (sequence));
    ms[0].updateMatchedPairs();
}

MidiAudioNode::MidiAudioNode (std::vector<juce::MidiMessageSequence> sequences,
                              Range<int> chans,
                              EditTimeRange editPos,
                              CachedValue<float>& volumeDb_,
                              CachedValue<bool>& mute_,
                              Clip& sourceClip, const MidiAudioNode* nodeToReplace)
    : ms (std::move (sequences)),
      editSection (editPos),
      channelNumbers (chans),
      volumeDb (volumeDb_),
      mute (mute_),
      clip (&sourceClip),
      wasMute (mute_),
      shouldCreateMessagesForTime (nodeToReplace == nullptr)
{
    jassert (channelNumbers.getStart() > 0 && channelNumbers.getEnd() <= 16);

    if (nodeToReplace != nullptr)
        midiSourceID = nodeToReplace->midiSourceID;

    for (auto& m : ms)
        m.updateMatchedPairs();
}

void MidiAudioNode::renderSection (const AudioRenderContext& rc, EditTimeRange editTime)
{
    if (rc.bufferForMidiMessages != nullptr)
    {
        auto localTime = editTime - editSection.getStart();

        if (ms.size() > 0 && editTime.getStart() < lastStart )
        {
            currentSequence++;
            if (currentSequence >= ms.size())
                currentSequence = 0;
        }
        lastStart = editTime.getStart();

        if (mute)
        {
            if (mute != wasMute)
            {
                wasMute = mute;
                createNoteOffs (*rc.bufferForMidiMessages, ms[currentSequence], localTime.getStart(), rc.midiBufferOffset, rc.playhead.isPlaying());
            }

            return;
        }

        if ((! rc.isContiguousWithPreviousBlock()) || localTime.getStart() <= 0.00001 || shouldCreateMessagesForTime)
        {
            createMessagesForTime (localTime.getStart(), *rc.bufferForMidiMessages, rc.midiBufferOffset);
            shouldCreateMessagesForTime = false;
        }

        auto numEvents = ms[currentSequence].getNumEvents();

        if (numEvents != 0)
        {
            currentIndex = jlimit (0, numEvents - 1, currentIndex);

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

        auto volScale = dbToGain (volumeDb);

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
                    MidiMessage m (meh->message);
                    m.multiplyVelocity (volScale);
                    rc.bufferForMidiMessages->addMidiMessage (m, rc.midiBufferOffset + eventTime, midiSourceID);
                }
            }
            else
            {
                break;
            }
        }

        if (rc.isLastBlockOfLoop())
            createNoteOffs (*rc.bufferForMidiMessages, ms[currentSequence], localTime.getEnd(), rc.midiBufferOffset + localTime.getLength(), rc.playhead.isPlaying());
    }
}

void MidiAudioNode::createMessagesForTime (double time, MidiMessageArray& buffer, double midiTimeOffset)
{
    const auto* midiClip = dynamic_cast<MidiClip*> (clip.get());

    if (midiClip != nullptr && midiClip->getMPEMode())
    {
        const int indexOfTime = ms[currentSequence].getNextIndexAtTime (time);

        Array<MidiMessage> mpeMessagesToAddAtStart;

        for (int i = channelNumbers.getStart(); i <= channelNumbers.getEnd(); ++i)
            MPEStartTrimmer::reconstructExpression (mpeMessagesToAddAtStart, ms[currentSequence], indexOfTime, i);

        for (auto& m : mpeMessagesToAddAtStart)
            buffer.addMidiMessage (m, midiTimeOffset + 0.0001, midiSourceID);
    }
    else
    {
        {
            juce::Array<juce::MidiMessage> controllerMessages;

            for (int i = channelNumbers.getStart(); i <= channelNumbers.getEnd(); ++i)
                ms[currentSequence].createControllerUpdatesForTime (i, time, controllerMessages);

            if (! controllerMessages.isEmpty())
                for (auto& m : controllerMessages)
                    buffer.addMidiMessage (m, midiSourceID);
        }

        if (! mute)
        {
            auto volScale = dbToGain (volumeDb);

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
                            MidiMessage m (meh->message);
                            m.multiplyVelocity (volScale);

                            // give these a tiny offset to make sure they're played after the controller updates
                            buffer.addMidiMessage (m, midiTimeOffset + 0.0001, midiSourceID);
                        }
                    }
                }
            }
        }
    }
}

void MidiAudioNode::createNoteOffs (MidiMessageArray& destination, const MidiMessageSequence& source,
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
            destination.addMidiMessage (MidiMessage::controllerEvent (i, 66 /* sustain pedal off */, 0), midiTimeOffset, midiSourceID);
            destination.addMidiMessage (MidiMessage::controllerEvent (i, 64 /* hold pedal off */, 0), midiTimeOffset, midiSourceID);

			// NB: Some buggy plugins seem to fail to respond to note-ons if they are preceded
            // by an all-notes-off, so avoid this while playing.
			if (! isPlaying)
	            destination.addMidiMessage (MidiMessage::allNotesOff (i), midiTimeOffset, midiSourceID);
        }
    }
}

void MidiAudioNode::getAudioNodeProperties (AudioNodeProperties& info)
{
    info.hasAudio = false;
    info.hasMidi = true;
    info.numberOfChannels = 0;
}

bool MidiAudioNode::isReadyToRender()
{
    return true;
}

void MidiAudioNode::visitNodes (const VisitorFn& v)
{
    v (*this);
}

bool MidiAudioNode::purgeSubNodes (bool, bool keepMidi)
{
    return keepMidi;
}

void MidiAudioNode::prepareAudioNodeToPlay (const PlaybackInitialisationInfo&)
{
}

void MidiAudioNode::releaseAudioNodeResources()
{
}

void MidiAudioNode::renderOver (const AudioRenderContext& rc)
{
    callRenderAdding (rc);
}

void MidiAudioNode::renderAdding (const AudioRenderContext& rc)
{
    invokeSplitRender (rc, *this);
}

//==============================================================================
MidiAudioNode* getClipIfPresentInNode (AudioNode* node, Clip& c)
{
    MidiAudioNode* existingNode = nullptr;

    if (node != nullptr)
        node->visitNodes ([&] (AudioNode& an)
                          {
                              if (existingNode == nullptr)
                                  if (auto midiNode = dynamic_cast<MidiAudioNode*> (&an))
                                      if (&midiNode->getClip() == &c)
                                          existingNode = midiNode;
                          });

    return existingNode;
}

}
