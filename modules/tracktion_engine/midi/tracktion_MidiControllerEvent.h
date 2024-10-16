/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

class MidiControllerEvent
{
public:
    static juce::ValueTree createControllerEvent (BeatPosition, int controllerType, int controllerValue);
    static juce::ValueTree createControllerEvent (BeatPosition, int controllerType, int controllerValue, int metadata);
    static juce::ValueTree createControllerEvent (const MidiControllerEvent&, BeatPosition);

    MidiControllerEvent (const juce::ValueTree&);
    MidiControllerEvent (MidiControllerEvent&&) = default;

    //==============================================================================
    int getControllerValue() const noexcept                         { return value; }
    void setControllerValue (int newValue, juce::UndoManager*);

    static int getControllerDefautValue (int type);

    int getMetadata() const noexcept                                { return metadata; }
    void setMetadata (int metaValue, juce::UndoManager*);

    //==============================================================================
    BeatPosition getBeatPosition() const noexcept                         { return beatNumber; }
    void setBeatPosition (BeatPosition, juce::UndoManager*);

    /** This takes into account quantising, groove templates, clip offset, etc */
    BeatPosition getEditBeats (const MidiClip&) const;
    TimePosition getEditTime (const MidiClip&) const;

    juce::String getLevelDescription (MidiClip*) const;

    //==============================================================================
    /** Controller type, from the list below */
    int getType() const noexcept                { return type; }
    void setType (int type, juce::UndoManager* um);

    enum ExtraControllerTypes
    {
        programChangeType   = 0x1001,
        noteVelocities      = 0x1003,
        aftertouchType      = 0x1004,
        pitchWheelType      = 0x1005,
        sysExType           = 0x1006, //< Really just here for MidiStrip editing
        channelPressureType = 0x1007,

        UnknownType         = 0x7fffffff
    };

    static juce::String getControllerTypeName (int type) noexcept;

    static int bankIDToCoarse (int id)          { return (id & 0xff00) >> 8; }
    static int bankIDToFine (int id)            { return id & 0xff; }

    juce::ValueTree state;

private:
    //==============================================================================
    friend class MidiList;

    BeatPosition beatNumber;
    int type, value, metadata;

    void updatePropertiesFromState() noexcept;

    MidiControllerEvent() = delete;
    MidiControllerEvent (const MidiControllerEvent&) = delete;

    JUCE_LEAK_DETECTOR (MidiControllerEvent)
};

}} // namespace tracktion { inline namespace engine
