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

MasterTrack::MasterTrack (Edit& e, const juce::ValueTree& v)
    : Track (e, v, true)
{
}

void MasterTrack::initialise()
{
    Track::initialise();

    pluginList.initialise (edit.getMasterPluginList().state);
}

MasterTrack::~MasterTrack()
{
    notifyListenersOfDeletion();
}

bool MasterTrack::isMasterTrack() const               { return true; }
juce::String MasterTrack::getName() const             { return TRANS("Master"); }
juce::String MasterTrack::getSelectableDescription()  { return TRANS("Master Track"); }
bool MasterTrack::canContainPlugin (Plugin* p) const  { return p->canBeAddedToMaster(); }

}} // namespace tracktion { inline namespace engine
