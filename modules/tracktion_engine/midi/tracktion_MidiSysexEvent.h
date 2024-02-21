/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
*/

namespace tracktion { inline namespace engine
{

class MidiSysexEvent
{
public:
    static juce::ValueTree createSysexEvent (const MidiSysexEvent&, BeatPosition);
    static juce::ValueTree createSysexEvent (const juce::MidiMessage&, BeatPosition);

    MidiSysexEvent (const juce::ValueTree&);
    MidiSysexEvent (const juce::MidiMessage&);
    MidiSysexEvent (const juce::MidiMessage&, BeatPosition);
    ~MidiSysexEvent() noexcept {}

    //==============================================================================
    const juce::MidiMessage& getMessage() const noexcept            { return message; }
    void setMessage (const juce::MidiMessage&, juce::UndoManager*);

    //==============================================================================
    BeatPosition getBeatPosition() const noexcept                         { return BeatPosition::fromBeats (message.getTimeStamp()); }
    void setBeatPosition (BeatPosition, juce::UndoManager*);

    // takes into account quantising, groove templates, clip offset, etc
    TimePosition getEditTime (const MidiClip&) const;
    BeatPosition getEditBeats (const MidiClip&) const;

    juce::ValueTree state;

private:
    //==============================================================================
    friend class MidiList;
    juce::MidiMessage message;

    void updateMessage();
    void updateTime();

    MidiSysexEvent() = delete;
    MidiSysexEvent (const MidiSysexEvent&) = delete;

    JUCE_LEAK_DETECTOR (MidiSysexEvent)
};

}} // namespace tracktion { inline namespace engine
