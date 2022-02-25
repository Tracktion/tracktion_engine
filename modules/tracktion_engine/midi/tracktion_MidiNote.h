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

class MidiNote
{
public:
    enum NoteEdge
    {
        startEdge,
        endEdge
    };

    static juce::ValueTree createNote (const MidiNote&, BeatPosition newStart, BeatDuration newLength);

    MidiNote (const juce::ValueTree&);
    MidiNote (MidiNote&&) = default;

    bool operator== (const MidiNote& other) const noexcept  { return state == other.state; }
    bool operator!= (const MidiNote& other) const noexcept  { return state != other.state; }

    //==============================================================================
    /** Start pos, in beats, from the start of the clip */
    BeatPosition getBeatPosition() const noexcept           { return startBeat; }
    BeatPosition getStartBeat() const noexcept              { return startBeat; }
    BeatPosition getEndBeat() const noexcept                { return startBeat + lengthInBeats; }
    BeatDuration getLengthBeats() const noexcept            { return lengthInBeats; }
    BeatRange getRangeBeats() const noexcept                { return { getStartBeat(), getEndBeat() }; }

    TimePosition getPlaybackTime (NoteEdge, const MidiClip&, const GrooveTemplate*) const;
    BeatPosition getPlaybackBeats (NoteEdge, const MidiClip&, const GrooveTemplate*) const;

    /** Returns the start, quantised according to the clip's settings. */
    BeatPosition getQuantisedStartBeat (const MidiClip&) const;
    BeatPosition getQuantisedStartBeat (const MidiClip*) const;

    BeatPosition getQuantisedEndBeat (const MidiClip&) const;
    BeatPosition getQuantisedEndBeat (const MidiClip*) const;

    BeatDuration getQuantisedLengthBeats (const MidiClip&) const;
    BeatDuration getQuantisedLengthBeats (const MidiClip*) const;

    /** Gets the start of this note in terms of edit time, taking into account quantising,
        groove templates, clip offset, etc...
    */
    TimePosition getEditStartTime (const MidiClip&) const;
    TimePosition getEditEndTime (const MidiClip&) const;

    TimeDuration getLengthSeconds (const MidiClip&) const;
    TimeRange getEditTimeRange (const MidiClip&) const;

    void setStartAndLength (BeatPosition newStartBeat, BeatDuration newLengthInBeats, juce::UndoManager*);

    int getNoteNumber() const noexcept              { return (int) noteNumber; }
    void setNoteNumber (int newNoteNumber, juce::UndoManager*);

    int getVelocity() const noexcept                { return (int) velocity; }
    void setVelocity (int newVelocity, juce::UndoManager*);

    int getColour() const noexcept                  { return (int) colour; }
    void setColour (int newColourIndex, juce::UndoManager*);

    bool isMute() const noexcept                    { return mute != 0; }
    void setMute (bool shouldMute, juce::UndoManager*);

    //==============================================================================
    juce::ValueTree state;

private:
    //==============================================================================
    friend class MidiList;
    friend struct NoteChannelAssigner;

    BeatPosition startBeat;
    BeatDuration lengthInBeats;
    uint8_t noteNumber, velocity, colour, mute;

    void updatePropertiesFromState();

    MidiNote() = delete;
    MidiNote (const MidiNote&) = delete;

    JUCE_LEAK_DETECTOR (MidiNote)
};

}} // namespace tracktion { inline namespace engine
