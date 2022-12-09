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

namespace
{
    template<typename VarType>
    inline void convertPropertyToType (juce::ValueTree& v, const juce::Identifier& id)
    {
        if (const auto* prop = v.getPropertyPointer (id))
            if (prop->isString())
                (*const_cast<juce::var*> (prop)) = static_cast<VarType> (*prop);
    }

    void convertMidiPropertiesFromStrings (juce::ValueTree& midi)
    {
        if (midi.hasType (IDs::NOTE))
        {
            convertPropertyToType<int>      (midi, IDs::p);
            convertPropertyToType<double>   (midi, IDs::b);
            convertPropertyToType<double>   (midi, IDs::l);
            convertPropertyToType<int>      (midi, IDs::v);
            convertPropertyToType<int>      (midi, IDs::c);
        }
        else if (midi.hasType (IDs::CONTROL))
        {
            convertPropertyToType<double>   (midi, IDs::b);
            convertPropertyToType<int>      (midi, IDs::type);
            convertPropertyToType<int>      (midi, IDs::val);
            convertPropertyToType<int>      (midi, IDs::metadata);
        }
        else if (MidiExpression::isExpression (midi.getType()))
        {
            convertPropertyToType<double>   (midi, IDs::b);
            convertPropertyToType<double>   (midi, IDs::v);
        }

        for (auto v : midi)
            convertMidiPropertiesFromStrings (v);
    }
}

template <typename Type>
static void removeMidiEventFromSelection (Type* event)
{
    for (SelectionManager::Iterator sm; sm.next();)
        if (auto sme = sm->getFirstItemOfType<SelectedMidiEvents>())
            sme->removeSelectedEvent (event);
}

//==============================================================================
/**
    Creates a MidiList with NoteExpression from a stream of MPE MIDI messages.

    N.B.    - We have to process the messages using the MPE interface as the zone may change
            - Notes may be stopped if they overlap with existing notes
            - MPENotes have no concept of time so we store this for each note
*/
class MPEtoNoteExpression   : private juce::MPEInstrument::Listener
{
public:
    MPEtoNoteExpression (MidiList& o, const TempoSequence* ts, juce::MPEZoneLayout layout, BeatPosition editBeatOfListTimeZero, juce::UndoManager* um)
        : list (o), tempoSequence (ts), firstBeatNum (editBeatOfListTimeZero), undoManager (um)
    {
        mpeInstrument.setZoneLayout (layout);
        mpeInstrument.addListener (this);
    }

    void processMidiMessage (const juce::MidiMessage& message)
    {
        currentEventBeat = tempoSequence != nullptr ? (tempoSequence->toBeats (TimePosition::fromSeconds (message.getTimeStamp())) - toDuration (firstBeatNum))
                                                    : BeatPosition::fromBeats (message.getTimeStamp());

        mpeInstrument.processNextMidiEvent (message);
    }

private:
    MidiList& list;
    const TempoSequence* tempoSequence;
    const BeatPosition firstBeatNum;
    juce::UndoManager* undoManager;
    juce::MPEInstrument mpeInstrument;

    struct ActiveNote
    {
        ActiveNote (juce::MPENote n, BeatPosition beat)
            : startNote (n), startBeat (beat)
        {
            modulations.ensureStorageAllocated (50);
        }

        enum ChangeType
        {
            pitchbend,
            pressure,
            timbre
        };

        struct Modulation
        {
            BeatPosition beat;
            float value;
            ActiveNote::ChangeType type;
        };

        bool containsModulation (ActiveNote::ChangeType t)
        {
            for (const auto& mod : modulations)
                if (mod.type == t)
                    return true;

            return false;
        }

        juce::MPENote startNote;
        BeatPosition startBeat;
        juce::Array<Modulation> modulations;
    };

    juce::OwnedArray<ActiveNote> activeNotes;
    BeatPosition currentEventBeat;

    ActiveNote* getActiveNote (juce::MPENote note)
    {
        for (auto activeNote : activeNotes)
            if (activeNote->startNote.noteID == note.noteID)
                return activeNote;

        return {};
    }

    void addNoteToList (juce::MPENote note, BeatPosition endBeat)
    {
        if (auto an = getActiveNote (note))
        {
            const auto startBeat = an->startBeat;
            auto n = list.addNote (an->startNote.initialNote, startBeat, toDuration (endBeat - toDuration (startBeat)),
                                   note.noteOnVelocity.as7BitInt(), 0, undoManager);
            auto noteState = n->state;

            auto lift = note.noteOffVelocity.as7BitInt();
            auto timb = an->startNote.timbre.asUnsignedFloat();
            auto pres = an->startNote.pressure.asUnsignedFloat();
            auto bend = (float) an->startNote.totalPitchbendInSemitones;

            if (lift != 0)
                noteState.setProperty (IDs::lift, lift, undoManager);

            if (timb != MidiList::defaultInitialTimbreValue)
                noteState.setProperty (IDs::timb, timb, undoManager);

            if (pres != MidiList::defaultInitialPressureValue)
                noteState.setProperty (IDs::pres, pres, undoManager);

            if (bend != MidiList::defaultInitialPitchBendValue)
                noteState.setProperty (IDs::bend, bend, undoManager);

            // MPE spec requires a timbre before a note-on (though we send all dimensions, just to be sure)
            // and pressure & pitchbend immediately following.
            MidiExpression::createAndAddExpressionToNote (noteState, IDs::PRESSURE, BeatPosition(), pres, undoManager);
            MidiExpression::createAndAddExpressionToNote (noteState, IDs::PITCHBEND, BeatPosition(), bend, undoManager);

            for (auto& mod : an->modulations)
            {
                auto getType = [] (ActiveNote::ChangeType type)
                {
                    switch (type)
                    {
                        case ActiveNote::pitchbend: return IDs::PITCHBEND;
                        case ActiveNote::pressure:  return IDs::PRESSURE;
                        case ActiveNote::timbre:    return IDs::TIMBRE;
                    }

                    jassertfalse;
                    return juce::Identifier();
                };

                const auto relativeBeat = mod.beat - startBeat;

                MidiExpression::createAndAddExpressionToNote (noteState, getType (mod.type), toPosition (relativeBeat), mod.value, undoManager);
            }

            activeNotes.removeObject (an);
        }
    }

    void noteAdded (juce::MPENote note) override
    {
        activeNotes.add (new ActiveNote (note, currentEventBeat));
        jassert (activeNotes.size() <= 16);
    }

    void notePressureChanged (juce::MPENote note) override
    {
        if (auto an = getActiveNote (note))
            an->modulations.add (ActiveNote::Modulation { currentEventBeat, note.pressure.asUnsignedFloat(), ActiveNote::pressure });
    }

    void notePitchbendChanged (juce::MPENote note) override
    {
        if (auto an = getActiveNote (note))
            an->modulations.add (ActiveNote::Modulation { currentEventBeat, float (note.totalPitchbendInSemitones), ActiveNote::pitchbend });
    }

    void noteTimbreChanged (juce::MPENote note) override
    {
        if (auto an = getActiveNote (note))
            an->modulations.add (ActiveNote::Modulation { currentEventBeat, note.timbre.asUnsignedFloat(), ActiveNote::timbre });
    }

    void noteKeyStateChanged (juce::MPENote) override
    {
    }

    void noteReleased (juce::MPENote note) override
    {
        addNoteToList (note, currentEventBeat);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MPEtoNoteExpression)
};

//==============================================================================
bool MidiExpression::listHasExpression (const MidiList& midiList) noexcept
{
    for (auto note : midiList.getNotes())
        if (noteHasExpression (note->state))
            return true;

    return false;
}

namespace NoteHelpers
{
    inline juce::MidiMessage createNoteOn (int midiChannel, int noteNumber, float noteOnVelocity) noexcept
    {
        // a note cannot be started with a zero note-on velocity:
        const float noteOnVelocityToUse = std::max (noteOnVelocity, 1.0f / 127.0f);
        return juce::MidiMessage::noteOn (midiChannel, noteNumber, noteOnVelocityToUse);
    }

    inline juce::MidiMessage createPitchbend (int midiChannel, float pitchbend, float pitchbendRange) noexcept
    {
        const int midiPitchWheelPos = juce::MidiMessage::pitchbendToPitchwheelPos (pitchbend, pitchbendRange);
        return juce::MidiMessage::pitchWheel (midiChannel, midiPitchWheelPos);
    }

    inline juce::MidiMessage createPressure (int midiChannel, float pressure) noexcept
    {
        auto midiPressure = juce::MidiMessage::floatValueToMidiByte (pressure);
        return juce::MidiMessage::channelPressureChange (midiChannel, midiPressure);
    }

    inline juce::MidiMessage createTimbre (int midiChannel, float timbre) noexcept
    {
        auto midiCcValue = juce::MidiMessage::floatValueToMidiByte (timbre);
        return juce::MidiMessage::controllerEvent (midiChannel, 74, midiCcValue);
    }

    inline juce::MidiMessage createNoteOff (int midiChannel, int noteNumber, float noteOffVelocity) noexcept
    {
        return juce::MidiMessage::noteOff (midiChannel, noteNumber, noteOffVelocity);
    }
}

inline void addMidiNoteOnExpressionToSequence (juce::MidiMessageSequence& seq, const juce::ValueTree& state, int midiChannel, double noteOnTime) noexcept
{
    using namespace NoteHelpers;
    jassert (state.hasType (IDs::NOTE));

    const float pitchbend = state.getProperty (IDs::bend, MidiList::defaultInitialPitchBendValue);
    const float pressure  = state.getProperty (IDs::pres, MidiList::defaultInitialPressureValue);
    const float timbre    = state.getProperty (IDs::timb, MidiList::defaultInitialTimbreValue);

    seq.addEvent (juce::MidiMessage (createPitchbend (midiChannel, pitchbend, 48.0f), noteOnTime));
    seq.addEvent (juce::MidiMessage (createPressure (midiChannel, pressure), noteOnTime));
    seq.addEvent (juce::MidiMessage (createTimbre (midiChannel, timbre), noteOnTime));
}

static void addMidiExpressionToSequence (juce::MidiMessageSequence& seq, const juce::ValueTree& state,
                                         const MidiClip& clip, MidiList::TimeBase tb, int midiChannel,
                                         BeatPosition notePlaybackBeat, BeatPosition notePlaybackEndBeat) noexcept
{
    using namespace NoteHelpers;

    auto& ts = clip.edit.tempoSequence;
    const auto beat = notePlaybackBeat + BeatDuration::fromBeats (static_cast<double> (state.getProperty (IDs::b)));

    if (beat > notePlaybackEndBeat)
        return;

    const auto time = tb == MidiList::TimeBase::beats ? beat.inBeats()
                                                      : ts.toTime (beat).inSeconds();

    if (state.hasType (IDs::PITCHBEND))
        seq.addEvent (juce::MidiMessage (createPitchbend (midiChannel, state[IDs::v], 48.0f), time));
    else if (state.hasType (IDs::PRESSURE))
        seq.addEvent (juce::MidiMessage (createPressure (midiChannel, state[IDs::v]), time));
    else if (state.hasType (IDs::TIMBRE))
        seq.addEvent (juce::MidiMessage (createTimbre (midiChannel, state[IDs::v]), time));
    else
        jassertfalse;
}

static void addExpressiveNoteToSequence (juce::MidiMessageSequence& seq, const MidiClip& clip, MidiList::TimeBase tb, const MidiNote& note, int midiChannel, const GrooveTemplate* grooveTemplate)
{
    if (note.isMute() || note.getLengthBeats() <= BeatDuration::fromBeats (0.00001))
        return;

    const auto downTime = tb == MidiList::TimeBase::beats ? note.getPlaybackBeats (MidiNote::startEdge, clip, grooveTemplate).inBeats()
                                                          : note.getPlaybackTime (MidiNote::startEdge, clip, grooveTemplate).inSeconds();
    const auto upTime   = tb == MidiList::TimeBase::beats ? note.getPlaybackBeats (MidiNote::endEdge,   clip, grooveTemplate).inBeats()
                                                          : note.getPlaybackTime (MidiNote::endEdge,   clip, grooveTemplate).inSeconds();

    if (upTime < downTime || upTime <= 0.0)
        return;

    auto state = note.state;
    const auto noteStartBeat = tb == MidiList::TimeBase::beats ? BeatPosition::fromBeats (downTime)
                                                               : clip.edit.tempoSequence.toBeats (TimePosition::fromSeconds (downTime));
    const auto noteEndBeat = tb == MidiList::TimeBase::beats ? BeatPosition::fromBeats (upTime)
                                                             : clip.edit.tempoSequence.toBeats (TimePosition::fromSeconds (upTime));

    // First add expression with defaults if not present
    addMidiNoteOnExpressionToSequence (seq, state, midiChannel, downTime);

    // Then note-on
    using namespace NoteHelpers;
    const auto noteOnVelocity = juce::jlimit (0.0f, 1.0f, note.getVelocity() / 127.0f);
    seq.addEvent (juce::MidiMessage (createNoteOn (midiChannel, note.getNoteNumber(), noteOnVelocity), downTime));

    // Then modulating expression
    for (auto v : state)
        addMidiExpressionToSequence (seq, v, clip, tb, midiChannel, noteStartBeat, noteEndBeat);

    // Finally note-off
    const auto noteOffVelocity = state.hasProperty (IDs::lift) ? int (state[IDs::lift]) / 127.0f : 0.0f;
    seq.addEvent (juce::MidiMessage (createNoteOff (midiChannel, note.getNoteNumber(), noteOffVelocity), upTime));
}

//==============================================================================
static void addToSequence (juce::MidiMessageSequence& seq, const MidiClip& clip, MidiList::TimeBase tb,
                           const MidiNote& note, int channelNumber, bool addNoteUp,
                           const GrooveTemplate* grooveTemplate)
{
    jassert (channelNumber < 17); // SysEx?

    if (note.isMute() || note.getLengthBeats() <= BeatDuration::fromBeats (0.00001))
        return;

    const auto downTime = [&]
    {
        switch (tb)
        {
            case MidiList::TimeBase::beatsRaw:  return note.getStartBeat().inBeats();
            case MidiList::TimeBase::beats:     return note.getPlaybackBeats (MidiNote::startEdge, clip, grooveTemplate).inBeats();
            case MidiList::TimeBase::seconds:   [[ fallthrough ]];
            default:                            return note.getPlaybackTime (MidiNote::startEdge, clip, grooveTemplate).inSeconds();
        }
    }();

    auto velocity = (uint8_t) note.getVelocity();
    int noteNumber = note.getNoteNumber();

    if (addNoteUp)
    {
        // nudge the note-up backwards just a bit to make sure the ordering is correct
        const auto upTime = [&]
        {
            switch (tb)
            {
                case MidiList::TimeBase::beatsRaw:  return note.getEndBeat().inBeats();
                case MidiList::TimeBase::beats:     return note.getPlaybackBeats (MidiNote::endEdge, clip, grooveTemplate).inBeats();
                case MidiList::TimeBase::seconds:   [[ fallthrough ]];
                default:                            return note.getPlaybackTime (MidiNote::endEdge, clip, grooveTemplate).inSeconds();
            }
        }();

        if (upTime > downTime && upTime > 0.0)
        {
            seq.addEvent (juce::MidiMessage::noteOn (channelNumber, noteNumber, velocity), std::max (0.0, downTime));
            seq.addEvent (juce::MidiMessage::noteOff (channelNumber, noteNumber), upTime);
        }
    }
    else if (downTime >= 0.0)
    {
        seq.addEvent (juce::MidiMessage::noteOn (channelNumber, noteNumber, velocity), downTime);
    }
}

static void addToSequence (juce::MidiMessageSequence& seq, const MidiClip& clip, MidiList::TimeBase tb,
                           const MidiControllerEvent& controller, int channelNumber)
{
    const auto time = [&]
                      {
                          switch (tb)
                          {
                              case MidiList::TimeBase::beatsRaw:  return controller.getBeatPosition().inBeats();
                              case MidiList::TimeBase::beats:     return std::max (0_bp, controller.getEditBeats (clip) - toDuration (clip.getStartBeat())).inBeats();
                              case MidiList::TimeBase::seconds:   [[ fallthrough ]];
                              default:                            return std::max (0_tp, controller.getEditTime (clip) - toDuration (clip.getPosition().getStart())).inSeconds();
                          }
                      }();

    const auto type = controller.getType();
    const auto value = controller.getControllerValue();

    if (juce::isPositiveAndBelow (type, 128))
    {
        seq.addEvent (juce::MidiMessage::controllerEvent (channelNumber, type, value >> 7), time);
        return;
    }

    switch (type)
    {
        case MidiControllerEvent::programChangeType:
            if (clip.isSendingBankChanges())
            {
                auto id = clip.getAudioTrack()->getIdForBank (controller.getMetadata());

                seq.addEvent (juce::MidiMessage::controllerEvent (channelNumber, 0x00, MidiControllerEvent::bankIDToCoarse (id)), time);
                seq.addEvent (juce::MidiMessage::controllerEvent (channelNumber, 0x20, MidiControllerEvent::bankIDToFine (id)), time);
            }

            seq.addEvent (juce::MidiMessage::programChange (channelNumber, value >> 7), time);
            break;

        case MidiControllerEvent::aftertouchType:
            seq.addEvent (juce::MidiMessage::aftertouchChange (channelNumber, controller.getMetadata(), value >> 7), time);
            break;

        case MidiControllerEvent::pitchWheelType:
            seq.addEvent (juce::MidiMessage::pitchWheel (channelNumber, value), time);
            break;

        case MidiControllerEvent::channelPressureType:
            seq.addEvent (juce::MidiMessage::channelPressureChange (channelNumber, value >> 7), time);
            break;

        default:
            jassertfalse;
            break;
    }
}

static void addToSequence (juce::MidiMessageSequence& seq, const MidiClip& clip, MidiList::TimeBase tb, const MidiSysexEvent& sysex)
{
    const auto time = [&]
                      {
                          switch (tb)
                          {
                              case MidiList::TimeBase::beatsRaw:  return sysex.getBeatPosition().inBeats();
                              case MidiList::TimeBase::beats:     return std::max (0_bp, sysex.getEditBeats (clip) - toDuration (clip.getStartBeat())).inBeats();
                              case MidiList::TimeBase::seconds:   [[ fallthrough ]];
                              default:                            return std::max (0_tp, sysex.getEditTime (clip) - toDuration (clip.getPosition().getStart())).inSeconds();
                          }
                      }();
    auto m = sysex.getMessage();
    m.setTimeStamp (time);
    seq.addEvent (m);
}

//==============================================================================
/**
    Determines the channels to assign to overlapping notes.
*/
class MPEChannelAssigner
{
public:
    //==========================================================================
    /** Constructor. */
    MPEChannelAssigner (juce::MidiMessageSequence& s, const MidiClip& c, MidiList::TimeBase tb, const GrooveTemplate* g)
        : seq (s), clip (c), timeBase (tb), groove (g)
    {
        zoneLayout.setLowerZone (15);
        const auto mpeZone = zoneLayout.getLowerZone();

        midiChannelBegin = mpeZone.getFirstMemberChannel() - 1;
        midiChannelEnd = mpeZone.getLastMemberChannel();
        midiChannelLastAssigned = midiChannelEnd - 1;
    }

    void addNote (MidiNote& note)
    {
        if (timeBase == MidiList::TimeBase::beats)
            clearNotesEndingBefore (note.getPlaybackBeats (MidiNote::startEdge, clip, groove));
        else
            clearNotesEndingBefore (note.getPlaybackTime (MidiNote::startEdge, clip, groove));

        auto midiChannel = findMidiChannelForNewNote (note.getNoteNumber());
        midiChannels[midiChannel].notes.add (&note);
        midiChannels[midiChannel].lastNoteNumberPlayed = note.getNoteNumber();

        addExpressiveNoteToSequence (seq, clip, timeBase, note, zoneLayout.getLowerZone().getMasterChannel() + midiChannel, groove);
    }

private:
    //==========================================================================
    struct MidiChannel
    {
        MidiNote* getNoteIfPresent (MidiNote& noteToLookFor) const noexcept
        {
            for (auto note : notes)
                if (note == &noteToLookFor)
                    return note;

            return {};
        }

        bool isFree() const noexcept
        {
            return notes.isEmpty();
        }

        juce::Array<MidiNote*> notes;
        int lastNoteNumberPlayed = -1;
    };

    int getNumMidiChannels() const noexcept
    {
        return midiChannelEnd - midiChannelBegin;
    }

    void clearNotesEndingBefore (TimePosition time)
    {
        // Iterate the notes in reverse as they will be removed when stopped
        for (auto& midiChannel : midiChannels)
            for (int i = midiChannel.notes.size(); --i >= 0;)
                if (auto n = midiChannel.notes.getUnchecked (i))
                    if (n->getPlaybackTime (MidiNote::endEdge, clip, groove) < time)
                        stopNote (*n);
    }

    void clearNotesEndingBefore (BeatPosition time)
    {
        // Iterate the notes in reverse as they will be removed when stopped
        for (auto& midiChannel : midiChannels)
            for (int i = midiChannel.notes.size(); --i >= 0;)
                if (auto n = midiChannel.notes.getUnchecked (i))
                    if (n->getPlaybackBeats (MidiNote::endEdge, clip, groove) < time)
                        stopNote (*n);
    }

    void stopNote (MidiNote& note)
    {
        for (int ch = midiChannelBegin; ch < midiChannelEnd; ++ch)
        {
            if (midiChannels[ch].notes.contains (&note))
            {
                midiChannels[ch].notes.removeFirstMatchingValue (&note);
                return;
            }
        }

        jassertfalse;
    }

    int findMidiChannelForNewNote (int noteNumber) noexcept
    {
        const int numMidiChannels = getNumMidiChannels();

        jassert (numMidiChannels > 0);

        if (numMidiChannels == 1)
            return midiChannelBegin;

        for (int ch = midiChannelBegin; ch != midiChannelEnd; ++ch)
            if (midiChannels[ch].isFree() && midiChannels[ch].lastNoteNumberPlayed == noteNumber)
                return ch;

        // find next free channel in round robin assignment
        for (int ch = midiChannelLastAssigned + 1; ; ++ch)
        {
            if (ch == midiChannelEnd)  // loop wrap-around
                ch = midiChannelBegin;

            if (midiChannels[ch].isFree())
            {
                midiChannelLastAssigned = ch;
                return ch;
            }

            if (ch == midiChannelLastAssigned)
                break; // no free channels!
        }

        midiChannelLastAssigned = findMidiChannelPlayingClosestNonequalNote (noteNumber);

        return midiChannelLastAssigned;
    }

    int findMidiChannelPlayingClosestNonequalNote (int noteNumber) noexcept
    {
        int channelWithClosestNote = midiChannelBegin;
        int closestNoteDistance = 127;

        for (int ch = midiChannelBegin; ch < midiChannelEnd; ++ch)
        {
            for (auto note : midiChannels[ch].notes)
            {
                const int noteDistance = std::abs (note->getNoteNumber() - noteNumber);

                if (noteDistance > 0 && noteDistance < closestNoteDistance)
                {
                    closestNoteDistance = noteDistance;
                    channelWithClosestNote = ch;
                }
            }
        }

        return channelWithClosestNote;
    }

    //==========================================================================
    juce::MidiMessageSequence& seq;
    const MidiClip& clip;
    const MidiList::TimeBase timeBase;
    const GrooveTemplate* groove;
    juce::MPEZoneLayout zoneLayout;
    MidiChannel midiChannels[16];
    int midiChannelBegin, midiChannelEnd, midiChannelLastAssigned;
};

//==============================================================================
static juce::ValueTree createNoteValueTree (int pitch, BeatPosition beat, BeatDuration length, int vel, int col)
{
    return createValueTree (IDs::NOTE,
                            IDs::p, pitch,
                            IDs::b, beat.inBeats(),
                            IDs::l, std::max (0.0, length.inBeats()),
                            IDs::v, vel,
                            IDs::c, col);
}

juce::ValueTree MidiNote::createNote (const MidiNote& n, BeatPosition newStart, BeatDuration newLength)
{
    juce::ValueTree v (n.state.createCopy());
    v.setProperty (IDs::b, newStart.inBeats(), nullptr);
    v.setProperty (IDs::l, newLength.inBeats(), nullptr);

    return v;
}

MidiNote::MidiNote (const juce::ValueTree& v)
    : state (v)
{
    updatePropertiesFromState();
}

void MidiNote::updatePropertiesFromState()
{
    noteNumber      = (uint8_t) juce::jlimit (0, 127, static_cast<int> (state.getProperty (IDs::p)));
    startBeat       = BeatPosition::fromBeats (static_cast<double> (state.getProperty (IDs::b)));
    lengthInBeats   = std::max (BeatDuration(), BeatDuration::fromBeats (static_cast<double> (state.getProperty (IDs::l))));
    velocity        = (uint8_t) juce::jlimit (0, 127, static_cast<int> (state.getProperty (IDs::v)));
    colour          = (uint8_t) juce::jlimit (0, 127, static_cast<int> (state.getProperty (IDs::c)));
    mute            = state.getProperty (IDs::m) ? 1 : 0;
}

template<>
struct MidiList::EventDelegate<MidiNote>
{
    static bool isSuitableType (const juce::ValueTree& v)
    {
        return v.hasType (IDs::NOTE);
    }

    static bool updateObject (MidiNote& m, const juce::Identifier& i)
    {
        m.updatePropertiesFromState();
        return i == IDs::b;
    }

    static void removeFromSelection (MidiNote* m)
    {
        removeMidiEventFromSelection (m);
    }
};

//==============================================================================
BeatPosition MidiNote::getQuantisedStartBeat (const MidiClip& c) const
{
    return c.getQuantisation().roundBeatToNearest (startBeat + toDuration (c.getContentStartBeat()))
           - toDuration (c.getContentStartBeat());
}

BeatPosition MidiNote::getQuantisedStartBeat (const MidiClip* const c) const
{
    if (c != nullptr)
        return getQuantisedStartBeat (*c);

    return startBeat;
}

BeatPosition MidiNote::getQuantisedEndBeat (const MidiClip& c) const
{
    return c.getQuantisation().roundBeatToNearest (startBeat + toDuration (c.getContentStartBeat()))
            + lengthInBeats - toDuration (c.getContentStartBeat());
}

BeatPosition MidiNote::getQuantisedEndBeat (const MidiClip* const c) const
{
    if (c != nullptr)
        return getQuantisedEndBeat (*c);

    return startBeat + lengthInBeats;
}

BeatDuration MidiNote::getQuantisedLengthBeats (const MidiClip& c) const
{
    return getQuantisedEndBeat (c) - getQuantisedStartBeat (c);
}

BeatDuration MidiNote::getQuantisedLengthBeats (const MidiClip* const c) const
{
    return getQuantisedEndBeat (c) - getQuantisedStartBeat (c);
}

//==============================================================================
TimePosition MidiNote::getEditStartTime (const MidiClip& c) const
{
    const auto quantisedBeatInEdit = c.getQuantisation().roundBeatToNearest (startBeat + toDuration (c.getContentStartBeat()));

    return c.edit.tempoSequence.toTime (quantisedBeatInEdit);
}

TimePosition MidiNote::getEditEndTime (const MidiClip& c) const
{
    const auto quantisedBeatInEdit = c.getQuantisation().roundBeatToNearest (startBeat + toDuration (c.getContentStartBeat()))
                                        + lengthInBeats;

    return c.edit.tempoSequence.toTime (quantisedBeatInEdit);
}

TimeRange MidiNote::getEditTimeRange (const MidiClip& c) const
{
    const auto quantisedStartBeat = c.getQuantisation().roundBeatToNearest (startBeat - toDuration (c.getLoopStartBeats()) + toDuration (c.getContentStartBeat()));

    return { c.edit.tempoSequence.toTime (quantisedStartBeat),
             c.edit.tempoSequence.toTime (quantisedStartBeat + lengthInBeats) };
}

TimeDuration MidiNote::getLengthSeconds (const MidiClip& c) const
{
    return getEditTimeRange (c).getLength();
}

//==============================================================================
void MidiNote::setStartAndLength (BeatPosition newStartBeat, BeatDuration newLengthInBeats, juce::UndoManager* undoManager)
{
    newStartBeat = std::max (BeatPosition(), newStartBeat);

    if (newLengthInBeats <= BeatDuration())
        newLengthInBeats = BeatDuration::fromBeats (1.0 / Edit::ticksPerQuarterNote);

    if (startBeat != newStartBeat)
    {
        state.setProperty (IDs::b, newStartBeat.inBeats(), undoManager);
        startBeat = newStartBeat;
    }

    if (lengthInBeats != newLengthInBeats)
    {
        state.setProperty (IDs::l, newLengthInBeats.inBeats(), undoManager);
        lengthInBeats = newLengthInBeats;
    }
}

void MidiNote::setNoteNumber (int newNoteNumber, juce::UndoManager* undoManager)
{
    newNoteNumber = juce::jlimit (0, 127, newNoteNumber);

    if (getNoteNumber() != newNoteNumber)
    {
        state.setProperty (IDs::p, newNoteNumber, undoManager);
        noteNumber = (uint8_t) newNoteNumber;
    }
}

void MidiNote::setVelocity (int newVelocity, juce::UndoManager* undoManager)
{
    newVelocity = juce::jlimit (0, 127, newVelocity);

    if (getVelocity() != newVelocity)
    {
        state.setProperty (IDs::v, newVelocity, undoManager);
        velocity = (uint8_t) newVelocity;
    }
}

void MidiNote::setColour (int newColourIndex, juce::UndoManager* um)
{
    newColourIndex = juce::jlimit (0, 127, newColourIndex);

    if (getColour() != newColourIndex)
    {
        state.setProperty (IDs::c, newColourIndex, um);
        colour = (uint8_t) newColourIndex;
    }
}

void MidiNote::setMute (bool shouldMute, juce::UndoManager* um)
{
    if (isMute() != shouldMute)
    {
        state.setProperty (IDs::m, shouldMute, um);
        mute = shouldMute ? 1 : 0;
    }
}

//==============================================================================
TimePosition MidiNote::getPlaybackTime (NoteEdge edge, const MidiClip& clip, const GrooveTemplate* const grooveTemplate) const
{
    auto pos = clip.getPosition();

    // nudge the note-up backwards just a bit to make sure the ordering is correct
    auto time = edge == startEdge ? getEditStartTime (clip)
                                  : std::min (getEditEndTime (clip), pos.getEnd()) - TimeDuration::fromSeconds (0.0001);

    if (grooveTemplate != nullptr)
        time = grooveTemplate->editTimeToGroovyTime (time, clip.getGrooveStrength(), clip.edit);

    return time - toDuration (pos.getStart());
}

BeatPosition MidiNote::getPlaybackBeats (NoteEdge edge, const MidiClip& clip, const GrooveTemplate* const grooveTemplate) const
{
    auto pos = clip.getPosition();

    // nudge the note-up backwards just a bit to make sure the ordering is correct
    auto time = edge == startEdge ? clip.getQuantisation().roundBeatToNearest (startBeat + toDuration (clip.getContentStartBeat()))
                                  : std::min (clip.getQuantisation().roundBeatToNearest (startBeat + toDuration (clip.getContentStartBeat()))
                                              + lengthInBeats, clip.edit.tempoSequence.toBeats (pos.getEnd())) - 0.00001_bd;

    if (grooveTemplate != nullptr)
        time = grooveTemplate->beatsTimeToGroovyTime (time, clip.getGrooveStrength());

    return time - toDuration (clip.edit.tempoSequence.toBeats (pos.getStart()));
}

//==============================================================================
juce::ValueTree MidiControllerEvent::createControllerEvent (const MidiControllerEvent& e, BeatPosition beat)
{
    juce::ValueTree v (e.state.createCopy());
    v.setProperty (IDs::b, beat.inBeats(), nullptr);
    return v;
}

juce::ValueTree MidiControllerEvent::createControllerEvent (BeatPosition beat, int controllerType, int controllerValue)
{
    return createValueTree (IDs::CONTROL,
                            IDs::b,     beat.inBeats(),
                            IDs::type,  controllerType,
                            IDs::val,   controllerValue);
}

juce::ValueTree MidiControllerEvent::createControllerEvent (BeatPosition beat, int controllerType, int controllerValue, int metadata)
{
    auto v = createControllerEvent (beat, controllerType, controllerValue);
    v.setProperty (IDs::metadata, metadata, nullptr);
    return v;
}

MidiControllerEvent::MidiControllerEvent (const juce::ValueTree& v)
    : state (v),
      beatNumber (BeatPosition::fromBeats (static_cast<double> (v.getProperty (IDs::b)))),
      type (int (v.getProperty (IDs::type))),
      value (v.getProperty (IDs::val))
{
    updatePropertiesFromState();
}

void MidiControllerEvent::updatePropertiesFromState() noexcept
{
    beatNumber  = BeatPosition::fromBeats (static_cast<double> (state.getProperty (IDs::b)));
    type        = state.getProperty (IDs::type);
    value       = state.getProperty (IDs::val);
    metadata    = state.getProperty (IDs::metadata);
}

template<>
struct MidiList::EventDelegate<MidiControllerEvent>
{
    static bool isSuitableType (const juce::ValueTree& v)
    {
        return v.hasType (IDs::CONTROL);
    }

    static bool updateObject (MidiControllerEvent& e, const juce::Identifier& i)
    {
        e.updatePropertiesFromState();
        return i == IDs::b;
    }

    static void removeFromSelection (MidiControllerEvent* e)
    {
        removeMidiEventFromSelection (e);
    }
};

//==============================================================================
juce::String MidiControllerEvent::getControllerTypeName (int type) noexcept
{
    if (juce::isPositiveAndBelow (type, 128))
        return TRANS(juce::MidiMessage::getControllerName (type));

    switch (type)
    {
        case programChangeType:     return TRANS("Program Change");
        case noteVelocities:        return TRANS("Note Velocities");
        case aftertouchType:        return TRANS("Aftertouch");
        case pitchWheelType:        return TRANS("Pitch Wheel");
        case sysExType:             return TRANS("SysEx Events");
        case channelPressureType:   return TRANS("Channel Pressure");
        default: break;
    };

    return "(" + TRANS("Unnamed") + ")";
}

BeatPosition MidiControllerEvent::getEditBeats (const MidiClip& c) const
{
    return c.getQuantisation().roundBeatToNearest (beatNumber - toDuration (c.getLoopStartBeats()) + toDuration (c.getContentStartBeat()));
}

TimePosition MidiControllerEvent::getEditTime (const MidiClip& c) const
{
    return c.edit.tempoSequence.toTime (getEditBeats (c));
}

juce::String MidiControllerEvent::getLevelDescription (MidiClip* ownerClip) const
{
    const int coarseValue = value >> 7;

    if (type == programChangeType)
    {
        juce::String bank;
        auto program = juce::String (coarseValue + 1) + " - ";

        if (ownerClip != nullptr && ownerClip->getTrack() != nullptr)
        {
            auto audioTrack = ownerClip->getAudioTrack();
            bank = audioTrack->getNameForBank (metadata);
            program += audioTrack->getNameForProgramNumber (coarseValue, metadata);
        }
        else
        {
            bank = TRANS("Bank") + " " + juce::String ((value & 0x0F) + 1);
            program += TRANS(juce::MidiMessage::getGMInstrumentName (coarseValue));
        }

        return bank + ": " + program;
    }

    if (type == pitchWheelType)
    {
        juce::String s (TRANS("Pitch Wheel") + ": ");
        const int percent = juce::jlimit (-100, 100, (int)((value - 0x2000) * (100.5 / 0x2000)));

        if (percent > 0)
            s += "+";

        s << percent << "%";

        return s;
    }

    if (type == channelPressureType)
        return TRANS("Channel Pressure") + " - " + juce::String (coarseValue);

    if (type == aftertouchType)
        return TRANS("Aftertouch") + " - " + juce::String (coarseValue);

    if (juce::isPositiveAndBelow (type, 128))
    {
        auto controllerName = TRANS(juce::MidiMessage::getControllerName (type));

        if (controllerName.isEmpty())
            controllerName = TRANS("Controller Number") + " " + juce::String (type);

        return controllerName + " - " + juce::String (coarseValue);
    }

    return juce::String (coarseValue);
}

void MidiControllerEvent::setMetadata (int m, juce::UndoManager* um)
{
    if (metadata != m)
    {
        state.setProperty (IDs::metadata, m, um);
        metadata = m;
    }
}

void MidiControllerEvent::setBeatPosition (BeatPosition newBeatNumber, juce::UndoManager* um)
{
    newBeatNumber = std::max (BeatPosition(), newBeatNumber);

    if (beatNumber != newBeatNumber)
    {
        state.setProperty (IDs::b, newBeatNumber.inBeats(), um);
        beatNumber = newBeatNumber;
    }
}

void MidiControllerEvent::setType (int t, juce::UndoManager* um)
{
    if (t != type)
    {
        state.setProperty (IDs::type, t, um);
        type = t;
    }
}

void MidiControllerEvent::setControllerValue (int newValue, juce::UndoManager* um)
{
    if (value != newValue)
    {
        state.setProperty (IDs::val, newValue, um);
        value = newValue;
    }
}

int MidiControllerEvent::getControllerDefautValue (int type)
{
    switch (type)
    {
        case pitchWheelType: return 8192;
        case 7:              return 99 << 7; // volume
        case 10:             return 64 << 7; // pan
        default:             return 0;
    }
}

//==============================================================================
static juce::String midiToHex (const juce::MidiMessage& m)
{
    return juce::String::toHexString (m.getRawData(), m.getRawDataSize(), 0);
}

juce::ValueTree MidiSysexEvent::createSysexEvent (const MidiSysexEvent& e, BeatPosition time)
{
    juce::ValueTree v (e.state.createCopy());
    v.setProperty (IDs::time, time.inBeats(), nullptr);
    return v;
}

juce::ValueTree MidiSysexEvent::createSysexEvent (const juce::MidiMessage& m, BeatPosition time)
{
    return createValueTree (IDs::SYSEX,
                            IDs::time, time.inBeats(),
                            IDs::data, midiToHex (m));
}

MidiSysexEvent::MidiSysexEvent (const juce::ValueTree& v)  : state (v)
{
    updateMessage();
}

void MidiSysexEvent::updateMessage()
{
    auto time = state.getProperty (IDs::time);

    juce::MemoryBlock d;
    d.loadFromHexString (state.getProperty (IDs::data).toString());

    if (d.getSize() > 0)
        message = juce::MidiMessage (d.getData(), (int) d.getSize(), time);
    else
        message.setTimeStamp (time);
}

void MidiSysexEvent::updateTime()
{
    message.setTimeStamp (state.getProperty (IDs::time));
}

template<>
struct MidiList::EventDelegate<MidiSysexEvent>
{
    static bool isSuitableType (const juce::ValueTree& v)
    {
        return v.hasType (IDs::SYSEX);
    }

    static bool updateObject (MidiSysexEvent& m, const juce::Identifier& i)
    {
        if (i == IDs::time)
        {
            m.updateTime();
            return true;
        }

        if (i == IDs::data)
            m.updateMessage();

        return false;
    }

    static void removeFromSelection (MidiSysexEvent* m)
    {
        removeMidiEventFromSelection (m);
    }
};

TimePosition MidiSysexEvent::getEditTime (const MidiClip& c) const
{
    return c.edit.tempoSequence.toTime (getEditBeats (c));
}

BeatPosition MidiSysexEvent::getEditBeats (const MidiClip& c) const
{
    return c.getQuantisation().roundBeatToNearest (getBeatPosition() - toDuration (c.getLoopStartBeats()) + toDuration (c.getContentStartBeat()));
}

void MidiSysexEvent::setMessage (const juce::MidiMessage& m, juce::UndoManager* um)
{
    state.setProperty (IDs::data, midiToHex (m), um);
}

void MidiSysexEvent::setBeatPosition (BeatPosition newBeatNumber, juce::UndoManager* um)
{
    state.setProperty (IDs::time, std::max (0.0, newBeatNumber.inBeats()), um);
}

//==============================================================================
juce::ValueTree MidiList::createMidiList()
{
    return createValueTree (IDs::SEQUENCE,
                            IDs::ver, 1,
                            IDs::channelNumber, MidiChannel (1));
}

MidiList::MidiList()  : state (IDs::SEQUENCE)
{
    state.setProperty (IDs::ver, 1, nullptr);
    initialise (nullptr);
}

MidiList::MidiList (const juce::ValueTree& v, juce::UndoManager* um)
    : state (v)
{
    jassert (state.hasType (IDs::SEQUENCE));
    state.setProperty (IDs::ver, 1, um);
    convertMidiPropertiesFromStrings (state);

    initialise (um);
}

MidiList::~MidiList()
{
}

void MidiList::initialise (juce::UndoManager* um)
{
    CRASH_TRACER

    midiChannel.referTo (state, IDs::channelNumber, um);
    isComp.referTo (state, IDs::isComp, um, false);

    noteList        = std::make_unique<EventList<MidiNote>> (state);
    controllerList  = std::make_unique<EventList<MidiControllerEvent>> (state);
    sysexList       = std::make_unique<EventList<MidiSysexEvent>> (state);
}

void MidiList::clear (juce::UndoManager* um)
{
    state.removeAllChildren (um);
    importedName = {};
}

void MidiList::copyFrom (const MidiList& other, juce::UndoManager* um)
{
    if (this != &other)
    {
        clear (um);
        state.copyPropertiesFrom (other.state, um);
        addFrom (other, um);
    }
}

void MidiList::addFrom (const MidiList& other, juce::UndoManager* um)
{
    if (this != &other)
        for (int i = 0; i < other.state.getNumChildren(); ++i)
            state.addChild (other.state.getChild (i).createCopy(), -1, um);
}

void MidiList::setMidiChannel (MidiChannel newChannel)
{
    midiChannel = newChannel;
}

//==============================================================================
template<typename EventType>
const juce::Array<EventType*>& getEventsChecked (const juce::Array<EventType*>& events)
{
   #if JUCE_DEBUG
    BeatPosition lastBeat;

    for (auto* e : events)
    {
        auto beat = e->getBeatPosition();
        jassert (lastBeat <= beat);
        lastBeat = beat;
    }
   #endif

    return events;
}

const juce::Array<MidiNote*>& MidiList::getNotes() const
{
    jassert (noteList != nullptr);
    return getEventsChecked (noteList->getSortedList());
}

const juce::Array<MidiControllerEvent*>& MidiList::getControllerEvents() const
{
    jassert (controllerList != nullptr);
    return getEventsChecked (controllerList->getSortedList());
}

const juce::Array<MidiSysexEvent*>& MidiList::getSysexEvents() const
{
    jassert (sysexList != nullptr);
    return getEventsChecked (sysexList->getSortedList());
}

//==============================================================================
void MidiList::moveAllBeatPositions (BeatDuration delta, juce::UndoManager* um)
{
    if (delta != BeatDuration())
    {
        for (auto e : getNotes())
            e->setStartAndLength (e->getStartBeat() + delta, e->getLengthBeats(), um);

        for (auto e : getControllerEvents())
            e->setBeatPosition (e->getBeatPosition() + delta, um);

        for (auto e : getSysexEvents())
            e->setBeatPosition (e->getBeatPosition() + delta, um);
    }
}

void MidiList::rescale (double factor, juce::UndoManager* um)
{
    if (factor != 1.0 && factor > 0.001 && factor < 1000.0)
    {
        for (auto e : getNotes())
            e->setStartAndLength (e->getStartBeat() * factor,
                                  e->getLengthBeats() * factor, um);

        for (auto e : getControllerEvents())
            e->setBeatPosition (e->getBeatPosition() * factor, um);

        for (auto e : getSysexEvents())
            e->setBeatPosition (e->getBeatPosition() * factor, um);
    }
}

void MidiList::trimOutside (BeatPosition start, BeatPosition end, juce::UndoManager* um)
{
    juce::Array<juce::ValueTree> itemsToRemove;

    for (auto n : getNotes())
    {
        if (n->getStartBeat() >= (end - BeatDuration::fromBeats (0.0001)) || n->getEndBeat() <= (start + BeatDuration::fromBeats (0.0001)))
        {
            itemsToRemove.add (n->state);
        }
        else if (n->getStartBeat() < start || n->getEndBeat() > end)
        {
            auto newStart = std::max (start, n->getStartBeat());
            auto newEnd   = std::min (end, n->getEndBeat());

            n->setStartAndLength (newStart, newEnd - newStart, um);
        }
    }

    for (auto e : getControllerEvents())
        if (e->getBeatPosition() < start || e->getBeatPosition() >= end)
            itemsToRemove.add (e->state);

    for (auto s : getSysexEvents())
        if (s->getBeatPosition() < start || s->getBeatPosition() >= end)
            itemsToRemove.add (s->state);

    for (auto& v : itemsToRemove)
        state.removeChild (v, um);
}

//==============================================================================
BeatPosition MidiList::getFirstBeatNumber() const
{
    auto t = BeatPosition::fromBeats (Edit::maximumLength);

    if (auto first = getNotes().getFirst())             t = std::min (t, first->getStartBeat());
    if (auto first = getControllerEvents().getFirst())  t = std::min (t, first->getBeatPosition());
    if (auto first = getSysexEvents().getFirst())       t = std::min (t, first->getBeatPosition());

    return t;
}

BeatPosition MidiList::getLastBeatNumber() const
{
    BeatPosition t;

    if (auto last = getNotes().getLast())             t = std::max (t, last->getEndBeat());
    if (auto last = getControllerEvents().getLast())  t = std::max (t, last->getBeatPosition());
    if (auto last = getSysexEvents().getLast())       t = std::max (t, last->getBeatPosition());

    return t;
}

MidiNote* MidiList::getNoteFor (const juce::ValueTree& s)
{
    for (auto n : getNotes())
        if (n->state == s)
            return n;

    return {};
}

juce::Range<int> MidiList::getNoteNumberRange() const
{
    if (getNumNotes() == 0)
        return {};

    int min = 127;
    int max = 0;

    for (auto n : getNotes())
    {
        min = std::min (min, n->getNoteNumber());
        max = std::max (max, n->getNoteNumber());
    }

    return { min, max };
}

MidiNote* MidiList::addNote (const MidiNote& note, juce::UndoManager* um)
{
    auto v = note.state.createCopy();
    state.addChild (v, -1, um);
    return noteList->getEventFor (v);
}

MidiNote* MidiList::addNote (int pitch, BeatPosition startBeat, BeatDuration lengthInBeats,
                             int velocity, int colourIndex, juce::UndoManager* um)
{
    auto v = createNoteValueTree (pitch, startBeat, lengthInBeats, velocity, colourIndex);
    state.addChild (v, -1, um);
    return noteList->getEventFor (v);
}

void MidiList::removeNote (MidiNote& note, juce::UndoManager* um)
{
    state.removeChild (note.state, um);
}

void MidiList::removeAllNotes (juce::UndoManager* um)
{
    for (int i = state.getNumChildren(); --i >= 0;)
        if (state.getChild (i).hasType (IDs::NOTE))
            state.removeChild (i, um);
}

//==============================================================================
MidiControllerEvent* MidiList::getControllerEventAt (BeatPosition beatNumber, int controllerType) const
{
    auto& controllerEvents = getControllerEvents();

    for (int i = controllerEvents.size(); --i >= 0;)
    {
        auto e = controllerEvents.getUnchecked (i);

        if (e->getBeatPosition() <= beatNumber && e->getType() == controllerType)
            return controllerEvents.getUnchecked (i);
    }

    return {};
}

bool MidiList::containsController (int controllerType) const
{
    for (auto e : getControllerEvents())
        if (e->getType() == controllerType)
            return true;

    return false;
}

MidiControllerEvent* MidiList::addControllerEvent (const MidiControllerEvent& event, juce::UndoManager* um)
{
    auto v = event.state.createCopy();
    state.addChild (v, -1, um);
    return controllerList->getEventFor (v);
}

MidiControllerEvent* MidiList::addControllerEvent (BeatPosition beat, int controllerType, int controllerValue, juce::UndoManager* um)
{
    auto v = MidiControllerEvent::createControllerEvent (beat, controllerType, controllerValue);
    state.addChild (v, -1, um);
    return controllerList->getEventFor (v);
}

MidiControllerEvent* MidiList::addControllerEvent (BeatPosition beat, int controllerType, int controllerValue, int metadata, juce::UndoManager* um)
{
    auto v = MidiControllerEvent::createControllerEvent (beat, controllerType, controllerValue, metadata);
    state.addChild (v, -1, um);
    return controllerList->getEventFor (v);
}

void MidiList::removeControllerEvent (MidiControllerEvent& e, juce::UndoManager* um)
{
    state.removeChild (e.state, um);
}

void MidiList::removeAllControllers (juce::UndoManager* um)
{
    for (int i = state.getNumChildren(); --i >= 0;)
        if (state.getChild (i).hasType (IDs::CONTROL))
            state.removeChild (i, um);
}

void MidiList::setControllerValueAt (int controllerType, BeatPosition beatNumber, int newValue, juce::UndoManager* um)
{
    beatNumber = std::max (BeatPosition(), beatNumber);
    auto& controllerEvents = getControllerEvents();

    for (int i = controllerEvents.size(); --i >= 0;) // N.B.: The order matters!
    {
        auto e = controllerEvents.getUnchecked (i);

        if (e->getType() == controllerType
              && e->getBeatPosition() <= beatNumber) // N.B.: '<=' because we want the event the beat is within
        {
            e->setControllerValue (newValue, um);
            break;
        }
    }
}

static int interpolate (int startValue, int rangeValues, BeatPosition startBeat, BeatPosition beat, BeatDuration rangeBeats) noexcept
{
    return juce::roundToInt (startValue + (rangeValues * (beat - startBeat).inBeats() / rangeBeats.inBeats()));
}

void MidiList::insertRepeatedControllerValue (int type, int startVal, int endVal,
                                              BeatRange beats, BeatDuration intervalBeats, juce::UndoManager* um)
{
    auto rangeBeats = beats.getLength();
    auto rangeValues = endVal - startVal;

    if (rangeBeats == BeatDuration())
    {
        addControllerEvent (beats.getStart(), type, startVal, um);
        addControllerEvent (beats.getEnd(), type, endVal, um);
        return;
    }

    auto maxNumSteps = std::max (2, type == MidiControllerEvent::pitchWheelType ? std::abs (rangeValues)
                                                                                : std::abs (rangeValues) >> 7);
    auto numSteps = juce::roundToInt (rangeBeats.inBeats() / intervalBeats.inBeats());
    auto limitedNumSteps = juce::jlimit (2, maxNumSteps, numSteps);

    if (numSteps != limitedNumSteps)
    {
        numSteps = limitedNumSteps;
        intervalBeats = BeatDuration::fromBeats (rangeBeats.inBeats() / numSteps);
    }

    auto beat = beats.getStart();

    for (int i = 0; i < numSteps; ++i)
    {
        const int value = interpolate (startVal, rangeValues, beats.getStart(), beat, rangeBeats);
        addControllerEvent (beat, type, value, um);
        beat = beat + intervalBeats;
    }

    if (beat - intervalBeats < beats.getEnd())
        addControllerEvent (beats.getEnd(), type, endVal, um);
}

void MidiList::removeControllersBetween (int controllerType, BeatPosition beatStart, BeatPosition beatEnd, juce::UndoManager* um)
{
    juce::Array<juce::ValueTree> itemsToRemove;

    for (auto e : getControllerEvents())
        if (e->getType() == controllerType && e->getBeatPosition() >= beatStart && e->getBeatPosition() < beatEnd)
            itemsToRemove.add (e->state);

    for (auto& v : itemsToRemove)
        state.removeChild (v, um);
}

//==============================================================================
MidiSysexEvent* MidiList::getSysexEventFor (const juce::ValueTree& v) const
{
    for (auto n : getSysexEvents())
        if (n->state == v)
            return n;

    return {};
}

MidiSysexEvent& MidiList::addSysExEvent (const juce::MidiMessage& message, BeatPosition beat, juce::UndoManager* um)
{
    auto v = MidiSysexEvent::createSysexEvent (message, beat);
    state.addChild (v, -1, um);

    return *sysexList->getEventFor (v);
}

void MidiList::removeSysExEvent (const MidiSysexEvent& event, juce::UndoManager* um)
{
    state.removeChild (event.state, um);
}

void MidiList::removeAllSysexes (juce::UndoManager* um)
{
    for (int i = state.getNumChildren(); --i >= 0;)
        if (state.getChild (i).hasType (IDs::SYSEX))
            state.removeChild (i, um);
}

//==============================================================================
bool MidiList::fileHasTempoChanges (const juce::File& f)
{
    juce::MidiFile midiFile;

    {
        juce::FileInputStream in (f);

        if (! in.openedOk() || ! midiFile.readFrom (in))
            return false;
    }

    juce::MidiMessageSequence tempoEvents;
    midiFile.findAllTempoEvents (tempoEvents);
    midiFile.findAllTimeSigEvents (tempoEvents);

    return tempoEvents.getNumEvents() > 0;
}

bool MidiList::looksLikeMPEData (const juce::File& f)
{
    juce::MidiFile midiFile;

    {
        juce::FileInputStream in (f);

        if (! in.openedOk() || ! midiFile.readFrom (in))
            return false;
    }

    juce::BigInteger noteChannelsUsed, controllerChannelsUsed;

    for (int currentTrack = midiFile.getNumTracks(); --currentTrack >= 0;)
    {
        auto& trackSequence = *midiFile.getTrack (currentTrack);

        for (int eventIndex = trackSequence.getNumEvents(); --eventIndex >= 0;)
        {
            if (auto* event = trackSequence.getEventPointer (eventIndex))
            {
                const auto& m = event->message;

                if (m.isNoteOn())
                    noteChannelsUsed.setBit (m.getChannel());
                else if (m.isController() || m.isPitchWheel() || m.isChannelPressure())
                    controllerChannelsUsed.setBit (m.getChannel());

                if (noteChannelsUsed.countNumberOfSetBits() > 1
                     && controllerChannelsUsed.countNumberOfSetBits() > 1)
                    return true;
            }
        }
    }

    return false;
}

bool MidiList::readSeparateTracksFromFile (const juce::File& f,
                                           juce::OwnedArray<MidiList>& lists,
                                           juce::Array<BeatPosition>& tempoChangeBeatNumbers,
                                           juce::Array<double>& bpms,
                                           juce::Array<int>& numerators,
                                           juce::Array<int>& denominators,
                                           BeatDuration& songLength,
                                           bool importAsNoteExpression)
{
    songLength = BeatDuration();
    juce::MidiFile midiFile;

    {
        juce::FileInputStream in (f);

        if (! in.openedOk() || ! midiFile.readFrom (in))
            return false;
    }

    juce::MidiMessageSequence tempoEvents;
    midiFile.findAllTempoEvents (tempoEvents);
    midiFile.findAllTimeSigEvents (tempoEvents);

    tempoChangeBeatNumbers.clearQuick();
    bpms.clearQuick();
    numerators.clearQuick();
    denominators.clearQuick();

    int numer = 4, denom = 4;
    double secsPerQuarterNote = 0.5;

    auto timeFormat = midiFile.getTimeFormat();
    auto tickLen = 1.0 / (timeFormat > 0 ? timeFormat & 0x7fff
                                         : ((timeFormat & 0x7fff) >> 8) * (timeFormat & 0xff));

    for (int i = 0; i < tempoEvents.getNumEvents(); ++i)
    {
        auto& msg = tempoEvents.getEventPointer (i)->message;

        if (msg.isTempoMetaEvent())
            secsPerQuarterNote = msg.getTempoSecondsPerQuarterNote();
        else if (msg.isTimeSignatureMetaEvent())
            msg.getTimeSignatureInfo (numer, denom);

        while ((i + 1) < tempoEvents.getNumEvents())
        {
            auto& msg2 = tempoEvents.getEventPointer (i + 1)->message;

            if (msg2.getTimeStamp() == msg.getTimeStamp())
            {
                ++i;

                if (msg2.isTempoMetaEvent())
                    secsPerQuarterNote = msg2.getTempoSecondsPerQuarterNote();
                else if (msg2.isTimeSignatureMetaEvent())
                    msg2.getTimeSignatureInfo (numer, denom);
            }
            else
            {
                break;
            }
        }

        tempoChangeBeatNumbers.add (BeatPosition::fromBeats (tickLen * msg.getTimeStamp()));
        bpms.add (4.0 * 60.0 / (denom * secsPerQuarterNote));
        numerators.add (numer);
        denominators.add (denom);
    }

    if (importAsNoteExpression)
    {
        juce::MidiMessageSequence destSequence;

        for (int currentTrack = 0; currentTrack < midiFile.getNumTracks(); ++currentTrack)
        {
            auto& trackSequence = *midiFile.getTrack (currentTrack);

            if (trackSequence.getNumEvents() > 0)
            {
                for (int i = trackSequence.getNumEvents(); --i >= 0;)
                {
                    auto& msg = trackSequence.getEventPointer (i)->message;
                    msg.setTimeStamp (tickLen * msg.getTimeStamp());
                }

                for (int channel = 0; channel <= 16; ++channel)
                {
                    if (channel == 0)
                        trackSequence.extractSysExMessages (destSequence);
                    else
                        trackSequence.extractMidiChannelMessages (channel, destSequence, true);
                }
            }
        }

        if (destSequence.getNumEvents() > 0)
        {
            destSequence.sort();
            destSequence.updateMatchedPairs();

            songLength = std::max (songLength, BeatDuration::fromBeats (destSequence.getStartTime() + destSequence.getEndTime()));

            std::unique_ptr<MidiList> midiList (new MidiList());
            midiList->importFromEditTimeSequenceWithNoteExpression (destSequence, nullptr, TimePosition(), nullptr);

            if (! midiList->isEmpty())
                lists.add (midiList.release());
        }
    }
    else
    {
        for (int currentTrack = 0; currentTrack < midiFile.getNumTracks(); ++currentTrack)
        {
            auto& trackSequence = *midiFile.getTrack (currentTrack);

            if (trackSequence.getNumEvents() > 0)
            {
                for (int i = trackSequence.getNumEvents(); --i >= 0;)
                {
                    auto& msg = trackSequence.getEventPointer (i)->message;
                    msg.setTimeStamp (tickLen * msg.getTimeStamp());
                }

                for (int channel = 0; channel <= 16; ++channel)
                {
                    juce::MidiMessageSequence channelSequence;
                    MidiChannel midiChannel ((uint8_t) std::max (1, channel));

                    if (channel == 0)
                        trackSequence.extractSysExMessages (channelSequence);
                    else
                        trackSequence.extractMidiChannelMessages (channel, channelSequence, true);

                    if (channelSequence.getNumEvents() > 0)
                    {
                        channelSequence.sort();
                        channelSequence.updateMatchedPairs();

                        songLength = std::max (songLength, BeatDuration::fromBeats (channelSequence.getEndTime()));

                        std::unique_ptr<MidiList> midiList (new MidiList());
                        midiList->setMidiChannel (midiChannel);
                        midiList->importMidiSequence (channelSequence, nullptr, TimePosition(), nullptr);

                        if (! midiList->isEmpty())
                            lists.add (midiList.release());
                    }
                }
            }
        }
    }

    return true;
}

//==============================================================================
void MidiList::importMidiSequence (const juce::MidiMessageSequence& sequence, Edit* edit,
                                   TimePosition editTimeOfListTimeZero, juce::UndoManager* um)
{
    auto ts = edit != nullptr ? &edit->tempoSequence : nullptr;
    auto firstBeatNum = ts != nullptr ? ts->toBeats (editTimeOfListTimeZero) : BeatPosition();
    const int channelNumber = getMidiChannel().getChannelNumber();

    for (int i = 0; i < sequence.getNumEvents(); ++i)
    {
        auto& m = sequence.getEventPointer (i)->message;

        auto beatTime = ts != nullptr ? ts->toBeats (TimePosition::fromSeconds (m.getTimeStamp())) - toDuration (firstBeatNum)
                                      : BeatPosition::fromBeats (m.getTimeStamp());

        if (m.isSysEx())
        {
            importedName = TRANS("SysEx");
            addSysExEvent (m, beatTime, um);
        }
        else if (m.isTrackNameEvent())
        {
            importedName = m.getTextFromTextMetaEvent();
        }
        else if (m.isForChannel (channelNumber))
        {
            if (channelNumber == 10)
                importedName = TRANS("Drums");

            if (m.isNoteOn())
            {
                const auto keyUpTime = TimePosition::fromSeconds (sequence.getTimeOfMatchingKeyUp (i));

                addNote (m.getNoteNumber(),
                         beatTime,
                         ts != nullptr ? ((ts->toBeats (keyUpTime) - firstBeatNum) - toDuration (beatTime))
                                       : (BeatDuration::fromBeats (keyUpTime.inSeconds()) - toDuration (beatTime)),
                         m.getVelocity(), edit != nullptr ? edit->engine.getEngineBehaviour().getDefaultNoteColour() : 0, um);
            }
            else if (m.isAftertouch())
            {
                addControllerEvent (beatTime, MidiControllerEvent::aftertouchType,
                                    m.getAfterTouchValue() << 7, m.getNoteNumber(), um);
            }
            else if (m.isPitchWheel())
            {
                addControllerEvent (beatTime, MidiControllerEvent::pitchWheelType, m.getPitchWheelValue(), um);
            }
            else if (m.isController())
            {
                addControllerEvent (beatTime, m.getControllerNumber(), m.getControllerValue() << 7, um);
            }
            else if (m.isProgramChange())
            {
                if (importedName.isEmpty())
                    importedName = TRANS(juce::MidiMessage::getGMInstrumentName (m.getProgramChangeNumber()));

                addControllerEvent (beatTime, MidiControllerEvent::programChangeType, m.getProgramChangeNumber() << 7, um);
            }
            else if (m.isChannelPressure())
            {
                addControllerEvent (beatTime, MidiControllerEvent::channelPressureType, m.getChannelPressureValue() << 7, um);
            }
        }
    }

    if (importedName.isEmpty())
        importedName = TRANS("channel") + " " + juce::String (channelNumber);
}

void MidiList::importFromEditTimeSequenceWithNoteExpression (const juce::MidiMessageSequence& sequence, Edit* edit,
                                                             TimePosition editTimeOfListTimeZero, juce::UndoManager* um)
{
    auto ts = edit != nullptr ? &edit->tempoSequence : nullptr;
    auto firstBeatNum = ts != nullptr ? ts->toBeats (editTimeOfListTimeZero) : BeatPosition();
    const int channelNumber = getMidiChannel().getChannelNumber();

    juce::MPEZoneLayout layout;
    layout.setLowerZone (15);
    MPEtoNoteExpression noteQueue (*this, ts, layout, firstBeatNum, um);

    for (int i = 0; i < sequence.getNumEvents(); ++i)
    {
        auto& m = sequence.getEventPointer (i)->message;
        const auto beatTime = ts != nullptr ? ts->toBeats (TimePosition::fromSeconds (m.getTimeStamp())) - toDuration (firstBeatNum)
                                            : BeatPosition::fromBeats (m.getTimeStamp());

        if (m.isSysEx())
        {
            importedName = TRANS("SysEx");
            addSysExEvent (m, beatTime, um);
        }
        else if (m.isTrackNameEvent())
        {
            importedName = m.getTextFromTextMetaEvent();
        }
        else
        {
            if (channelNumber == 10)
                importedName = TRANS("Drums");

            if (m.isNoteOn() || m.isNoteOff() || m.isPitchWheel() || m.isChannelPressure()
                || (m.isController() && m.getControllerNumber() == 74))
            {
                noteQueue.processMidiMessage (m);
            }
            else if (m.isAftertouch())
            {
                addControllerEvent (beatTime, MidiControllerEvent::aftertouchType, m.getAfterTouchValue() << 7, m.getNoteNumber(), um);
            }
            else if (m.isPitchWheel())
            {
                addControllerEvent (beatTime, MidiControllerEvent::pitchWheelType, m.getPitchWheelValue(), um);
            }
            else if (m.isController())
            {
                addControllerEvent (beatTime, m.getControllerNumber(), m.getControllerValue() << 7, um);
            }
            else if (m.isProgramChange())
            {
                if (importedName.isEmpty())
                    importedName = TRANS(juce::MidiMessage::getGMInstrumentName (m.getProgramChangeNumber()));

                addControllerEvent (beatTime, MidiControllerEvent::programChangeType, m.getProgramChangeNumber() << 7, um);
            }
            else if (m.isChannelPressure())
            {
                addControllerEvent (beatTime, MidiControllerEvent::channelPressureType, m.getChannelPressureValue() << 7, um);
            }
        }
    }

    if (importedName.isEmpty())
        importedName = TRANS("channel") + " " + juce::String (channelNumber);
}

//==============================================================================
juce::MidiMessageSequence MidiList::exportToPlaybackMidiSequence (MidiClip& clip, TimeBase tb, bool generateMPE) const
{
    return clip.edit.engine.getEngineBehaviour().createPlaybackMidiSequence (*this, clip, tb, generateMPE);
}

juce::MidiMessageSequence MidiList::createDefaultPlaybackMidiSequence (const MidiList& list, MidiClip& clip, TimeBase timeBase, bool generateMPE)
{
    juce::MidiMessageSequence destSequence;
    
    auto& ts = clip.edit.tempoSequence;
    auto midiStartBeat = clip.getContentStartBeat();
    auto channelNumber = list.getMidiChannel().getChannelNumber();

    // NB: allow extra space here in case the notes get quantised or nudged around later on..
    const auto overlapAllowance = 0.5_bd;
    auto firstNoteBeat = timeBase == TimeBase::beatsRaw ? list.getFirstBeatNumber() - overlapAllowance
                                                        : toPosition (ts.toBeats (clip.getPosition().getStart()) - midiStartBeat - overlapAllowance);
    auto lastNoteBeat  = timeBase == TimeBase::beatsRaw ? list.getLastBeatNumber() + overlapAllowance
                                                        : toPosition (ts.toBeats (clip.getPosition().getEnd())   - midiStartBeat + overlapAllowance);

    auto& notes = list.getNotes();
    const auto numNotes = notes.size();
    auto selectedEvents = clip.getSelectedEvents();

    auto grooveTemplate = clip.edit.engine.getGrooveTemplateManager().getTemplateByName (clip.getGrooveTemplate());

    if (grooveTemplate != nullptr && grooveTemplate->isEmpty())
        grooveTemplate = nullptr;

    // Do controllers first in case they send and program or bank change messages
    auto& controllerEvents = list.getControllerEvents();

    {
        // Add cumulative controller events that are off the start
        juce::Array<int> doneControllers;

        for (auto e : controllerEvents)
        {
            auto beat = e->getBeatPosition();

            if (beat < firstNoteBeat)
            {
                if (! doneControllers.contains (e->getType()))
                {
                    addToSequence (destSequence, clip, timeBase, *e, channelNumber);
                    doneControllers.add (e->getType());
                }
            }
        }
    }

    // Add the real controller events:
    for (auto e : controllerEvents)
    {
        auto beat = e->getBeatPosition();

        if (beat >= firstNoteBeat && beat < lastNoteBeat)
            addToSequence (destSequence, clip, timeBase, *e, channelNumber);
    }

    // Then the note events
    if (! generateMPE)
    {
        for (int i = 0; i < numNotes; ++i)
        {
            auto& note = *notes.getUnchecked (i);

            if (selectedEvents != nullptr && ! selectedEvents->isSelected (&note))
                continue;

            // check for subsequent overlaps
            auto thisNoteStart = note.getStartBeat();

            if (thisNoteStart >= lastNoteBeat)
                break;

            auto thisNoteEnd = note.getEndBeat();
            auto noteNum = note.getNoteNumber();
            bool useNoteUp = true;

            for (int j = i + 1; j < numNotes; ++j)
            {
                const auto& note2 = *notes.getUnchecked (j);
                const auto s = note2.getStartBeat();

                if (s >= lastNoteBeat || s >= thisNoteEnd)
                    break;

                if (note2.getNoteNumber() == noteNum)
                {
                    useNoteUp = false;
                    break;
                }
            }

            if (thisNoteEnd > firstNoteBeat)
                addToSequence (destSequence, clip, timeBase, note, channelNumber, useNoteUp, grooveTemplate);
        }
    }
    else
    {
        MPEChannelAssigner assigner (destSequence, clip, timeBase, grooveTemplate);

        for (auto note : notes)
        {
            if (selectedEvents != nullptr && ! selectedEvents->isSelected (note))
                continue;

            assigner.addNote (*note);
        }
    }

    // Add the SysEx events:
    for (auto e : list.getSysexEvents())
    {
        auto beat = e->getBeatPosition();

        if (beat >= firstNoteBeat && beat < lastNoteBeat)
            addToSequence (destSequence, clip, timeBase, *e);
    }
    
    return destSequence;
}

}} // namespace tracktion { inline namespace engine
