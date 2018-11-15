/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class MidiNote
{
public:
    enum NoteEdge
    {
        startEdge,
        endEdge
    };

    static juce::ValueTree createNote (const MidiNote&, double newStart, double newLength);

    MidiNote (const juce::ValueTree&);
    MidiNote (MidiNote&&) = default;

    bool operator== (const MidiNote& other) const noexcept  { return state == other.state; }
    bool operator!= (const MidiNote& other) const noexcept  { return state != other.state; }

    //==============================================================================
    /** Start pos, in beats, from the start of the clip */
    double getBeatPosition() const noexcept             { return startBeat; }
    double getStartBeat() const noexcept                { return startBeat; }
    double getEndBeat() const noexcept                  { return startBeat + lengthInBeats; }
    double getLengthBeats() const noexcept              { return lengthInBeats; }
    juce::Range<double> getRangeBeats() const noexcept  { return { getStartBeat(), getEndBeat() }; }

    double getPlaybackTime (NoteEdge, const MidiClip&, const GrooveTemplate*) const;

    /** Returns the start, quantised according to the clip's settings. */
    double getQuantisedStartBeat (const MidiClip&) const;
    double getQuantisedStartBeat (const MidiClip*) const;

    double getQuantisedEndBeat (const MidiClip&) const;
    double getQuantisedEndBeat (const MidiClip*) const;

    double getQuantisedLengthBeats (const MidiClip&) const;
    double getQuantisedLengthBeats (const MidiClip*) const;

    /** Gets the start of this note in terms of edit time, taking into account quantising,
        groove templates, clip offset, etc...
    */
    double getEditStartTime (const MidiClip&) const;
    double getEditEndTime (const MidiClip&) const;

    double getLengthSeconds (const MidiClip&) const;
    EditTimeRange getEditTimeRange (const MidiClip&) const;

    void setStartAndLength (double newStartBeat, double newLengthInBeats, juce::UndoManager*);

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

    double startBeat, lengthInBeats;
    juce::uint8 noteNumber, velocity, colour, mute;

    void updatePropertiesFromState();

    MidiNote() = delete;
    MidiNote (const MidiNote&) = delete;

    JUCE_LEAK_DETECTOR (MidiNote)
};

} // namespace tracktion_engine
