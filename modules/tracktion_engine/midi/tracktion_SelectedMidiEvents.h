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

/** Represents a set of selected MIDI notes.
    This will broadcast a change message when the events that are selected are changed.
*/
class SelectedMidiEvents   : public Selectable,
                             public juce::ChangeBroadcaster
{
public:
    SelectedMidiEvents (MidiClip&);
    SelectedMidiEvents (juce::Array<MidiClip*>);
    ~SelectedMidiEvents() override;

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
    void setSelected (SelectionManager&, const juce::Array<MidiNote*>&, bool addToSelection, bool allowMixedSelection = false);
    void setSelected (SelectionManager&, const juce::Array<MidiSysexEvent*>&, bool addToSelection, bool allowMixedSelection = false);
    void setSelected (SelectionManager&, const juce::Array<MidiControllerEvent*>&, bool addToSelection, bool allowMixedSelection = false);
    bool isSelected (const MidiNote*) const;
    bool isSelected (const MidiSysexEvent*) const;
    bool isSelected (const MidiControllerEvent*) const;

    int getNumSelected() const      { return selectedNotes.size() + selectedSysexes.size() + selectedControllers.size(); }
    bool anythingSelected() const   { return getNumSelected() != 0; }

    const juce::Array<MidiNote*>& getSelectedNotes() const noexcept                     { return selectedNotes; }
    const juce::Array<MidiSysexEvent*>& getSelectedSysexes() const noexcept             { return selectedSysexes; }
    const juce::Array<MidiControllerEvent*>& getSelectedControllers() const noexcept    { return selectedControllers; }

    //==============================================================================
    void moveEvents (TimeDuration deltaStart, TimeDuration deltaLength, int deltaNote);
    void setNoteLengths (BeatDuration newLength);
    void setVelocities (int newVelocity);
    void changeColour (uint8_t newColour);

    void nudge (TimecodeSnapType, int leftRight, int upDown);

    TimeRange getSelectedRange() const;

    //==============================================================================
    juce::String getSelectableDescription() override;
    void selectionStatusChanged (bool isNowSelected) override;

    const juce::Array<MidiClip*>& getClips()                                            { return clips; }

    void setClips (juce::Array<MidiClip*> clips);

    /** Moves all controller data in 'clips' between edit times. Optionally moves the data making
        a copy at the original location
     */
    static void moveControllerData (const juce::Array<MidiClip*>& clips, const juce::Array<MidiControllerEvent*>* onlyTheseEvents,
                                    BeatDuration deltaBeats, TimePosition startTime, TimePosition endTime, bool makeCopy);

    /** Host should set this callback to specify if it wants MIDI CC locked to MIDI notes when
        nudging
     */
    std::function<bool()> shouldLockControllerToNotes;

private:
    juce::Array<MidiClip*> clips;

    juce::Array<MidiNote*> selectedNotes;
    juce::Array<MidiSysexEvent*> selectedSysexes;
    juce::Array<MidiControllerEvent*> selectedControllers;

    void sendSelectionChangedMessage (SelectionManager*);

    bool contains (const MidiNote&) const noexcept;
    bool contains (const MidiSysexEvent&) const noexcept;
    bool contains (const MidiControllerEvent&) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectedMidiEvents)
};

}} // namespace tracktion { inline namespace engine
