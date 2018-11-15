/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


RemoteSLCompact::RemoteSLCompact (ExternalControllerManager& ecm)  : ControlSurface (ecm)
{
    deviceDescription               = "Novation ReMOTE SL Compact";
    needsMidiChannel                = true;
    needsMidiBackChannel            = true;
    wantsClock                      = false;
    deletable                       = false;
    numberOfFaderChannels           = 0;
    numCharactersForTrackNames      = 8;
    numParameterControls            = 8;
    numCharactersForParameterLabels = 8;
    numMarkers                      = 0;
    numCharactersForMarkerLabels    = 0;
    numAuxes                        = 0;
    numCharactersForAuxLabels       = 0;
    wantsAuxBanks                   = false;
    followsTrackSelection           = false;
}

RemoteSLCompact::~RemoteSLCompact()
{
    notifyListenersOfDeletion();
}

void RemoteSLCompact::initialiseDevice (bool) {}
void RemoteSLCompact::shutDownDevice() {}
void RemoteSLCompact::updateMiscFeatures() {}
void RemoteSLCompact::acceptMidiMessage (const MidiMessage&) {}
void RemoteSLCompact::moveFader (int, float) {}
void RemoteSLCompact::moveMasterLevelFader (float, float) {}
void RemoteSLCompact::movePanPot (int, float) {}
void RemoteSLCompact::moveAux (int, const char*, float) {}
void RemoteSLCompact::clearAux (int) {}
void RemoteSLCompact::soloCountChanged (bool) {}
void RemoteSLCompact::updateSoloAndMute (int, Track::MuteAndSoloLightState, bool) {}
void RemoteSLCompact::playStateChanged (bool) {}
void RemoteSLCompact::recordStateChanged (bool) {}
void RemoteSLCompact::automationReadModeChanged (bool) {}
void RemoteSLCompact::automationWriteModeChanged (bool) {}
void RemoteSLCompact::faderBankChanged (int, const StringArray&) {}
void RemoteSLCompact::channelLevelChanged (int, float) {}
void RemoteSLCompact::trackSelectionChanged (int, bool) {}
void RemoteSLCompact::trackRecordEnabled (int, bool) {}
void RemoteSLCompact::masterLevelsChanged(float, float) {}
void RemoteSLCompact::timecodeChanged (int, int, int, int, bool, bool) {}
void RemoteSLCompact::clickOnOffChanged (bool) {}
void RemoteSLCompact::snapOnOffChanged (bool) {}
void RemoteSLCompact::loopOnOffChanged (bool) {}
void RemoteSLCompact::slaveOnOffChanged (bool) {}
void RemoteSLCompact::punchOnOffChanged (bool) {}
void RemoteSLCompact::undoStatusChanged (bool, bool) {}
void RemoteSLCompact::parameterChanged (int, const ParameterSetting&) {}
void RemoteSLCompact::clearParameter (int) {}
void RemoteSLCompact::markerChanged (int, const MarkerSetting&) {}
void RemoteSLCompact::clearMarker (int) {}
void RemoteSLCompact::auxBankChanged (int) {}

bool RemoteSLCompact::eatsAllMessages()                 { return false; }
bool RemoteSLCompact::canSetEatsAllMessages()           { return false; }
void RemoteSLCompact::setEatsAllMessages (bool)         {}
bool RemoteSLCompact::canChangeSelectedPlugin()         { return true; }
void RemoteSLCompact::currentSelectionChanged()         {}
bool RemoteSLCompact::showingPluginParams()             { return true; }
bool RemoteSLCompact::showingMarkers()                  { return false; }
bool RemoteSLCompact::showingTracks()                   { return false; }
void RemoteSLCompact::pluginBypass (bool)               {}
bool RemoteSLCompact::isPluginSelected (Plugin*)        { return false; }

bool RemoteSLCompact::wantsMessage (const MidiMessage& m)
{
    return m.isController();
}
