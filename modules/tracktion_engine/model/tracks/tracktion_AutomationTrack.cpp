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

AutomationTrack::AutomationTrack (Edit& e, const juce::ValueTree& v)
    : Track (e, v, false)
{
}

AutomationTrack::~AutomationTrack()
{
    notifyListenersOfDeletion();
}

//==============================================================================
juce::String AutomationTrack::getSelectableDescription()
{
    auto n = getName();
    return TRANS("Automation") + (n.isEmpty() ? juce::String() : " - \"" + getName() + "\"");
}

juce::String AutomationTrack::getName() const
{
    if (auto ap = getCurrentlyShownAutoParam())
        return ap->getFullName();

    return {};
}

}} // namespace tracktion { inline namespace engine
