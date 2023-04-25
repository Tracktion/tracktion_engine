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

ArrangerTrack::ArrangerTrack (Edit& e, const juce::ValueTree& v)
    : ClipTrack (e, v, 30, 13, 60)
{
}

ArrangerTrack::~ArrangerTrack()
{
    notifyListenersOfDeletion();
}

bool ArrangerTrack::isArrangerTrack() const             { return true; }
juce::String ArrangerTrack::getSelectableDescription()  { return getName(); }
juce::String ArrangerTrack::getName()                   { return TRANS("Arranger"); }
bool ArrangerTrack::canContainPlugin (Plugin*) const    { return false; }

}} // namespace tracktion { inline namespace engine
