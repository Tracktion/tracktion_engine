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

MarkerTrack::MarkerTrack (Edit& e, const juce::ValueTree& v)
    : ClipTrack (e, v, false)
{
}

MarkerTrack::~MarkerTrack()
{
    notifyListenersOfDeletion();
}

bool MarkerTrack::isMarkerTrack() const                  { return true; }
juce::String MarkerTrack::getSelectableDescription()     { return getName(); }
juce::String MarkerTrack::getName() const                { return TRANS("Marker"); }
bool MarkerTrack::canContainPlugin (Plugin*) const       { return false; }

}} // namespace tracktion { inline namespace engine
