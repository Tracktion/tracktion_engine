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

ClipSlot::ClipSlot (const juce::ValueTree& v, Track& t)
    : EditItem (EditItemID::fromID (v), t.edit),
      state (v), track (t)
{
    initialiseClipOwner (track.edit, state);
}

ClipSlot::~ClipSlot()
{
    notifyListenersOfDeletion();
}

//==============================================================================
void ClipSlot::setClip (Clip* c)
{
    if (auto existingClip = getClip())
        existingClip->removeFromParent();

    if (c)
        c->moveTo (*this);
}

Clip* ClipSlot::getClip()
{
    return getClips()[0];
}

//==============================================================================
juce::String ClipSlot::getName() const
{
    return TRANS("Clip Slot");
}

juce::String ClipSlot::getSelectableDescription()
{
    return TRANS("Clip Slot");
}

juce::ValueTree& ClipSlot::getClipOwnerState()
{
    return state;
}

Selectable* ClipSlot::getClipOwnerSelectable()
{
    return this;
}

Edit& ClipSlot::getClipOwnerEdit()
{
    return track.edit;
}

//==============================================================================
void ClipSlot::clipCreated (Clip&)
{
}

void ClipSlot::clipAddedOrRemoved()
{
}

void ClipSlot::clipOrderChanged()
{
}

void ClipSlot::clipPositionChanged()
{
}

}} // namespace tracktion { inline namespace engine
