/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include "tracktion_MidiNodeHelpers.h"


namespace tracktion { inline namespace engine
{

//==============================================================================
using EditBeatPosition = double;
using ClipBeatPosition = double;
using SequenceBeatPosition = double;
using BlockBeatPosition = double;

using EditBeatDuration = double;
using ClipBeatDuration = double;
using BlockBeatDuration = double;

using EditBeatRange = juce::Range<double>;
using ClipBeatRange = juce::Range<double>;
using SequenceBeatRange = juce::Range<double>;
using BlockBeatRange = juce::Range<double>;

namespace chocMidiHelpers
{
    //==============================================================================
    struct PitchWheel
    {
        void addToBuffer (int channel, juce::Array<juce::MidiMessage>& dest) const
        {
            if (value)
                dest.add (juce::MidiMessage::pitchWheel (channel, static_cast<int> (*value)));
        }

        void update (uint32_t v)
        {
            value = v;
        }

    private:
        std::optional<uint32_t> value;
    };

    struct ControllerValues
    {
        void addToBuffer (int channel, juce::Array<juce::MidiMessage>& dest) const
        {
            std::for_each (std::begin (values), std::end (values),
                           [&, index = 0] (const auto& v) mutable
                           {
                               if (v)
                                   dest.add (juce::MidiMessage::controllerEvent (channel, index, *v));

                               ++index;
                           });
        }

        void update (int controller, uint8_t value)
        {
            values[controller] = value;
        }

    private:
        std::optional<uint8_t> values[128];
    };

    struct ProgramChange
    {
        void addToBuffer (int channel, double time, juce::Array<juce::MidiMessage>& dest) const
        {
            if (! value)
                return;

            if (bankLSB && bankMSB)
            {
                dest.add (juce::MidiMessage::controllerEvent (channel, 0x00, *bankMSB).withTimeStamp (time));
                dest.add (juce::MidiMessage::controllerEvent (channel, 0x20, *bankLSB).withTimeStamp (time));
            }

            dest.add (juce::MidiMessage::programChange (channel, *value).withTimeStamp (time));
        }

        // Returns true if this is a bank number change, and false otherwise.
        bool trySetBank (uint8_t controller, uint8_t v)
        {
            switch (controller)
            {
                case 0x00: bankMSB = v; return true;
                case 0x20: bankLSB = v; return true;
            }

            return false;
        }

        void setProgram (uint8_t v)
        {
            value = v;
        }

    private:
        std::optional<uint8_t> value, bankLSB, bankMSB;
    };

    struct ParameterNumberState
    {
        // If the effective parameter number has changed since the last time this function was called,
        // this will emit the current parameter in full (MSB and LSB).
        // This should be called before each data message (entry, increment, decrement: 0x06, 0x26, 0x60, 0x61)
        // to ensure that the data message operates on the correct parameter number.
        void sendIfNecessary (int channel, double time, juce::Array<juce::MidiMessage>& dest)
        {
            const auto newestMsb = newestKind == Kind::rpn ? newestRpnMsb : newestNrpnMsb;
            const auto newestLsb = newestKind == Kind::rpn ? newestRpnLsb : newestNrpnLsb;

            auto lastSent = std::tie (lastSentKind, lastSentMsb, lastSentLsb);
            const auto newest = std::tie (newestKind, newestMsb, newestLsb);

            if (lastSent == newest || ! newestMsb || ! newestLsb)
                return;

            dest.add (juce::MidiMessage::controllerEvent (channel, newestKind == Kind::rpn ? 0x65 : 0x63, *newestMsb).withTimeStamp (time));
            dest.add (juce::MidiMessage::controllerEvent (channel, newestKind == Kind::rpn ? 0x64 : 0x62, *newestLsb).withTimeStamp (time));

            lastSent = newest;
        }

        bool trySetProgramNumber (uint8_t controller, uint8_t value)
        {
            switch (controller)
            {
                case 0x65: newestRpnMsb  = value; newestKind = Kind::rpn;  return true;
                case 0x64: newestRpnLsb  = value; newestKind = Kind::rpn;  return true;
                case 0x63: newestNrpnMsb = value; newestKind = Kind::nrpn; return true;
                case 0x62: newestNrpnLsb = value; newestKind = Kind::nrpn; return true;
            }

            return false;
        }

    private:
        enum class Kind { rpn, nrpn };

        std::optional<uint8_t> newestRpnLsb, newestRpnMsb, newestNrpnLsb, newestNrpnMsb;
        std::optional<uint8_t> lastSentLsb, lastSentMsb;
        Kind lastSentKind = Kind::rpn, newestKind = Kind::rpn;
    };

    inline void createControllerUpdatesForTime (const choc::midi::Sequence& sequence,
                                                uint8_t channel, double time,
                                                juce::Array<juce::MidiMessage>& dest)
    {
        ProgramChange programChange;
        ControllerValues controllerValues;
        PitchWheel pitchWheel;
        ParameterNumberState parameterNumberState;

        for (const auto& event : sequence)
        {
            if (! event.message.isShortMessage())
                continue;

            const auto mm = event.message.getShortMessage();

            if (! (mm.getChannel1to16() == channel && event.timeStamp <= time))
                continue;

            if (mm.isController())
            {
                const auto num = mm.getControllerNumber();

                if (parameterNumberState.trySetProgramNumber (num, mm.getControllerValue()))
                    continue;

                if (programChange.trySetBank (num, mm.getControllerValue()))
                    continue;

                constexpr int passthroughs[] { 0x06, 0x26, 0x60, 0x61 };

                if (std::find (std::begin (passthroughs), std::end (passthroughs), num) != std::end (passthroughs))
                {
                    parameterNumberState.sendIfNecessary (channel, event.timeStamp, dest);
                    dest.add (toMidiMessage (event));
                }
                else
                {
                    controllerValues.update (num, mm.getControllerValue());
                }
            }
            else if (mm.isProgramChange())
            {
                programChange.setProgram (mm.getProgramChangeNumber());
            }
            else if (mm.isPitchWheel())
            {
                pitchWheel.update (mm.getPitchWheelValue());
            }
        }

        pitchWheel.addToBuffer (channel, dest);

        controllerValues.addToBuffer (channel, dest);

        // Also emits bank change messages if necessary.
        programChange.addToBuffer (channel, time, dest);

        // Set the parameter number to its final state.
        parameterNumberState.sendIfNecessary (channel, time, dest);
    }
}

//==============================================================================
//==============================================================================
namespace MidiHelpers
{
    /** Snips out a section of a sequence adding note on/off events for notes at the loop boundries. */
    inline juce::MidiMessageSequence createLoopSection (juce::MidiMessageSequence sourceSequence,
                                                        juce::Range<double> loopRange)
    {
        juce::MidiMessageSequence res;

        jassert (! loopRange.isEmpty());
        sourceSequence.updateMatchedPairs();

        for (int i = 0; i < sourceSequence.getNumEvents(); ++i)
        {
            if (auto meh = sourceSequence.getEventPointer (i))
            {
                if (meh->noteOffObject != nullptr
                    && meh->message.isNoteOn())
                {
                    juce::Range<double> noteRange (meh->message.getTimeStamp(),
                                                   meh->noteOffObject->message.getTimeStamp());

                    // Note before loop start
                    if (noteRange.getEnd() < loopRange.getStart())
                        continue;

                    // Past the end of the sequence
                    if (noteRange.getStart() >= loopRange.getEnd())
                        break;

                    // Crop note start to loop start
                    if (noteRange.getStart() < loopRange.getStart())
                        noteRange = noteRange.withStart (loopRange.getStart());

                    // Crop note end to loop end
                    if (noteRange.getEnd() > loopRange.getEnd())
                        noteRange = noteRange.withEnd (loopRange.getEnd());

                    res.addEvent ({ meh->message, noteRange.getStart() });
                    res.addEvent ({ meh->noteOffObject->message, noteRange.getEnd() });
                }
                else if (! meh->message.isNoteOff())
                {
                    if (meh->message.getTimeStamp() >= loopRange.getEnd())
                        break;

                    if (meh->message.getTimeStamp() < loopRange.getStart())
                        continue;

                    res.addEvent (meh->message);
                }
            }
        }

        res.updateMatchedPairs();
        res.sort();

        return res;
    }

    /** Snips out a section of a set of sequences adding note on/off events for notes at the loop boundries. */
    inline std::vector<juce::MidiMessageSequence> createLoopSection (const std::vector<juce::MidiMessageSequence>& sourceSequences,
                                                                     juce::Range<double> loopRange)
    {
        std::vector<juce::MidiMessageSequence> res;
        jassert (! loopRange.isEmpty());

        for (auto& seq : sourceSequences)
            res.push_back (createLoopSection (seq, loopRange));

        return res;
    }

    inline void applyQuantisationToSequence (const QuantisationType& q, juce::MidiMessageSequence& ms, bool canQuantiseNoteOffs)
    {
        if (! q.isEnabled())
            return;

        const bool quantiseNoteOffs = canQuantiseNoteOffs && q.isQuantisingNoteOffs();

        for (int i = ms.getNumEvents(); --i >= 0;)
        {
            auto* e = ms.getEventPointer (i);
            auto& m = e->message;

            if (m.isNoteOn())
            {
                const auto noteOnTime = (q.roundBeatToNearest (BeatPosition::fromBeats (m.getTimeStamp()))).inBeats();

                if (auto noteOff = e->noteOffObject)
                {
                    auto& mOff = noteOff->message;

                    if (quantiseNoteOffs)
                    {
                        auto noteOffTime = (q.roundBeatUp (BeatPosition::fromBeats (mOff.getTimeStamp()))).inBeats();

                        static constexpr double beatsToBumpUpBy = 1.0 / 512.0;

                        if (noteOffTime <= noteOnTime) // Don't want note on and off time the same
                            noteOffTime = q.roundBeatUp (BeatPosition::fromBeats (noteOnTime + beatsToBumpUpBy)).inBeats();

                        mOff.setTimeStamp (noteOffTime);
                    }
                    else
                    {
                        // nudge the note-up backwards just a bit to make sure the ordering is correct
                        mOff.setTimeStamp (noteOnTime + (mOff.getTimeStamp() - m.getTimeStamp()) - 0.00001);
                    }
                }

                m.setTimeStamp (noteOnTime);
            }
            else if (m.isNoteOff() && quantiseNoteOffs)
            {
                m.setTimeStamp ((q.roundBeatUp (BeatPosition::fromBeats (m.getTimeStamp()))).inBeats());
            }
        }
    }

    inline void applyGrooveToSequence (const GrooveTemplate& groove, float grooveStrength, juce::MidiMessageSequence& ms)
    {
        for (auto mh : ms)
        {
            auto& m = mh->message;

            if (m.isNoteOn() || m.isNoteOff())
                m.setTimeStamp (groove.beatsTimeToGroovyTime (BeatPosition::fromBeats (m.getTimeStamp()), grooveStrength).inBeats());
        }
    }

    inline void createNoteOffs (ActiveNoteList& activeNoteList,
                                MidiMessageArray& destination,
                                MidiMessageArray::MPESourceID midiSourceID,
                                double midiTimeOffset, bool isPlaying)
    {
        int activeChannels = 0;

        // First send note-off events for currently playing notes
        activeNoteList.iterate ([&] (int channel, int noteNumber)
                                {
                                    activeChannels |= (1 << channel);
                                    destination.addMidiMessage (juce::MidiMessage::noteOff (channel, noteNumber), midiTimeOffset, midiSourceID);
                                });
        activeNoteList.reset();

        // Send controller off events for used channels
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

    inline choc::midi::Sequence& addSequence (choc::midi::Sequence& dest, const juce::MidiMessageSequence& src, double timeStampOffset)
    {
        for (auto meh : src)
        {
            dest.events.push_back ({ meh->message.getTimeStamp() + timeStampOffset,
                                     { meh->message.getRawData(), (size_t) meh->message.getRawDataSize() } });
        }

        return dest;
    }

    inline void createNoteOffMap (std::vector<std::pair<size_t, size_t>>& noteOffMap,
                                  const choc::midi::Sequence& seq)
    {
        noteOffMap.clear();
        const auto seqLen = seq.events.size();

        for (size_t i = 0; i < seqLen; ++i)
        {
            const auto& e = seq.events[i].message;

            if (! e.isShortMessage())
                continue;

            const auto m = e.getShortMessage();

            if (m.isNoteOn())
            {
                const auto note = m.getNoteNumber();
                const auto chan = m.getChannel0to15();

                for (size_t j = i + 1; j < seqLen; ++j)
                {
                    const auto& e2 = seq.events[j].message;

                    if (! e2.isShortMessage())
                        continue;

                    const auto m2 = e2.getShortMessage();

                    if (m2.getNoteNumber() == note
                        && m2.getChannel0to15() == chan
                        && m2.isNoteOff())
                    {
                        noteOffMap.emplace_back (std::make_pair (i, j));
                        break;
                    }
                }
            }
        }
    }

    inline choc::midi::Sequence::Event* getNoteOff (size_t noteOnIndex,
                                                    choc::midi::Sequence& ms,
                                                    const std::vector<std::pair<size_t, size_t>>& noteOffMap)
    {
        auto found = std::find_if (noteOffMap.begin(), noteOffMap.end(),
                                   [noteOnIndex] (const auto& m) { return m.first == noteOnIndex; });

        if (found != noteOffMap.end())
            return &ms.events[found->second];

        return {};
    }

    inline const choc::midi::Sequence::Event* getNoteOff (size_t noteOnIndex,
                                                          const choc::midi::Sequence& ms,
                                                          const std::vector<std::pair<size_t, size_t>>& noteOffMap)
    {
        auto found = std::find_if (noteOffMap.begin(), noteOffMap.end(),
                                   [noteOnIndex] (const auto& m) { return m.first == noteOnIndex; });

        if (found != noteOffMap.end())
            return &ms.events[found->second];

        return {};
    }

    inline void applyQuantisationToSequence (const QuantisationType& q, bool canQuantiseNoteOffs,
                                             choc::midi::Sequence& ms, const std::vector<std::pair<size_t, size_t>>& noteOffMap)
    {
        if (! q.isEnabled())
            return;

        const bool quantiseNoteOffs = canQuantiseNoteOffs && q.isQuantisingNoteOffs();

        size_t index = ms.events.size();
        std::for_each (ms.events.rbegin(), ms.events.rend(),
                       [&] (auto& e)
                       {
                           --index;

                           if (! e.message.isShortMessage())
                               return;

                           const auto m = e.message.getShortMessage();

                           if (m.isNoteOn())
                           {
                               const auto noteOnTime = q.roundBeatToNearest (BeatPosition::fromBeats (e.timeStamp)).inBeats();

                               if (auto noteOff = getNoteOff (index, ms, noteOffMap))
                               {
                                   if (quantiseNoteOffs)
                                   {
                                       auto noteOffTime = (q.roundBeatUp (BeatPosition::fromBeats (noteOff->timeStamp))).inBeats();

                                       static constexpr double beatsToBumpUpBy = 1.0 / 512.0;

                                       if (noteOffTime <= noteOnTime) // Don't want note on and off time the same
                                           noteOffTime = q.roundBeatUp (BeatPosition::fromBeats (noteOnTime + beatsToBumpUpBy)).inBeats();

                                       noteOff->timeStamp = noteOffTime;
                                   }
                                   else
                                   {
                                       // nudge the note-up backwards just a bit to make sure the ordering is correct
                                       noteOff->timeStamp = (noteOnTime + (noteOff->timeStamp - e.timeStamp) - 0.00001);
                                   }
                               }

                               e.timeStamp = noteOnTime;
                           }
                           else if (m.isNoteOff() && quantiseNoteOffs)
                           {
                               e.timeStamp = q.roundBeatUp (BeatPosition::fromBeats (e.timeStamp)).inBeats();
                           }
                       });
    }

    inline void applyGrooveToSequence (const GrooveTemplate& groove, float grooveStrength, choc::midi::Sequence& ms)
    {
        for (auto& e : ms)
        {
            if (! e.message.isShortMessage())
                continue;

            const auto m = e.message.getShortMessage();

            if (m.isNoteOn() || m.isNoteOff())
                e.timeStamp = groove.beatsTimeToGroovyTime (BeatPosition::fromBeats (e.timeStamp), grooveStrength).inBeats();
        }
    }

    inline void createMessagesForTime (MidiMessageArray& destBuffer,
                                       const choc::midi::Sequence& sourceSequence,
                                       const std::vector<std::pair<size_t, size_t>>& noteOffMap,
                                       double time,
                                       juce::Range<int> channelNumbers,
                                       LiveClipLevel& clipLevel,
                                       bool useMPEChannelMode, MidiMessageArray::MPESourceID midiSourceID,
                                       juce::Array<juce::MidiMessage>& controllerMessagesScratchBuffer)
    {
        if (useMPEChannelMode)
        {
            const auto indexOfTime = [&]() -> size_t
                                     {
                                         const auto numEvents = sourceSequence.events.size();

                                         for (size_t i = 0; i < numEvents; ++i)
                                             if (sourceSequence.events[i].timeStamp >= time)
                                                 return i;

                                         return {};
                                     }();

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
                    chocMidiHelpers::createControllerUpdatesForTime (sourceSequence, (uint8_t) i, time, controllerMessagesScratchBuffer);

                for (auto& m : controllerMessagesScratchBuffer)
                    destBuffer.addMidiMessage (m, midiSourceID);
            }

            if (! clipLevel.isMute())
            {
                auto volScale = clipLevel.getGain();

                for (size_t i = 0; i < sourceSequence.events.size(); ++i)
                {
                    auto e = sourceSequence.events[i];

                    if (! e.message.isShortMessage())
                        continue;

                    const auto m = e.message.getShortMessage();

                    if (! m.isNoteOn())
                        continue;

                    if (auto noteOffEvent = getNoteOff (i, sourceSequence, noteOffMap))
                    {
                        if (e.timeStamp >= time)
                            break;

                        // don't play very short notes or ones that have already finished
                        if (noteOffEvent->timeStamp > time + 0.0001)
                        {
                            juce::MidiMessage m2 ((int) m.data[0], (int) m.data[1], (int) m.data[2], e.timeStamp);
                            m2.multiplyVelocity (volScale);

                            // give these a tiny offset to make sure they're played after the controller updates
                            destBuffer.addMidiMessage (m2, 0.0001, midiSourceID);
                        }
                    }
                }
            }
        }
    }

    inline ActiveNoteList getNotesOnAtTime (const choc::midi::Sequence& sourceSequence,
                                            const std::vector<std::pair<size_t, size_t>>& noteOffMap,
                                            double time,
                                            juce::Range<int> channelNumbers, LiveClipLevel& clipLevel)
    {
        ActiveNoteList noteList;

        if (clipLevel.isMute())
            return {};

        for (size_t i = 0; i < sourceSequence.events.size(); ++i)
        {
            auto e = sourceSequence.events[i];

            if (! e.message.isShortMessage())
                continue;

            const auto m = e.message.getShortMessage();

            if (! m.isNoteOn())
                continue;

            if (! channelNumbers.contains ((int) m.getChannel1to16()))
                continue;

            if (auto noteOffEvent = getNoteOff (i, sourceSequence, noteOffMap))
            {
                if (e.timeStamp >= time)
                    break;

                // don't play very short notes or ones that have already finished
                if (noteOffEvent->timeStamp > time + 0.0001)
                    noteList.startNote ((int) m.getChannel1to16(), (int) m.getNoteNumber());
            }
        }

        return noteList;
    }
}

//==============================================================================
//==============================================================================
class MidiGenerator
{
public:
    MidiGenerator() = default;
    virtual ~MidiGenerator() = default;

    //==============================================================================
    virtual void createMessagesForTime (MidiMessageArray& destBuffer,
                                        double time,
                                        ActiveNoteList&,
                                        juce::Range<int> channelNumbers,
                                        LiveClipLevel&,
                                        bool useMPEChannelMode, MidiMessageArray::MPESourceID,
                                        juce::Array<juce::MidiMessage>& controllerMessagesScratchBuffer)
    {
        juce::ignoreUnused (destBuffer, time, channelNumbers, useMPEChannelMode, controllerMessagesScratchBuffer);
    }

    virtual ActiveNoteList getNotesOnAtTime (double /*time*/, juce::Range<int> /*channelNumbers*/, LiveClipLevel&)
    {
        jassertfalse; // You shouldn't be calling the default implementation of this!
        return {};
    }

    //==============================================================================
    virtual void cacheSequence (double /*offset*/) {}

    virtual void setTime (double) = 0;
    virtual bool advance() = 0;

    virtual bool exhausted() = 0;
    virtual juce::MidiMessage getEvent() = 0;
};


//==============================================================================
struct EventGenerator   : public MidiGenerator
{
    EventGenerator (const choc::midi::Sequence& seq,
                    const std::vector<std::pair<size_t, size_t>>& noteOffs)
        : sequence (seq), noteOffMap (noteOffs)
    {
    }

    void createMessagesForTime (MidiMessageArray& destBuffer,
                                SequenceBeatPosition time,
                                ActiveNoteList& activeNoteList,
                                juce::Range<int> channelNumbers,
                                LiveClipLevel& clipLevel,
                                bool useMPEChannelMode, MidiMessageArray::MPESourceID midiSourceID,
                                juce::Array<juce::MidiMessage>& controllerMessagesScratchBuffer) override
    {
        thread_local MidiMessageArray scratchBuffer;
        scratchBuffer.clear();

        MidiHelpers::createMessagesForTime (scratchBuffer,
                                            sequence, noteOffMap,
                                            time,
                                            channelNumbers,
                                            clipLevel,
                                            useMPEChannelMode, midiSourceID,
                                            controllerMessagesScratchBuffer);

        // This isn't quite right as there could be notes that are turned on in the original buffer after the scratch buffer?
        for (const auto& e : scratchBuffer)
        {
            if (e.isNoteOn())
                activeNoteList.startNote (e.getChannel(), e.getNoteNumber());
            else if (e.isNoteOff())
                activeNoteList.clearNote (e.getChannel(), e.getNoteNumber());
        }

        destBuffer.mergeFrom (scratchBuffer);
    }

    ActiveNoteList getNotesOnAtTime (SequenceBeatPosition time, juce::Range<int> channelNumbers, LiveClipLevel& clipLevel) override
    {
        return MidiHelpers::getNotesOnAtTime (sequence, noteOffMap,
                                              time,
                                              channelNumbers,
                                              clipLevel);
    }

    void setTime (SequenceBeatPosition pos) override
    {
        auto numEvents = sequence.events.size();

        // Set the index to the start of the range
        if (numEvents != 0)
        {
            currentIndex = std::clamp<size_t> (currentIndex, 0, numEvents - 1);

            if (sequence.events[currentIndex].timeStamp >= pos)
            {
                while (currentIndex > 0 && sequence.events[currentIndex - 1].timeStamp >= pos)
                    --currentIndex;
            }
            else
            {
                while (currentIndex < numEvents && sequence.events[currentIndex].timeStamp < pos)
                    ++currentIndex;
            }
        }
    }

    juce::MidiMessage getEvent() override
    {
        [[ maybe_unused ]] auto numEvents = sequence.events.size();
        jassert (currentIndex < numEvents);

        return toMidiMessage (sequence.events[currentIndex]);
    }

    bool advance() override
    {
        ++currentIndex;
        return ! exhausted();
    }

    bool exhausted() override
    {
        return currentIndex >= sequence.events.size();
    }

    const choc::midi::Sequence& sequence;
    const std::vector<std::pair<size_t, size_t>>& noteOffMap;
    size_t currentIndex = 0;
};


//==============================================================================
//==============================================================================
class CachingMidiEventGenerator : public MidiGenerator
{
public:
    CachingMidiEventGenerator (std::vector<juce::MidiMessageSequence> seq,
                               QuantisationType qt,
                               const GrooveTemplate& grooveTemplate, float grooveStrength_)
        : sequences (std::move (seq)),
          quantisation (std::move (qt)),
          groove (grooveTemplate),
          grooveStrength (grooveStrength_)
    {
        // Cache the sequence at 0.0 time to reserve the required storage
        cacheSequence (0.0);

        // Reserve the scratch space for the note on/off map
        size_t maxNumEvents = 0, maxNumNoteOns = 0;

        for (auto& sequence : sequences)
        {
            size_t squenceNumEvents = 0, squenceNumNoteOns = 0;

            for (auto meh : sequence)
            {
                ++squenceNumEvents;

                if (meh->message.isNoteOn())
                    ++squenceNumNoteOns;
            }

            maxNumEvents = std::max (squenceNumEvents, maxNumEvents);
            maxNumNoteOns = std::max (squenceNumNoteOns, maxNumNoteOns);
        }

        noteOffMap.reserve (maxNumNoteOns);
        currentSequence.events.reserve (maxNumEvents);
    }

    void createMessagesForTime (MidiMessageArray& destBuffer,
                                EditBeatPosition editBeatPosition,
                                ActiveNoteList& noteList,
                                juce::Range<int> channelNumbers,
                                LiveClipLevel& clipLevel,
                                bool useMPEChannelMode, MidiMessageArray::MPESourceID midiSourceID,
                                juce::Array<juce::MidiMessage>& controllerMessagesScratchBuffer) override
    {
        generator.createMessagesForTime (destBuffer,
                                         editBeatPosition,
                                         noteList,
                                         channelNumbers,
                                         clipLevel,
                                         useMPEChannelMode, midiSourceID,
                                         controllerMessagesScratchBuffer);
    }

    ActiveNoteList getNotesOnAtTime (EditBeatPosition time, juce::Range<int> channelNumbers, LiveClipLevel& clipLevel) override
    {
        return MidiHelpers::getNotesOnAtTime (currentSequence, noteOffMap,
                                              time,
                                              channelNumbers,
                                              clipLevel);
    }

    void setTime (EditBeatPosition editBeatPosition) override
    {
        generator.setTime (editBeatPosition);
    }

    void cacheSequence (double offsetBeats) override
    {
        // Create a new sequence by:
        // - Iterating the current sequence
        // - Adding the offset timestamp to get Edit times
        // - Applying the quantisation
        // - Applying the groove
        // - Sortign so events are in order
        // - Setting the sequence to be iterated
        // - Updating the offset used

        if (sequences.size() > 0)
            if (++currentSequenceIndex >= sequences.size())
                currentSequenceIndex = 0;

        // Create the cached sequence (without allocating)
        currentSequence.events.clear();

        if (currentSequenceIndex < sequences.size())
            MidiHelpers::addSequence (currentSequence, sequences[currentSequenceIndex], offsetBeats);

        jassert (std::is_sorted (currentSequence.begin(), currentSequence.end()));
        MidiHelpers::createNoteOffMap (noteOffMap, currentSequence);
        MidiHelpers::applyQuantisationToSequence (quantisation, false, currentSequence, noteOffMap);

        if (! groove.isEmpty())
            MidiHelpers::applyGrooveToSequence (groove, grooveStrength, currentSequence);

        currentSequence.sortEvents();
        MidiHelpers::createNoteOffMap (noteOffMap, currentSequence);

        cachedSequenceOffset = offsetBeats;
    }

    juce::MidiMessage getEvent() override
    {
        auto e = generator.getEvent();
        e.addToTimeStamp (-cachedSequenceOffset);
        return e;
    }

    bool advance() override
    {
        return generator.advance();
    }

    bool exhausted() override
    {
        return generator.exhausted();
    }

private:
    std::vector<juce::MidiMessageSequence> sequences;

    choc::midi::Sequence currentSequence;
    std::vector<std::pair<size_t, size_t>> noteOffMap;
    EventGenerator generator { currentSequence, noteOffMap };

    const QuantisationType quantisation;
    const GrooveTemplate groove;
    float grooveStrength = 0.0;

    size_t currentSequenceIndex = 0;
    double cachedSequenceOffset = 0.0;
};

//==============================================================================
//==============================================================================
class LoopedMidiEventGenerator : public MidiGenerator
{
public:
    LoopedMidiEventGenerator (std::unique_ptr<MidiGenerator> gen,
                              std::shared_ptr<ActiveNoteList> anl,
                              EditBeatRange clipRangeToUse,
                              ClipBeatRange loopTimesToUse)
        : generator (std::move (gen)),
          activeNoteList (std::move (anl)),
          clipRange (clipRangeToUse),
          loopTimes (loopTimesToUse)
    {
        assert (activeNoteList);
    }

    void createMessagesForTime (MidiMessageArray& destBuffer,
                                EditBeatPosition editBeatPosition,
                                ActiveNoteList& noteList,
                                juce::Range<int> channelNumbers,
                                LiveClipLevel& clipLevel,
                                bool useMPEChannelMode, MidiMessageArray::MPESourceID midiSourceID,
                                juce::Array<juce::MidiMessage>& controllerMessagesScratchBuffer) override
    {
        // Ensure the correct sequence is cached
        setTime (editBeatPosition);

        generator->createMessagesForTime (destBuffer,
                                          editBeatPositionToSequenceBeatPosition (editBeatPosition),
                                          noteList,
                                          channelNumbers,
                                          clipLevel,
                                          useMPEChannelMode, midiSourceID,
                                          controllerMessagesScratchBuffer);
    }

    ActiveNoteList getNotesOnAtTime (EditBeatPosition editBeatPosition, juce::Range<int> channelNumbers, LiveClipLevel& clipLevel) override
    {
        return generator->getNotesOnAtTime (editBeatPositionToSequenceBeatPosition (editBeatPosition),
                                            channelNumbers,
                                            clipLevel);
    }

    void setTime (EditBeatPosition editBeatPosition) override
    {
        const ClipBeatPosition clipPos = editBeatPosition - clipRange.getStart();

        if (loopTimes.isEmpty())
        {
            generator->setTime (clipPos);
        }
        else
        {
            const SequenceBeatPosition sequencePos = loopTimes.getStart() + std::fmod (clipPos, loopTimes.getLength());

            setLoopIndex (static_cast<int> (clipPos / loopTimes.getLength()));
            generator->setTime (sequencePos);
        }
    }

    juce::MidiMessage getEvent() override
    {
        const auto offsetBeats = clipRange.getStart() - loopTimes.getStart() + (loopIndex * loopTimes.getLength());

        auto e = generator->getEvent();
        e.addToTimeStamp (offsetBeats);
        return e;
    }

    bool advance() override
    {
        generator->advance();

        if (exhausted() && ! loopTimes.isEmpty())
        {
            setLoopIndex (loopIndex + 1);
            generator->setTime (0.0);
        }

        return exhausted();
    }

    bool exhausted() override
    {
        return generator->exhausted();
    }

private:
    //==============================================================================
    std::unique_ptr<MidiGenerator> generator;
    std::shared_ptr<ActiveNoteList> activeNoteList;
    const EditBeatRange clipRange;
    const ClipBeatRange loopTimes;
    int loopIndex = 0;

    SequenceBeatPosition editBeatPositionToSequenceBeatPosition (EditBeatPosition editBeatPosition) const
    {
        const ClipBeatPosition clipPos = editBeatPosition - clipRange.getStart();

        if (loopTimes.isEmpty())
            return clipPos;

        const SequenceBeatPosition sequencePos = loopTimes.getStart() + std::fmod (clipPos, loopTimes.getLength());

        return sequencePos;
    }

    void setLoopIndex (int newLoopIndex)
    {
        if (newLoopIndex == loopIndex)
            return;

        loopIndex = newLoopIndex;
        const auto sequenceOffset = clipRange.getStart() + (loopIndex * loopTimes.getLength());
        generator->cacheSequence (sequenceOffset);
    }
};


//==============================================================================
class OffsetMidiEventGenerator  : public MidiGenerator
{
public:
    OffsetMidiEventGenerator (std::unique_ptr<MidiGenerator> gen,
                              ClipBeatDuration offsetToUse)
        : generator (std::move (gen)),
          clipOffset (offsetToUse)
    {
    }

    void createMessagesForTime (MidiMessageArray& destBuffer,
                                EditBeatPosition editBeatPosition,
                                ActiveNoteList& noteList,
                                juce::Range<int> channelNumbers,
                                LiveClipLevel& clipLevel,
                                bool useMPEChannelMode, MidiMessageArray::MPESourceID midiSourceID,
                                juce::Array<juce::MidiMessage>& controllerMessagesScratchBuffer) override
    {
        generator->createMessagesForTime (destBuffer,
                                          editBeatPosition + clipOffset,
                                          noteList,
                                          channelNumbers,
                                          clipLevel,
                                          useMPEChannelMode, midiSourceID,
                                          controllerMessagesScratchBuffer);
    }

    ActiveNoteList getNotesOnAtTime (EditBeatPosition editBeatPosition, juce::Range<int> channelNumbers, LiveClipLevel& clipLevel) override
    {
        return generator->getNotesOnAtTime (editBeatPosition + clipOffset,
                                            channelNumbers,
                                            clipLevel);
    }

    void setTime (EditBeatPosition editBeatPosition) override
    {
        generator->setTime (editBeatPosition + clipOffset);
    }

    juce::MidiMessage getEvent() override
    {
        auto e = generator->getEvent();
        e.addToTimeStamp (-clipOffset);
        return e;
    }

    bool advance() override
    {
        return generator->advance();
    }

    bool exhausted() override
    {
        return generator->exhausted();
    }

private:
    //==============================================================================
    std::unique_ptr<MidiGenerator> generator;
    const ClipBeatDuration clipOffset;
};

//==============================================================================
//==============================================================================
class GeneratorAndNoteList
{
public:
    GeneratorAndNoteList (std::vector<juce::MidiMessageSequence> sequencesToUse,
                          BeatRange editRangeToUse,
                          BeatRange loopRangeToUse,
                          BeatDuration offsetToUse,
                          const QuantisationType& quantisation_,
                          const GrooveTemplate* groove_,
                          float grooveStrength_)
        : sequences (std::move (sequencesToUse)),
          editRange (editRangeToUse),
          loopRange (loopRangeToUse),
          offset (offsetToUse),
          quantisation (quantisation_),
          groove (groove_ != nullptr ? *groove_ : GrooveTemplate()),
          grooveStrength (grooveStrength_)
    {
    }

    void initialise (std::shared_ptr<ActiveNoteList> noteListToUse,
                     bool sendNoteOffs, size_t lastSequencesHash)
    {
        shouldSendNoteOffs = sendNoteOffs;
        shouldCreateMessagesForTime = shouldSendNoteOffs || noteListToUse == nullptr;
        activeNoteList = noteListToUse ? std::move (noteListToUse)
                                       : std::make_shared<ActiveNoteList>();

        const EditBeatRange clipRangeRaw { editRange.getStart().inBeats(), editRange.getEnd().inBeats() };
        const ClipBeatRange loopRangeRaw { loopRange.getStart().inBeats(), loopRange.getEnd().inBeats() };

        if (! loopRangeRaw.isEmpty())
            sequences = MidiHelpers::createLoopSection (std::move (sequences), loopRangeRaw);

        sequencesHash = std::hash<std::vector<juce::MidiMessageSequence>>{} (sequences);

        if (sequencesHash != lastSequencesHash)
            shouldSendNoteOffsForNotesNoLongerPlaying = true;

        auto cachingGenerator = std::make_unique<CachingMidiEventGenerator> (std::move (sequences),
                                                                             std::move (quantisation), std::move (groove), grooveStrength);
        auto loopedGenerator = std::make_unique<LoopedMidiEventGenerator> (std::move (cachingGenerator),
                                                                           activeNoteList, clipRangeRaw, loopRangeRaw);
        generator = std::make_unique<OffsetMidiEventGenerator> (std::move (loopedGenerator),
                                                                offset.inBeats());

        controllerMessagesScratchBuffer.ensureStorageAllocated (32);
    }

    const std::shared_ptr<ActiveNoteList>& getActiveNoteList() const
    {
        return activeNoteList;
    }

    void processSection (MidiMessageArray& destBuffer, choc::buffer::FrameCount numSamples,
                         BeatRange sectionEditBeatRange,
                         TimeRange sectionEditTimeRange,
                         LiveClipLevel& clipLevel,
                         juce::Range<int> channelNumbers,
                         bool useMPEChannelMode,
                         MidiMessageArray::MPESourceID midiSourceID,
                         bool isPlaying,
                         bool isContiguousWithPreviousBlock,
                         bool lastBlockOfLoop)
    {
        const auto secondsPerBeat = sectionEditTimeRange.getLength() / sectionEditBeatRange.getLength().inBeats();
        const auto blockStartBeatRelativeToClip = sectionEditBeatRange.getStart() - editRange.getStart();

        const auto volScale = clipLevel.getGain();
        const auto isLastBlockOfClip = sectionEditBeatRange.containsInclusive (editRange.getEnd());
        const double beatDurationOfOneSample = sectionEditBeatRange.getLength().inBeats() / numSamples;
        const auto timeDurationOfOneSample = sectionEditTimeRange.getLength() / numSamples;
        assert (timeDurationOfOneSample >= 10us);

        const auto clipIntersection = sectionEditBeatRange.getIntersectionWith (editRange);

        if (clipIntersection.isEmpty())
        {
            if (activeNoteList->areAnyNotesActive())
                MidiHelpers::createNoteOffs (*activeNoteList,
                                             destBuffer,
                                             midiSourceID,
                                             (sectionEditTimeRange.getLength() - timeDurationOfOneSample).inSeconds(),
                                             isPlaying);

            return;
        }

        if (shouldSendNoteOffs)
        {
            MidiHelpers::createNoteOffs (*activeNoteList,
                                         destBuffer,
                                         midiSourceID,
                                         (sectionEditTimeRange.getLength() - timeDurationOfOneSample).inSeconds(),
                                         isPlaying);
            shouldSendNoteOffs = false;
            shouldCreateMessagesForTime = true;
        }

        // This turns notes off that are no longer playing due to a change in the sequence
        // It is only called when the sequence changes
        if (shouldSendNoteOffsForNotesNoLongerPlaying)
        {
            // Find the currently playing notes from the sequence
            // Send note offs for any active notes, not in that sequence
            // This should also catch note numbers being moved
            const auto currentlyPlayingNoteList = generator->getNotesOnAtTime (clipIntersection.getStart().inBeats(), channelNumbers, clipLevel);

            if (activeNoteList->areAnyNotesActive())
                activeNoteList->iterate ([&] (auto chan, auto note)
                                         {
                                             if (currentlyPlayingNoteList.isNoteActive (chan, note))
                                                 return;

                                             destBuffer.addMidiMessage (juce::MidiMessage::noteOff (chan, note), 0.0, midiSourceID);
                                             activeNoteList->clearNote (chan, note);
                                         });

            shouldSendNoteOffsForNotesNoLongerPlaying = false;
        }

        if (! isContiguousWithPreviousBlock
            || blockStartBeatRelativeToClip <= 0.00001_bd
            || shouldCreateMessagesForTime)
        {
            generator->createMessagesForTime (destBuffer, sectionEditBeatRange.getStart().inBeats(),
                                              *activeNoteList,
                                              channelNumbers, clipLevel, useMPEChannelMode, midiSourceID,
                                              controllerMessagesScratchBuffer);
            shouldCreateMessagesForTime = false;

            // Ensure generator is initialised
            generator->setTime (sectionEditBeatRange.getStart().inBeats());
        }

        // Iterate notes in blocks
        {
            for (;;)
            {
                if (generator->exhausted())
                    break;

                auto e = generator->getEvent();
                const EditBeatPosition editBeatPosition = e.getTimeStamp();

                // Ensure we stop at the clip end
                if (editBeatPosition >= clipIntersection.getEnd().inBeats())
                    break;

                BlockBeatPosition blockBeatPosition = editBeatPosition - sectionEditBeatRange.getStart().inBeats();

                // This time correction is due to rounding errors accumulating and casuing events to be slightly negative in a block
                if (blockBeatPosition < -0.000001)
                {
                    generator->advance();
                    continue;
                }

                blockBeatPosition = std::max (blockBeatPosition, 0.0);

                // Note-offs that are on the end boundry need to be nudged back by 1 sample so they're not lost (which leads to stuck notes)
                if (e.isNoteOff() && juce::isWithin (editBeatPosition, sectionEditBeatRange.getEnd().inBeats(), beatDurationOfOneSample))
                    blockBeatPosition = blockBeatPosition - beatDurationOfOneSample;

                e.multiplyVelocity (volScale);
                const auto eventTimeSeconds = blockBeatPosition * secondsPerBeat.inSeconds();
                destBuffer.addMidiMessage (e, eventTimeSeconds, midiSourceID);

                // Update note list
                if (e.isNoteOn())
                    activeNoteList->startNote (e.getChannel(), e.getNoteNumber());
                else if (e.isNoteOff())
                    activeNoteList->clearNote (e.getChannel(), e.getNoteNumber());

                generator->advance();
            }
        }

        if (lastBlockOfLoop)
            MidiHelpers::createNoteOffs (*activeNoteList,
                                         destBuffer,
                                         midiSourceID,
                                         (sectionEditTimeRange.getLength() - timeDurationOfOneSample).inSeconds(),
                                         isPlaying);

        if (isLastBlockOfClip)
        {
            const auto endOfClipBeats = editRange.getEnd() - sectionEditBeatRange.getStart();

            // If the section ends right at the end of the clip, we need to nudge the note-offs back so they get played in this buffer
            auto eventTimeSeconds = (endOfClipBeats.inBeats() - beatDurationOfOneSample) * secondsPerBeat.inSeconds();

            MidiHelpers::createNoteOffs (*activeNoteList,
                                         destBuffer,
                                         midiSourceID,
                                         eventTimeSeconds,
                                         isPlaying);
        }
    }

    bool hasSameContentAs (const GeneratorAndNoteList& o) const
    {
        return editRange.getStart() == o.editRange.getStart()
                && loopRange == o.loopRange
                && offset == o.offset
                && quantisation == o.quantisation
                && groove == o.groove
                && grooveStrength == o.grooveStrength;
    }

    size_t getSequencesHash() const
    {
        return sequencesHash;
    }

private:
    std::shared_ptr<ActiveNoteList> activeNoteList;
    std::unique_ptr<MidiGenerator> generator;

    std::vector<juce::MidiMessageSequence> sequences;
    size_t sequencesHash = 0;
    const BeatRange editRange, loopRange;
    const BeatDuration offset;
    QuantisationType quantisation;
    GrooveTemplate groove;
    float grooveStrength = 0.0f;

    bool shouldCreateMessagesForTime = false, shouldSendNoteOffs = false, shouldSendNoteOffsForNotesNoLongerPlaying = false;
    juce::Array<juce::MidiMessage> controllerMessagesScratchBuffer;
};


//==============================================================================
//==============================================================================
LoopingMidiNode::LoopingMidiNode (std::vector<juce::MidiMessageSequence> sequences,
                                  juce::Range<int> midiChannelNumbers,
                                  bool useMPE,
                                  BeatRange editTimeRange,
                                  BeatRange sequenceLoopRange,
                                  BeatDuration sequenceOffset,
                                  LiveClipLevel liveClipLevel,
                                  ProcessState& processStateToUse,
                                  EditItemID editItemIDToUse,
                                  const QuantisationType& quantisation,
                                  const GrooveTemplate* groove,
                                  float grooveStrength,
                                  std::function<bool()> shouldBeMuted)
    : TracktionEngineNode (processStateToUse),
      channelNumbers (midiChannelNumbers),
      useMPEChannelMode (useMPE),
      editRange (editTimeRange),
      clipLevel (liveClipLevel),
      editItemID (editItemIDToUse),
      shouldBeMutedDelegate (std::move (shouldBeMuted)),
      wasMute (liveClipLevel.isMute())
{
    jassert (! sequences.empty());
    jassert (channelNumbers.getStart() > 0 && channelNumbers.getEnd() <= 16);

    // Create this now but don't initialise it until we know if we have to
    // steal an old node's ActiveNoteList, this happens in prepareToPlay
    generatorAndNoteList = std::make_unique<GeneratorAndNoteList> (std::move (sequences),
                                                                   editTimeRange,
                                                                   sequenceLoopRange,
                                                                   sequenceOffset,
                                                                   quantisation,
                                                                   groove,
                                                                   grooveStrength);
}

const std::shared_ptr<ActiveNoteList>& LoopingMidiNode::getActiveNoteList() const
{
    return generatorAndNoteList->getActiveNoteList();
}

tracktion::graph::NodeProperties LoopingMidiNode::getNodeProperties()
{
    tracktion::graph::NodeProperties props;
    props.hasMidi = true;
    props.nodeID = (size_t) editItemID.getRawID();
    return props;
}

void LoopingMidiNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    std::shared_ptr<ActiveNoteList> activeNoteList;
    bool sendNoteOffEvents = false;
    size_t lastSequencesHash = 0;

    if (auto oldNode = findNodeWithIDIfNonZero<LoopingMidiNode> (info.nodeGraphToReplace, getNodeProperties().nodeID))
    {
        midiSourceID = oldNode->midiSourceID;
        activeNoteList = oldNode->generatorAndNoteList->getActiveNoteList();
        sendNoteOffEvents = ! generatorAndNoteList->hasSameContentAs (*oldNode->generatorAndNoteList);
        lastSequencesHash = oldNode->generatorAndNoteList->getSequencesHash();
    }

    generatorAndNoteList->initialise (activeNoteList, sendNoteOffEvents, lastSequencesHash);
}

bool LoopingMidiNode::isReadyToProcess()
{
    return true;
}

void LoopingMidiNode::process (ProcessContext& pc)
{
    SCOPED_REALTIME_CHECK
    const auto timelineRange = getTimelineSampleRange();

    if (timelineRange.isEmpty())
        return;

    if (shouldBeMutedDelegate && shouldBeMutedDelegate())
        return;

    const auto isPlaying = getPlayHead().isPlaying();

    if (const bool mute = clipLevel.isMute(); mute)
    {
        if (mute != wasMute)
        {
            wasMute = mute;
            MidiHelpers::createNoteOffs (*generatorAndNoteList->getActiveNoteList(),
                                         pc.buffers.midi,
                                         midiSourceID,
                                         (getEditTimeRange().getLength() - 10us).inSeconds(),
                                         isPlaying);
        }

        return;
    }

    generatorAndNoteList->processSection (pc.buffers.midi, pc.numSamples,
                                          getEditBeatRange(),
                                          getEditTimeRange(),
                                          clipLevel,
                                          channelNumbers,
                                          useMPEChannelMode,
                                          midiSourceID,
                                          isPlaying,
                                          getPlayHeadState().isContiguousWithPreviousBlock(),
                                          getPlayHeadState().isLastBlockOfLoop());
}

}} // namespace tracktion { inline namespace engine
