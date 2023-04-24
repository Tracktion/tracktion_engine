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

AutomationTrack::AutomationTrack (Edit& e, const juce::ValueTree& v)
    : Track (e, v, 50, 13, 2000)
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

juce::String AutomationTrack::getName()
{
    if (auto ap = getCurrentlyShownAutoParam())
        return ap->getFullName();

    return {};
}

}} // namespace tracktion { inline namespace engine
