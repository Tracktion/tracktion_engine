/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

MarkerTrack::MarkerTrack (Edit& edit, const ValueTree& v)  : ClipTrack (edit, v, 40, 13, 60)
{
}

MarkerTrack::~MarkerTrack()
{
    notifyListenersOfDeletion();
}

bool MarkerTrack::isMarkerTrack() const                  { return true; }
juce::String MarkerTrack::getSelectableDescription()     { return getName(); }
juce::String MarkerTrack::getName()                      { return TRANS("Markers"); }
bool MarkerTrack::canContainPlugin (Plugin*) const       { return false; }

}
