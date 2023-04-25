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

MackieXT::MackieXT (ExternalControllerManager& ecm, MackieMCU& m, int id)
    : ControlSurface (ecm), mcu (m)
{
    deviceDescription = m.deviceDescription + " XT #" + juce::String (id + 1);

    needsMidiChannel = true;
    needsMidiBackChannel = true;
    wantsClock = false;
    deletable = false;
    numberOfFaderChannels = 0;
    numCharactersForTrackNames = 0;
    numParameterControls = 0;
    numCharactersForParameterLabels = 0;
    numMarkers = 0;
    numCharactersForMarkerLabels = 0;

    mcu.registerXT (this);
}

MackieXT::~MackieXT()
{
    notifyListenersOfDeletion();
    mcu.unregisterXT (this);
}

void MackieXT::initialiseDevice (bool)
{
    CRASH_TRACER

    for (int i = 0; i < 0x20; ++i)
        mcu.lightUpButton (deviceIdx, i, false);

    mcu.setDisplay (deviceIdx, "            - Mackie Control Universal XT - ", "");

    mcu.setSignalMetersEnabled (deviceIdx, true);
}

void MackieXT::shutDownDevice()
{
    // send a reset message:
    uint8_t d[8] = { 0xf0, 0x00, 0x00, 0x66, 0x15, 0x08, 0x00, 0xf7 };
    sendMidiArray (0, d);
}

void MackieXT::acceptMidiMessage (int, const juce::MidiMessage& m)
{
    mcu.acceptMidiMessageInt (deviceIdx, m);
}

}} // namespace tracktion { inline namespace engine
