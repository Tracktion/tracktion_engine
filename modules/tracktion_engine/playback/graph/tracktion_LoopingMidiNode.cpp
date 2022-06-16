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

    //==============================================================================
    virtual void cacheSequence (double /*offset*/) {}

    virtual void setTime (double) = 0;
    virtual bool advance() = 0;

    virtual bool exhausted() = 0;
    virtual juce::MidiMessage getEvent() = 0;
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
                               const GrooveTemplate& grooveTemplate, float grooveStrength_)
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

        if (! groove.isEmpty())
            MidiHelpers::applyGrooveToSequence (groove, grooveStrength, currentSequence);

        currentSequence.updateMatchedPairs();
        currentSequence.sort();

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
    juce::MidiMessageSequence currentSequence;
    SequenceEventGenerator generator { currentSequence };

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
                                          editBeatPosition,
                                          noteList,
                                          channelNumbers,
                                          clipLevel,
                                          useMPEChannelMode, midiSourceID,
                                          controllerMessagesScratchBuffer);
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

        auto e = generator->getEvent();
        e.addToTimeStamp (offsetBeats);
        return e;
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
                     bool sendNoteOffs)
    {
        shouldSendNoteOffs = sendNoteOffs;
        shouldCreateMessagesForTime = shouldSendNoteOffs || noteListToUse == nullptr;
        activeNoteList = noteListToUse ? std::move (noteListToUse)
                                       : std::make_shared<ActiveNoteList>();

        const EditBeatRange clipRangeRaw { editRange.getStart().inBeats(), editRange.getEnd().inBeats() };
        const ClipBeatRange loopRangeRaw { loopRange.getStart().inBeats(), loopRange.getEnd().inBeats() };

        sequences = MidiHelpers::createLoopSection (std::move (sequences), loopRangeRaw);

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

        const auto clipIntersection = sectionEditBeatRange.getIntersectionWith (editRange);

        if (clipIntersection.isEmpty())
            return;

        if (shouldSendNoteOffs)
        {
            DBG("SENDING NOTE OFFS");
            MidiHelpers::createNoteOffs (*activeNoteList,
                                         destBuffer,
                                         midiSourceID,
                                         (sectionEditTimeRange.getLength() - 10us).inSeconds(),
                                         isPlaying);
            shouldSendNoteOffs = false;
            shouldCreateMessagesForTime = true;
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
                                         (sectionEditTimeRange.getLength() - 10us).inSeconds(),
                                         isPlaying);

        if (isLastBlockOfClip)
        {
            const auto endOfClipBeats = editRange.getEnd() - sectionEditBeatRange.getStart();
            const auto eventTimeSeconds = endOfClipBeats.inBeats() * secondsPerBeat.inSeconds();
            MidiHelpers::createNoteOffs (*activeNoteList,
                                         destBuffer,
                                         midiSourceID,
                                         eventTimeSeconds,
                                         isPlaying);
        }
    }

    bool hasSameContentAs (const GeneratorAndNoteList& o) const
    {
        return editRange == o.editRange
                && loopRange == o.loopRange
                && offset == o.offset
                && quantisation == o.quantisation
                && groove == o.groove
                && grooveStrength == o.grooveStrength;
    }

private:
    std::shared_ptr<ActiveNoteList> activeNoteList;
    std::unique_ptr<MidiGenerator> generator;

    std::vector<juce::MidiMessageSequence> sequences;
    const BeatRange editRange, loopRange;
    const BeatDuration offset;
    QuantisationType quantisation;
    GrooveTemplate groove;
    float grooveStrength = 0.0f;

    bool shouldCreateMessagesForTime = false, shouldSendNoteOffs = false;
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

    if (info.rootNodeToReplace != nullptr)
    {
        const auto nodeIDToLookFor = getNodeProperties().nodeID;

        visitNodes (*info.rootNodeToReplace, [&] (Node& n)
                    {
                        LoopingMidiNode* foundMidiNode = nullptr;

                        if (auto midiNode = dynamic_cast<LoopingMidiNode*> (&n))
                        {
                            if (midiNode->getNodeProperties().nodeID == nodeIDToLookFor)
                                foundMidiNode = midiNode;
                        }
                        else if (auto combiningNode = dynamic_cast<CombiningNode*> (&n))
                        {
                            for (auto internalNode : combiningNode->getInternalNodes())
                                if (auto internalMidiNode = dynamic_cast<LoopingMidiNode*> (internalNode))
                                    if (internalMidiNode->getNodeProperties().nodeID == nodeIDToLookFor)
                                        foundMidiNode = internalMidiNode;
                        }

                        if (foundMidiNode != nullptr)
                        {
                            midiSourceID = foundMidiNode->midiSourceID;
                            activeNoteList = foundMidiNode->generatorAndNoteList->getActiveNoteList();
                            sendNoteOffEvents = ! generatorAndNoteList->hasSameContentAs (*foundMidiNode->generatorAndNoteList);
                        }
                    }, true);
    }

    generatorAndNoteList->initialise (activeNoteList, sendNoteOffEvents);
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
