/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


MackieXT::MackieXT (ExternalControllerManager& ecm, MackieMCU& m, int id)
    : ControlSurface (ecm), mcu (m)
{
    deviceDescription = "Mackie Control Universal XT #" + String (id + 1);

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
    uint8 d[8] = { 0xf0, 0x00, 0x00, 0x66, 0x15, 0x08, 0x00, 0xf7 };
    sendMidiArray (d);
}

void MackieXT::acceptMidiMessage (const MidiMessage& m)
{
    mcu.acceptMidiMessage (deviceIdx, m);
}

void MackieXT::updateMiscFeatures() {}
void MackieXT::moveFader (int, float) {}
void MackieXT::moveMasterLevelFader (float, float) {}
void MackieXT::movePanPot (int, float) {}
void MackieXT::moveAux (int, const char*, float) {}
void MackieXT::updateSoloAndMute (int, Track::MuteAndSoloLightState, bool) {}
void MackieXT::soloCountChanged (bool) {}
void MackieXT::playStateChanged (bool) {}
void MackieXT::recordStateChanged (bool) {}
void MackieXT::automationReadModeChanged (bool) {}
void MackieXT::automationWriteModeChanged (bool) {}
void MackieXT::faderBankChanged (int, const StringArray&) {}
void MackieXT::channelLevelChanged (int, float) {}
void MackieXT::trackSelectionChanged (int, bool) {}
void MackieXT::trackRecordEnabled (int, bool) {}
void MackieXT::masterLevelsChanged (float, float) {}
void MackieXT::timecodeChanged (int, int, int, int, bool, bool) {}
void MackieXT::clickOnOffChanged (bool) {}
void MackieXT::snapOnOffChanged (bool) {}
void MackieXT::loopOnOffChanged (bool) {}
void MackieXT::slaveOnOffChanged (bool) {}
void MackieXT::punchOnOffChanged (bool) {}
void MackieXT::parameterChanged (int, const ParameterSetting&) {}
void MackieXT::clearParameter (int) {}
void MackieXT::markerChanged (int, const MarkerSetting&) {}
void MackieXT::clearMarker (int) {}
