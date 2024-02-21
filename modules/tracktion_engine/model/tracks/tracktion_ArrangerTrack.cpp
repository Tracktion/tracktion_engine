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

ArrangerTrack::ArrangerTrack (Edit& e, const juce::ValueTree& v)
    : ClipTrack (e, v)
{
}

ArrangerTrack::~ArrangerTrack()
{
    notifyListenersOfDeletion();
}

bool ArrangerTrack::isArrangerTrack() const             { return true; }
juce::String ArrangerTrack::getSelectableDescription()  { return getName(); }
juce::String ArrangerTrack::getName() const             { return TRANS("Arranger"); }
bool ArrangerTrack::canContainPlugin (Plugin*) const    { return false; }

}} // namespace tracktion { inline namespace engine
