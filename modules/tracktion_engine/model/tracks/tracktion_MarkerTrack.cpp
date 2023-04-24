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

MarkerTrack::MarkerTrack (Edit& e, const juce::ValueTree& v)  : ClipTrack (e, v, 40, 13, 60)
{
}

MarkerTrack::~MarkerTrack()
{
    notifyListenersOfDeletion();
}

bool MarkerTrack::isMarkerTrack() const                  { return true; }
juce::String MarkerTrack::getSelectableDescription()     { return getName(); }
juce::String MarkerTrack::getName()                      { return TRANS("Marker"); }
bool MarkerTrack::canContainPlugin (Plugin*) const       { return false; }

}} // namespace tracktion { inline namespace engine
