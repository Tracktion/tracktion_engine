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

ArrangerClip::ArrangerClip (const juce::ValueTree& v, EditItemID id, ClipTrack& targetTrack)
   : Clip (v, targetTrack, id, Type::arranger)
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

String ArrangerClip::getSelectableDescription()
{
    return TRANS("Arranger Clip") + " - \"" + getName() + "\"";
}

Colour ArrangerClip::getDefaultColour() const
{
    return Colours::red.withHue (7.0f / 9.0f);
}

//==============================================================================
bool ArrangerClip::canGoOnTrack (Track& t)
{
    return t.isArrangerTrack();
}

void ArrangerClip::valueTreePropertyChanged (ValueTree& v, const juce::Identifier& i)
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

}
