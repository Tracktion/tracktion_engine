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

ArrangerClip::ArrangerClip (const juce::ValueTree& v, EditItemID id, ClipOwner& targetParent)
   : Clip (v, targetParent, id, Type::arranger)
{
}

ArrangerClip::~ArrangerClip()
{
    notifyListenersOfDeletion();
}

void ArrangerClip::initialise()
{
    Clip::initialise();
}

juce::String ArrangerClip::getSelectableDescription()
{
    return TRANS("Arranger Clip") + " - \"" + getName() + "\"";
}

juce::Colour ArrangerClip::getDefaultColour() const
{
    return juce::Colours::red.withHue (7.0f / 9.0f);
}

//==============================================================================
bool ArrangerClip::canBeAddedTo (ClipOwner& co)
{
    return isArrangerTrack (co);
}

void ArrangerClip::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
{
    if (v == state)
    {
        if (i == IDs::sync)
        {
            SelectionManager::refreshAllPropertyPanels();
            changed();
        }
    }

    Clip::valueTreePropertyChanged (v, i);
}

}} // namespace tracktion { inline namespace engine
