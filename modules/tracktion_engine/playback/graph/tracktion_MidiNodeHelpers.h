/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine
{

namespace MidiNodeHelpers
{
    inline void createMessagesForTime (MidiMessageArray& destBuffer,
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
                    destBuffer.addMidiMessage (m, 0.0001, midiSourceID);
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

    inline void createNoteOffs (MidiMessageArray& destination, const juce::MidiMessageSequence& sourceSequence,
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

    /** Asserts if any MIDI messages are timestamped outside the given range. */
    inline void sanityCheckMidiBuffer (const MidiMessageArray& midi, double maxTimeStamp)
    {
        for (const auto& m : midi)
            jassertquiet (m.getTimeStamp() < maxTimeStamp);
    }
}

}} // namespace tracktion { inline namespace engine
