/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


/** If you need to play back MPE data from a point *after* the data starts, it's
    important to reconstruct the expression values immediately preceding the last
    note-on, then the note on, then the last expression values before the trim point.

    If you don't, many instruments that rely on relative starting points (particularly
    common with CC74, 'timbre') or resetting voices to a start value before the note
    on, will sound wrong when played back.
*/
struct MPEStartTrimmer
{
    /** Reconstruct note expression for a particular channel. Reconstructed messages will
        be added to the mpeMessagesToAddAtStart array. These messages should be played back
        (in order) to properly restore the MPE 'state' at the trimIndex.
    */
    static void reconstructExpression (Array<MidiMessage>& mpeMessagesToAddAtStart,
                                       const juce::MidiMessageSequence& data,
                                       int trimIndex, int channel)
    {
        jassert (trimIndex < data.getNumEvents());

        const int lastNoteOnIndex = searchBackForNoteOn (data, trimIndex, channel);

        if (! wasFound (lastNoteOnIndex))
            return;

        const auto& noteOn = data.getEventPointer (lastNoteOnIndex)->message;
        const auto initial = searchBackForExpression (data, lastNoteOnIndex, channel, MessageToStopAt::noteOff);
        const auto mostRecent = searchBackForExpression (data, trimIndex, channel, MessageToStopAt::noteOn);

        const int centrePitchbend = MidiMessage::pitchbendToPitchwheelPos (0.0f, 12.f);

        mpeMessagesToAddAtStart.add (MidiMessage::controllerEvent       (channel, 74, wasFound (initial.timbre)    ? initial.timbre    : 64));
        mpeMessagesToAddAtStart.add (MidiMessage::channelPressureChange (channel,     wasFound (initial.pressure)  ? initial.pressure  : 0));
        mpeMessagesToAddAtStart.add (MidiMessage::pitchWheel            (channel,     wasFound (initial.pitchBend) ? initial.pitchBend : centrePitchbend));
        mpeMessagesToAddAtStart.add (MidiMessage::noteOn (channel, noteOn.getNoteNumber(), noteOn.getVelocity()));

        if (wasFound (mostRecent.timbre))
            mpeMessagesToAddAtStart.add (MidiMessage::controllerEvent (channel, 74, mostRecent.timbre));

        if (wasFound (mostRecent.pressure))
            mpeMessagesToAddAtStart.add (MidiMessage::channelPressureChange (channel, mostRecent.pressure));

        if (wasFound (mostRecent.pitchBend))
            mpeMessagesToAddAtStart.add (MidiMessage::pitchWheel (channel, mostRecent.pitchBend));
    }

private:
    static int searchBackForNoteOn (const juce::MidiMessageSequence& data, int startIndex, int channel)
    {
        while (--startIndex >= 0)
        {
            const auto& m = data.getEventPointer (startIndex)->message;

            if (m.getChannel() == channel)
            {
                if (m.isNoteOn (true))
                    return startIndex;

                if (m.isNoteOff (true))
                    return notFound;
            }
        }

        return notFound;
    }

    struct ExpressionData
    {
        int timbre;
        int pressure;
        int pitchBend;
    };

    enum MessageToStopAt
    {
        noteOn,
        noteOff
    };

    static ExpressionData searchBackForExpression (const juce::MidiMessageSequence& data,
                                                   int startIndex, int channel, MessageToStopAt stopAt)
    {
        int timbre    = notFound;
        int pressure  = notFound;
        int pitchBend = notFound;

        for (int i = startIndex; --i >= 0;) // Find initial note-on timbre value
        {
            const auto& m = data.getEventPointer (i)->message;

            if (m.getChannel() != channel)
                continue;

            if ((stopAt == noteOn ? m.isNoteOn (true) : m.isNoteOff (true))
                || (wasFound (timbre) && wasFound (pressure) && wasFound (pitchBend)))
                break;

            if (m.isController() && m.getControllerNumber() == 74 && ! wasFound (timbre))
                timbre = m.getControllerValue();

            else if (m.isChannelPressure() && ! wasFound (pressure))
                pressure = m.getChannelPressureValue();

            else if (m.isPitchWheel() && ! wasFound (pitchBend))
                pitchBend = m.getPitchWheelValue();
        }

        return { timbre, pressure, pitchBend };
    }

    static constexpr int notFound = -1;
    static bool wasFound (int v) { return v != notFound; };
};
