/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class MidiSysexEvent
{
public:
    static juce::ValueTree createSysexEvent (const MidiSysexEvent&, double time);
    static juce::ValueTree createSysexEvent (const juce::MidiMessage&, double time);

    MidiSysexEvent (const juce::ValueTree&);
    MidiSysexEvent (const juce::MidiMessage&);
    MidiSysexEvent (const juce::MidiMessage&, double beatNumber);
    ~MidiSysexEvent() noexcept {}

    //==============================================================================
    const juce::MidiMessage& getMessage() const noexcept            { return message; }
    void setMessage (const juce::MidiMessage&, juce::UndoManager*);

    //==============================================================================
    double getBeatPosition() const noexcept                         { return message.getTimeStamp(); }
    void setBeatPosition (double beatNumber, juce::UndoManager*);

    // takes into account quantising, groove templates, clip offset, etc
    double getEditTime (const MidiClip&) const;

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

} // namespace tracktion_engine
