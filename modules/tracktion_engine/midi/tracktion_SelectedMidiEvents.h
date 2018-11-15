/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** Represents a set of selected MIDI notes.
    This will broadcast a change message when the events that are selected are changed.
*/
class SelectedMidiEvents   : public Selectable,
                             public juce::ChangeBroadcaster
{
public:
    SelectedMidiEvents (MidiClip&);
    SelectedMidiEvents (juce::Array<MidiClip*>);
    ~SelectedMidiEvents();

    Edit& getEdit();

    //==============================================================================
    MidiClip* clipForEvent (MidiNote*) const;
    MidiClip* clipForEvent (MidiSysexEvent*) const;
    MidiClip* clipForEvent (MidiControllerEvent*) const;

    //==============================================================================
    void addSelectedEvent (MidiNote*, bool addToCurrent);
    void addSelectedEvent (MidiSysexEvent*, bool addToCurrent);
    void addSelectedEvent (MidiControllerEvent*, bool addToCurrent);
    void removeSelectedEvent (MidiNote*);
    void removeSelectedEvent (MidiSysexEvent*);
    void removeSelectedEvent (MidiControllerEvent*);
    void setSelected (SelectionManager&, const juce::Array<MidiNote*>&, bool addToSelection);
    void setSelected (SelectionManager&, const juce::Array<MidiSysexEvent*>&, bool addToSelection);
    void setSelected (SelectionManager&, const juce::Array<MidiControllerEvent*>&, bool addToSelection);
    bool isSelected (const MidiNote*) const;
    bool isSelected (const MidiSysexEvent*) const;
    bool isSelected (const MidiControllerEvent*) const;

    int getNumSelected() const      { return selectedNotes.size() + selectedSysexes.size() + selectedControllers.size(); }
    bool anythingSelected() const   { return getNumSelected() != 0; }

    const juce::Array<MidiNote*>& getSelectedNotes() const noexcept                     { return selectedNotes; }
    const juce::Array<MidiSysexEvent*>& getSelectedSysexes() const noexcept             { return selectedSysexes; }
    const juce::Array<MidiControllerEvent*>& getSelectedControllers() const noexcept    { return selectedControllers; }

    //==============================================================================
    void moveNotes (double deltaStart, double deltaLength, int deltaNote);
    void setNoteLengths (double newLength);
    void setVelocities (int newVelocity);
    void changeColour (juce::uint8 newColour);

    void nudge (TimecodeSnapType, int leftRight, int upDown);

    EditTimeRange getSelectedRange() const;

    //==============================================================================
    juce::String getSelectableDescription() override;
    void selectionStatusChanged (bool isNowSelected) override;

    juce::Array<MidiClip*> clips;

private:
    juce::Array<MidiNote*> selectedNotes;
    juce::Array<MidiSysexEvent*> selectedSysexes;
    juce::Array<MidiControllerEvent*> selectedControllers;

    MidiNote* lastNoteForNoteAutomation = nullptr;

    void sendSelectionChangedMessage (SelectionManager*);

    void legatoNote (MidiNote*, juce::Array<MidiNote*>& notesToUse,
                     double maxEndBeat, juce::UndoManager&);

    bool contains (const MidiNote&) const noexcept;
    bool contains (const MidiSysexEvent&) const noexcept;
    bool contains (const MidiControllerEvent&) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectedMidiEvents)
};

} // namespace tracktion_engine
