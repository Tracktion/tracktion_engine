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

/**
*/
class MidiList
{
public:
    MidiList();
    MidiList (const juce::ValueTree&, juce::UndoManager*);
    ~MidiList();

    static juce::ValueTree createMidiList();

    /** Clears the current list and copies the others contents and properties. */
    void copyFrom (const MidiList&, juce::UndoManager*);

    /** Adds copies of the events in another list to this one. */
    void addFrom (const MidiList&, juce::UndoManager*);

    //==============================================================================
    enum class NoteAutomationType
    {
        none,       /**< No automation, add the sequence as plain MIDI with the channel of the clip. */
        expression  /**< Add the automation as EXP assuming the source sequence is MPE MIDI. */
    };

    //==============================================================================
    const juce::Array<MidiNote*>& getNotes() const;
    const juce::Array<MidiControllerEvent*>& getControllerEvents() const;
    const juce::Array<MidiSysexEvent*>& getSysexEvents() const;

    //==============================================================================
    bool isAttachedToClip() const noexcept                          { return ! state.getParent().hasType (IDs::NA); }

    void setCompList (bool shouldBeComp) noexcept                   { isComp = shouldBeComp; }
    bool isCompList() const noexcept                                { return isComp; }

    //==============================================================================
    /** Gets the list's midi channel number. Value is 1 to 16. */
    MidiChannel getMidiChannel() const                              { return midiChannel; }

    /** Gives the list a channel number that it'll use when generating real midi messages. Value is 1 to 16. */
    void setMidiChannel (MidiChannel chanNum);

    /** If the data was pulled from a midi file then this may have a useful name describing its purpose. */
    juce::String getImportedMidiTrackName() const noexcept          { return importedName; }

    //==============================================================================
    bool isEmpty() const noexcept                                   { return state.getNumChildren() == 0; }

    void clear (juce::UndoManager*);
    void trimOutside (BeatPosition firstBeat, BeatPosition lastBeat, juce::UndoManager*);
    void moveAllBeatPositions (BeatDuration deltaBeats, juce::UndoManager*);
    void rescale (double factor, juce::UndoManager*);

    //==============================================================================
    int getNumNotes() const                                         { return getNotes().size(); }
    MidiNote* getNote (int index) const                             { return getNotes()[index]; }
    MidiNote* getNoteFor (const juce::ValueTree&);

    juce::Range<int> getNoteNumberRange() const;

    /** Beat number of first event in the list */
    BeatPosition getFirstBeatNumber() const;

    /** Beat number of last event in the list */
    BeatPosition getLastBeatNumber() const;

    MidiNote* addNote (const MidiNote&, juce::UndoManager*);
    MidiNote* addNote (int pitch, BeatPosition startBeat, BeatDuration lengthInBeats, int velocity, int colourIndex, juce::UndoManager*);
    void removeNote (MidiNote&, juce::UndoManager*);
    void removeAllNotes (juce::UndoManager*);

    //==============================================================================
    int getNumControllerEvents() const                              { return getControllerEvents().size(); }

    MidiControllerEvent* getControllerEvent (int index) const       { return getControllerEvents()[index]; }
    MidiControllerEvent* getControllerEventAt (BeatPosition, int controllerType) const;

    MidiControllerEvent* addControllerEvent (const MidiControllerEvent&, juce::UndoManager*);
    MidiControllerEvent* addControllerEvent (BeatPosition, int controllerType, int controllerValue, juce::UndoManager*);
    MidiControllerEvent* addControllerEvent (BeatPosition, int controllerType, int controllerValue, int metadata, juce::UndoManager*);

    void removeControllerEvent (MidiControllerEvent&, juce::UndoManager*);
    void removeAllControllers (juce::UndoManager*);

    /** True if there are any controller events of this type */
    bool containsController (int controllerType) const;

    void setControllerValueAt (int controllerType, BeatPosition beatNumber, int newValue, juce::UndoManager*);
    void removeControllersBetween (int controllerType, BeatPosition beatNumberStart, BeatPosition beatNumberEnd, juce::UndoManager*);

    /** Adds controller values over a specified time, at an even interval */
    void insertRepeatedControllerValue (int type, int startVal, int endVal,
                                        BeatRange rangeBeats,
                                        BeatDuration intervalBeats, juce::UndoManager*);

    //==============================================================================
    int getNumSysExEvents() const                                   { return getSysexEvents().size(); }

    MidiSysexEvent* getSysexEvent (int index) const                 { return getSysexEvents()[index]; }
    MidiSysexEvent* getSysexEventUnchecked (int index) const        { return getSysexEvents().getUnchecked (index); }
    MidiSysexEvent* getSysexEventFor (const juce::ValueTree&) const;

    MidiSysexEvent& addSysExEvent (const juce::MidiMessage&, BeatPosition, juce::UndoManager*);

    void removeSysExEvent (const MidiSysexEvent&, juce::UndoManager*);
    void removeAllSysexes (juce::UndoManager*);

    //==============================================================================
    /** Adds the contents of a MidiMessageSequence to this list.
        If an Edit is provided, it'll be used to convert the timestamps from possible seconds to beats
    */
    void importMidiSequence (const juce::MidiMessageSequence&, Edit*,
                             TimePosition editTimeOfListTimeZero, juce::UndoManager*);

    /** Adds the contents of a MidiSequence to this list assigning MPE expression changes to EXP expression. */
    void importFromEditTimeSequenceWithNoteExpression (const juce::MidiMessageSequence&, Edit*,
                                                       TimePosition editTimeOfListTimeZero, juce::UndoManager*);

    /** Determines MIDI event timing. */
    enum class TimeBase
    {
        seconds,    /** Event times will be in seconds relative to the Edit timeline. */
        beats,      /** Event times will be in beats relative to the Edit timeline. */
        beatsRaw    /** Event times will be in beats with no quantisation or groove. */
    };

    /** Creates a juce::MidiMessageSequence from the list in order to be played back
        The sequence will be in terms of edit time, either in seconds or beats
        @param MidiClip     The clip boundries to use and the groove template if the clip contains one
        @param TimeBase     The format the exported MIDI event times will be in
        @param generateMPE  Whether the sequence should create MPE or standard MIDI
    */
    juce::MidiMessageSequence exportToPlaybackMidiSequence (MidiClip&, TimeBase, bool generateMPE) const;

    /** Creates the default MIDI playback sequence. */
    static juce::MidiMessageSequence createDefaultPlaybackMidiSequence (const MidiList&, MidiClip&, TimeBase, bool generateMPE);

    //==============================================================================
    static bool looksLikeMPEData (const juce::File&);

    static constexpr const double defaultInitialTimbreValue = 0.5;
    static constexpr const double defaultInitialPitchBendValue = 0;
    static constexpr const double defaultInitialPressureValue = 0;

    static bool fileHasTempoChanges (const juce::File&);

    static bool readSeparateTracksFromFile (const juce::File&,
                                            juce::OwnedArray<MidiList>& lists,
                                            juce::Array<BeatPosition>& tempoChangeBeatNumbers,
                                            juce::Array<double>& bpms,
                                            juce::Array<int>& numerators,
                                            juce::Array<int>& denominators,
                                            BeatDuration& songLength,
                                            bool importAsNoteExpression);

    //==============================================================================
    template <typename Type>
    static void sortMidiEventsByTime (juce::Array<Type>& notes)
    {
        std::sort (notes.begin(), notes.end(),
                   [] (const Type& a, const Type& b) { return a->getBeatPosition() < b->getBeatPosition(); });
    }

    template <typename Type>
    static void sortMidiEventsByNoteNumber (juce::Array<Type>& notes)
    {
        std::sort (notes.begin(), notes.end(),
                   [] (const Type& a, const Type& b) { return a->getNoteNumber() < b->getNoteNumber(); });
    }

    //==============================================================================
    juce::ValueTree state;

private:
    //==============================================================================
    juce::CachedValue<MidiChannel> midiChannel;
    juce::CachedValue<bool> isComp;

    juce::String importedName;

    void initialise (juce::UndoManager*);

    template<typename EventType>
    struct EventDelegate
    {
        static bool isSuitableType (const juce::ValueTree&);
        /** Return true if the order may have changed. */
        static bool updateObject (EventType&, const juce::Identifier&);
        static void removeFromSelection (EventType*);
    };

    template<typename EventType>
    struct EventList : public ValueTreeObjectList<EventType>
    {
        EventList (const juce::ValueTree& v)
            : ValueTreeObjectList<EventType> (v)
        {
            ValueTreeObjectList<EventType>::rebuildObjects();
        }

        ~EventList() override
        {
            ValueTreeObjectList<EventType>::freeObjects();
        }

        EventType* getEventFor (const juce::ValueTree& v)
        {
            for (auto m : ValueTreeObjectList<EventType>::objects)
                if (m->state == v)
                    return m;

            return {};
        }

        bool isSuitableType (const juce::ValueTree& v) const override   { return EventDelegate<EventType>::isSuitableType (v); }
        EventType* createNewObject (const juce::ValueTree& v) override  { return new EventType (v); }
        void deleteObject (EventType* m) override                       { delete m; }
        void newObjectAdded (EventType*) override                       { triggerSort(); }
        void objectRemoved (EventType* m) override                      { EventDelegate<EventType>::removeFromSelection (m); triggerSort(); }
        void objectOrderChanged() override                              { triggerSort(); }

        void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i) override
        {
            if (auto e = getEventFor (v))
                if (EventDelegate<EventType>::updateObject (*e, i))
                    triggerSort();
        }

        void triggerSort()
        {
            const juce::ScopedLock sl (lock);
            needsSorting = true;
        }

        const juce::Array<EventType*>& getSortedList()
        {
            TRACKTION_ASSERT_MESSAGE_THREAD

            const juce::ScopedLock sl (lock);

            if (needsSorting)
            {
                needsSorting = false;
                sortedEvents = ValueTreeObjectList<EventType>::objects;
                sortMidiEventsByTime (sortedEvents);
            }

            return sortedEvents;
        }

        bool needsSorting = true;
        juce::Array<EventType*> sortedEvents;
        juce::CriticalSection lock;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EventList)
    };

    std::unique_ptr<EventList<MidiNote>> noteList;
    std::unique_ptr<EventList<MidiControllerEvent>> controllerList;
    std::unique_ptr<EventList<MidiSysexEvent>> sysexList;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiList)
};

}} // namespace tracktion { inline namespace engine
