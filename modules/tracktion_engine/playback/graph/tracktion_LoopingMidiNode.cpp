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

//==============================================================================
//==============================================================================
namespace MidiHelpers
{
    /** Snips out a section of a sequence adding note on/off events for notes at the loop boundries. */
    juce::MidiMessageSequence createLoopSection (juce::MidiMessageSequence sourceSequence,
                                                 juce::Range<double> loopRange)
    {
        juce::MidiMessageSequence res;

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
    std::vector<juce::MidiMessageSequence> createLoopSection (const std::vector<juce::MidiMessageSequence>& sourceSequences,
                                                              juce::Range<double> loopRange)
    {
        std::vector<juce::MidiMessageSequence> res;

        for (auto& seq : sourceSequences)
            res.push_back (createLoopSection (seq, loopRange));

        return res;
    }

    void applyQuantisationToSequence (const QuantisationType& q, juce::MidiMessageSequence& ms, bool canQuantiseNoteOffs)
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

    void applyGrooveToSequence (const GrooveTemplate& groove, float grooveStrength, juce::MidiMessageSequence& ms)
    {
        for (auto mh : ms)
        {
            auto& m = mh->message;

            if (m.isNoteOn() || m.isNoteOff())
                m.setTimeStamp (groove.beatsTimeToGroovyTime (BeatPosition::fromBeats (m.getTimeStamp()), grooveStrength).inBeats());
        }
    }

    void createNoteOffs (ActiveNoteList& activeNoteList,
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

    virtual void createNoteOffs (MidiMessageArray&,
                                 MidiMessageArray::MPESourceID,
                                 BlockBeatDuration /*midiTimeOffset*/, bool /*isPlaying*/) {}
    virtual void nextSequence() {}
    virtual void visitEventsInRange (juce::Range<double>,
                                     const std::function<bool (const juce::MidiMessage&)>& visitEventsCallback,
                                     const std::function<void (ActiveNoteList&, BlockBeatPosition timeInBeats)>& createNoteOffsCallback) {}

    //==============================================================================
    virtual void cacheSequence (double /*offset*/) {}

    virtual void setTime (double) = 0;
    virtual bool advance() = 0;

    virtual bool exhausted() = 0;
    virtual juce::MidiMessage getEvent() = 0;

    double getTimeStamp() { return getEvent().getTimeStamp(); }
};

//==============================================================================
struct MidiEventGenerator : public MidiGenerator
{
    MidiEventGenerator (std::vector<juce::MidiMessageSequence> seq)
        : sequences (std::move (seq))
    {
        for (auto& sequence : sequences)
            sequence.updateMatchedPairs();
    }

    void createMessagesForTime (MidiMessageArray& destBuffer,
                                SequenceBeatPosition time,
                                ActiveNoteList& activeNoteList,
                                juce::Range<int> channelNumbers,
                                LiveClipLevel& clipLevel,
                                bool useMPEChannelMode, MidiMessageArray::MPESourceID midiSourceID,
                                juce::Array<juce::MidiMessage>& controllerMessagesScratchBuffer) override
    {
        auto& sequence = sequences[currentSequence];
        thread_local MidiMessageArray scratchBuffer;
        scratchBuffer.clear();

        MidiNodeHelpers::createMessagesForTime (scratchBuffer,
                                                sequence,
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

    void nextSequence() override
    {
        if (sequences.size() > 0)
            if (++currentSequence >= sequences.size())
                currentSequence = 0;
    }

    void visitEventsInRange (SequenceBeatRange timeRange,
                             const std::function<bool (const juce::MidiMessage&)>& visitor,
                             const std::function<void (ActiveNoteList&, double timeInBeats)>&) override
    {
        auto& sequence = sequences[currentSequence];
        auto numEvents = sequence.getNumEvents();

        // Set the index to the start of the range
        if (numEvents != 0)
        {
            currentIndex = juce::jlimit (0, numEvents - 1, currentIndex);

            if (sequence.getEventTime (currentIndex) >= timeRange.getStart())
            {
                while (currentIndex > 0 && sequence.getEventTime (currentIndex - 1) >= timeRange.getStart())
                    --currentIndex;
            }
            else
            {
                while (currentIndex < numEvents && sequence.getEventTime (currentIndex) < timeRange.getStart())
                    ++currentIndex;
            }
        }

        // Then visit each note within the range
        for (;;)
        {
            if (auto meh = sequence.getEventPointer (currentIndex++))
            {
                auto eventTime = meh->message.getTimeStamp();

                // We want to include note-offs that are on the beat boundry but not note-ons
                if (meh->message.isNoteOff())
                {
                    if (eventTime > timeRange.getEnd())
                        break;
                }
                else if (eventTime >= timeRange.getEnd())
                {
                    break;
                }

                visitor ({ meh->message, eventTime });
            }
            else
            {
                break;
            }
        }
    }

    void setTime (SequenceBeatPosition pos) override
    {
        auto& sequence = sequences[currentSequence];
        auto numEvents = sequence.getNumEvents();

        // Set the index to the start of the range
        if (numEvents != 0)
        {
            currentIndex = juce::jlimit (0, numEvents - 1, currentIndex);

            if (sequence.getEventTime (currentIndex) >= pos)
            {
                while (currentIndex > 0 && sequence.getEventTime (currentIndex - 1) >= pos)
                    --currentIndex;
            }
            else
            {
                while (currentIndex < numEvents && sequence.getEventTime (currentIndex) < pos)
                    ++currentIndex;
            }
        }
    }

    juce::MidiMessage getEvent() override
    {
        auto& sequence = sequences[currentSequence];
        [[ maybe_unused ]] auto numEvents = sequence.getNumEvents();
        jassert (currentIndex < numEvents);

        return sequence.getEventPointer (currentIndex)->message;
    }

    bool advance() override
    {
        ++currentIndex;
        return ! exhausted();
    }

    bool exhausted() override
    {
        return currentIndex >= sequences[currentSequence].getNumEvents();
    }

    std::vector<juce::MidiMessageSequence> sequences;
    int currentIndex = -1;
    size_t currentSequence = 0;
};

//==============================================================================
struct SequenceEventGenerator   : public MidiGenerator
{
    SequenceEventGenerator (juce::MidiMessageSequence& seq)
        : sequence (seq)
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

        MidiNodeHelpers::createMessagesForTime (scratchBuffer,
                                                sequence,
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

    void setTime (SequenceBeatPosition pos) override
    {
        auto numEvents = sequence.getNumEvents();

        // Set the index to the start of the range
        if (numEvents != 0)
        {
            currentIndex = juce::jlimit (0, numEvents - 1, currentIndex);

            if (sequence.getEventTime (currentIndex) >= pos)
            {
                while (currentIndex > 0 && sequence.getEventTime (currentIndex - 1) >= pos)
                    --currentIndex;
            }
            else
            {
                while (currentIndex < numEvents && sequence.getEventTime (currentIndex) < pos)
                    ++currentIndex;
            }
        }
    }

    juce::MidiMessage getEvent() override
    {
        [[ maybe_unused ]] auto numEvents = sequence.getNumEvents();
        jassert (currentIndex >= 0);
        jassert (currentIndex < numEvents);

        return sequence.getEventPointer (currentIndex)->message;
    }

    bool advance() override
    {
        ++currentIndex;
        return ! exhausted();
    }

    bool exhausted() override
    {
        return currentIndex >= sequence.getNumEvents();
    }

    juce::MidiMessageSequence& sequence;
    int currentIndex = -1;
};


//==============================================================================
//==============================================================================
class CachingMidiEventGenerator : public MidiGenerator
{
public:
    CachingMidiEventGenerator (std::vector<juce::MidiMessageSequence> seq,
                               QuantisationType qt,
                               const GrooveTemplate* grooveTemplate, float grooveStrength_)
        : sequences (std::move (seq)),
          quantisation (std::move (qt)),
          groove (grooveTemplate),
          grooveStrength (grooveStrength_)
    {
        // Cache the sequence at 0.0 time to reserve the required storage
        cacheSequence (0.0);
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

        currentSequence.clear();
        currentSequence.addSequence (sequences[currentSequenceIndex], offsetBeats);
        currentSequence.updateMatchedPairs();

        // For some reason, isQuantisingNoteOffs is ignored by the existing MidiList::getPlaybackBeats
        // to maintain compatibility we'll have to do the same
        MidiHelpers::applyQuantisationToSequence (quantisation, currentSequence, false);

        if (groove)
            MidiHelpers::applyGrooveToSequence (*groove, grooveStrength, currentSequence);

        currentSequence.updateMatchedPairs();
        currentSequence.sort();

        cachedSequenceOffset = offsetBeats;
    }

    void nextSequence() override
    {
        jassertfalse;
    }

    juce::MidiMessage getEvent() override
    {
        return { generator.getEvent(), generator.getTimeStamp() - cachedSequenceOffset };
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
    juce::MidiMessageSequence currentSequence;
    SequenceEventGenerator generator { currentSequence };

    const QuantisationType quantisation;
    const GrooveTemplate* groove = nullptr;
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
                                          editBeatPosition,
                                          noteList,
                                          channelNumbers,
                                          clipLevel,
                                          useMPEChannelMode, midiSourceID,
                                          controllerMessagesScratchBuffer);
    }

    void nextSequence() override
    {
        generator->nextSequence();
    }

    void visitEventsInRange (EditBeatRange editTimeRange,
                             const std::function<bool (const juce::MidiMessage&)>& visitor,
                             const std::function<void (ActiveNoteList&, BlockBeatPosition timeInBeats)>& createNoteOffsCallback) override
    {
        double timeAdjustment = 0.0;
        auto visitEvent = [&] (const juce::MidiMessage& e)
                          {
                              const EditBeatPosition editBeatPosition = e.getTimeStamp() + timeAdjustment;
                              return visitor (juce::MidiMessage (e, editBeatPosition));
                          };

        ClipBeatRange clipBeatRange = editTimeRange - clipRange.getStart();
        const auto splitRange = rangeToSplitRange (clipBeatRange, loopTimes);
        timeAdjustment = clipRange.getStart() + splitRange.offset1;
        generator->visitEventsInRange (splitRange.range1, visitEvent, createNoteOffsCallback);

        if (splitRange.isSplit)
        {
            generator->nextSequence();

            // Start any notes that cross the loop boundry
            const auto editBeatPosition = clipRange.getStart() + splitRange.offset2;

            // Visit events in the block
            timeAdjustment = editBeatPosition;
            generator->visitEventsInRange (splitRange.range2, visitEvent, createNoteOffsCallback);
        }
    }

    void setTime (EditBeatPosition editBeatPosition) override
    {
        const ClipBeatPosition clipPos = editBeatPosition - clipRange.getStart();
        const SequenceBeatPosition sequencePos = loopTimes.getStart() + std::fmod (clipPos, loopTimes.getLength());

        setLoopIndex (static_cast<int> (clipPos / loopTimes.getLength()));
        generator->setTime (sequencePos);
    }

    juce::MidiMessage getEvent() override
    {
        const auto offsetBeats = clipRange.getStart() - loopTimes.getStart() + (loopIndex * loopTimes.getLength());
        return { generator->getEvent(), generator->getTimeStamp() + offsetBeats };
    }

    bool advance() override
    {
        generator->advance();

        if (exhausted())
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

    void setLoopIndex (int newLoopIndex)
    {
        if (newLoopIndex == loopIndex)
            return;

        loopIndex = newLoopIndex;
        const auto sequenceOffset = clipRange.getStart() + (loopIndex * loopTimes.getLength());
        generator->cacheSequence (sequenceOffset);
    }

    //==============================================================================
    struct SplitRange
    {
        SplitRange (juce::Range<double> r1, double o1)
            : range1 (r1), offset1 (o1) {}

        SplitRange (juce::Range<double> r1, double o1,
                    juce::Range<double> r2, double o2)
            : range1 (r1), range2 (r2),
              offset1 (o1), offset2 (o2), isSplit (true) {}

        ClipBeatRange range1, range2;
        ClipBeatDuration offset1 = 0.0, offset2 = 0.0;
        bool isSplit = false;
    };

    static SplitRange rangeToSplitRange (ClipBeatRange unloopedRange, ClipBeatRange loopRange)
    {
        if (loopRange.isEmpty())
            return { unloopedRange, 0.0 };

        const auto start = unloopedRange.getStart();
        const auto end = unloopedRange.getEnd();

        const auto s = linearPositionToLoopPosition (start, loopRange);
        const auto e = linearPositionToLoopPosition (end, loopRange);

        const auto offset1 = start - s;

        if (s > e)
        {
            const auto offset2 = start - loopRange.getStart();

            return { { s, loopRange.getEnd() }, offset1,
                     { loopRange.getStart(), e }, offset2 };
        }

        return { { s, e}, offset1 };
    }

    static ClipBeatPosition linearPositionToLoopPosition (ClipBeatPosition position, ClipBeatRange loopRange)
    {
        return loopRange.getStart() + std::fmod (position, loopRange.getLength());
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

    void nextSequence() override
    {
        generator->nextSequence();
    }

    void visitEventsInRange (EditBeatRange editTimeRange,
                             const std::function<bool (const juce::MidiMessage&)>& visitor,
                             const std::function<void (ActiveNoteList&, BlockBeatPosition timeInBeats)>& createNoteOffsCallback) override
    {
        editTimeRange = editTimeRange + clipOffset;

        auto visitEvent = [&] (const juce::MidiMessage& e)
                          {
                              const EditBeatPosition editBeatPosition = e.getTimeStamp() - clipOffset;
                              return visitor (juce::MidiMessage (e, editBeatPosition));
                          };

        generator->visitEventsInRange (editTimeRange, visitEvent, createNoteOffsCallback);
    }

    void setTime (EditBeatPosition editBeatPosition) override
    {
        generator->setTime (editBeatPosition + clipOffset);
    }

    juce::MidiMessage getEvent() override
    {
        return { generator->getEvent(), generator->getTimeStamp() - clipOffset };
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
class ActiveNoteGenerator : public MidiGenerator
{
public:
    ActiveNoteGenerator (std::unique_ptr<MidiGenerator> input,
                         std::shared_ptr<ActiveNoteList> anl)
        : generator (std::move (input)),
          activeNoteList (std::move (anl))
    {
        assert (activeNoteList);
    }

    void createMessagesForTime (MidiMessageArray& destBuffer,
                                EditBeatPosition time,
                                ActiveNoteList& noteList,
                                juce::Range<int> channelNumbers,
                                LiveClipLevel& clipLevel,
                                bool useMPEChannelMode, MidiMessageArray::MPESourceID midiSourceID,
                                juce::Array<juce::MidiMessage>& controllerMessagesScratchBuffer) override
    {
        generator->createMessagesForTime (destBuffer,
                                          time,
                                          noteList,
                                          channelNumbers,
                                          clipLevel,
                                          useMPEChannelMode, midiSourceID,
                                          controllerMessagesScratchBuffer);
    }

    void nextSequence() override
    {
        generator->nextSequence();
    }

    void createNoteOffs (MidiMessageArray& destination,
                         MidiMessageArray::MPESourceID midiSourceID,
                         BlockBeatPosition midiTimeOffset, bool isPlaying) override
    {
        MidiHelpers::createNoteOffs (*activeNoteList,
                                     destination,
                                     midiSourceID,
                                     midiTimeOffset, isPlaying);
    }

    void visitEventsInRange (EditBeatRange editBeatRange,
                             const std::function<bool (const juce::MidiMessage&)>& visitor,
                             const std::function<void (ActiveNoteList&, BlockBeatPosition timeInBeats)>& createNoteOffsCallback) override
    {
        auto visitEvents = [&, this] (const auto& e)
        {
            if (e.isNoteOn())
                activeNoteList->startNote (e.getChannel(), e.getNoteNumber());
            else if (e.isNoteOff())
                activeNoteList->clearNote (e.getChannel(), e.getNoteNumber());

            return visitor (e);
        };

        generator->visitEventsInRange (editBeatRange, visitEvents, createNoteOffsCallback);
    }

    void setTime (EditBeatPosition editBeatPosition) override
    {
        generator->setTime (editBeatPosition);
    }

    juce::MidiMessage getEvent() override
    {
        return { generator->getEvent(), generator->getTimeStamp() };
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
    std::unique_ptr<MidiGenerator> generator;
    std::shared_ptr<ActiveNoteList> activeNoteList;
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
      loopRange (sequenceLoopRange),
      offset (sequenceOffset),
      clipLevel (liveClipLevel),
      editItemID (editItemIDToUse),
      shouldBeMutedDelegate (std::move (shouldBeMuted)),
      wasMute (liveClipLevel.isMute())
{
    jassert (channelNumbers.getStart() > 0 && channelNumbers.getEnd() <= 16);

    activeNoteList = std::make_shared<ActiveNoteList>();

    const EditBeatRange clipRangeRaw { editRange.getStart().inBeats(), editRange.getEnd().inBeats() };
    const ClipBeatRange loopRangeRaw { sequenceLoopRange.getStart().inBeats(), sequenceLoopRange.getEnd().inBeats() };

    sequences = MidiHelpers::createLoopSection (sequences, loopRangeRaw);

    auto cachingGenerator = std::make_unique<CachingMidiEventGenerator> (std::move (sequences), quantisation, groove, grooveStrength);
    auto loopedGenerator = std::make_unique<LoopedMidiEventGenerator> (std::move (cachingGenerator), activeNoteList, clipRangeRaw, loopRangeRaw);
    auto offsetGenerator = std::make_unique<OffsetMidiEventGenerator> (std::move (loopedGenerator), sequenceOffset.inBeats());
    generator = std::make_unique<ActiveNoteGenerator> (std::move (offsetGenerator), activeNoteList);

    controllerMessagesScratchBuffer.ensureStorageAllocated (32);
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
    if (info.rootNodeToReplace != nullptr)
    {
        bool foundNodeToReplace = false;
        const auto nodeIDToLookFor = getNodeProperties().nodeID;

        visitNodes (*info.rootNodeToReplace, [&] (Node& n)
                    {
                        if (auto midiNode = dynamic_cast<LoopingMidiNode*> (&n))
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
            generator->createNoteOffs (pc.buffers.midi, midiSourceID, 0.0, isPlaying);
        }

        return;
    }

    const auto sectionEditBeatRange = getEditBeatRange();
    const auto secondsPerBeat = getEditTimeRange().getLength() / sectionEditBeatRange.getLength().inBeats();
    const auto blockStartBeatRelativeToClip = sectionEditBeatRange.getStart() - editRange.getStart();

    auto volScale = clipLevel.getGain();
    const auto lastBlockOfLoop = getPlayHeadState().isLastBlockOfLoop();
    const auto isLastBlockOfClip = sectionEditBeatRange.containsInclusive (editRange.getEnd());
    const double beatDurationOfOneSample = sectionEditBeatRange.getLength().inBeats() / pc.numSamples;

    const auto clipIntersection = sectionEditBeatRange.getIntersectionWith (editRange);

    if (clipIntersection.isEmpty())
        return;

    if (! getPlayHeadState().isContiguousWithPreviousBlock()
        || blockStartBeatRelativeToClip <= 0.00001_bd
        || shouldCreateMessagesForTime)
    {
        generator->createMessagesForTime (pc.buffers.midi, sectionEditBeatRange.getStart().inBeats(),
                                          *activeNoteList,
                                          channelNumbers, clipLevel, useMPEChannelMode, midiSourceID,
                                          controllerMessagesScratchBuffer);
        shouldCreateMessagesForTime = false;
    }

    // Ensure generator is initialised
    generator->setTime (sectionEditBeatRange.getStart().inBeats());

    // Iterate notes in blocks
    {
        for (;;)
        {
            if (generator->exhausted())
                break;

            auto e = generator->getEvent();
            const EditBeatPosition editBeatPosition = e.getTimeStamp();

            if (editBeatPosition >= sectionEditBeatRange.getEnd().inBeats())
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
            pc.buffers.midi.addMidiMessage (e, eventTimeSeconds, midiSourceID);

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
                                     pc.buffers.midi,
                                     midiSourceID,
                                     (getEditTimeRange().getLength() - 10us).inSeconds(),
                                     isPlaying);

    if (isLastBlockOfClip)
    {
        const auto endOfClipBeats = editRange.getEnd() - sectionEditBeatRange.getStart();
        const auto eventTimeSeconds = endOfClipBeats.inBeats() * secondsPerBeat.inSeconds();
        MidiHelpers::createNoteOffs (*activeNoteList,
                                     pc.buffers.midi,
                                     midiSourceID,
                                     eventTimeSeconds,
                                     isPlaying);
    }
}

}} // namespace tracktion { inline namespace engine
