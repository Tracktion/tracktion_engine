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

RemoteSLCompact::RemoteSLCompact (ExternalControllerManager& ecm)  : NovationRemoteSl (ecm)
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

bool RemoteSLCompact::eatsAllMessages()                                 { return false; }
bool RemoteSLCompact::canSetEatsAllMessages()                           { return false; }
bool RemoteSLCompact::canChangeSelectedPlugin()                         { return true; }
bool RemoteSLCompact::showingPluginParams()                             { return true; }
bool RemoteSLCompact::showingMarkers()                                  { return false; }
bool RemoteSLCompact::showingTracks()                                   { return false; }
bool RemoteSLCompact::isPluginSelected (Plugin*)                        { return false; }
bool RemoteSLCompact::wantsMessage (int, const juce::MidiMessage& m)    { return m.isController(); }

}} // namespace tracktion { inline namespace engine
