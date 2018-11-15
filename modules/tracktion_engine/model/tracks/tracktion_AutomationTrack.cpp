/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


AutomationTrack::AutomationTrack (Edit& e, const ValueTree& v)
    : Track (e, v, 50, 13, 2000)
{
}

AutomationTrack::~AutomationTrack()
{
    notifyListenersOfDeletion();
}

//==============================================================================
String AutomationTrack::getSelectableDescription()
{
    auto n = getName();
    return TRANS("Automation") + (n.isEmpty() ? String() : " - \"" + getName() + "\"");
}

String AutomationTrack::getName()
{
    if (auto ap = getCurrentlyShownAutoParam())
        return ap->getFullName();

    return {};
}
