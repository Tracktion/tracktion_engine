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
    assert (state.hasType (IDs::CLIPSLOT));
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


//==============================================================================
//==============================================================================
ClipSlotList::ClipSlotList (const juce::ValueTree& v, Track& t)
    : ValueTreeObjectList<ClipSlot> (v),
      state (v),
      track (t)
{
    assert (v.hasType (IDs::CLIPSLOTS));
    rebuildObjects();
}

ClipSlotList::~ClipSlotList()
{
    freeObjects();
}

juce::Array<ClipSlot*> ClipSlotList::getClipSlots() const
{
    return objects;
}

void ClipSlotList::ensureNumberOfSlots (int numSlots)
{
    for (int i = size(); i < numSlots; ++i)
        parent.appendChild (juce::ValueTree (IDs::CLIPSLOT), &track.edit.getUndoManager());

    assert (objects.size() >= numSlots);
}

void ClipSlotList::deleteSlot (ClipSlot& cs)
{
    assert (&cs.track == &track);
    parent.removeChild (cs.state, &track.edit.getUndoManager());
}

//==============================================================================
bool ClipSlotList::isSuitableType (const juce::ValueTree& v) const
{
    return v.hasType (IDs::CLIPSLOT);
}

ClipSlot* ClipSlotList::createNewObject (const juce::ValueTree& v)
{
    return new ClipSlot (v, track);
}

void ClipSlotList::deleteObject (ClipSlot* clipSlot)
{
    delete clipSlot;
}

void ClipSlotList::newObjectAdded (ClipSlot*)
{
}

void ClipSlotList::objectRemoved (ClipSlot*)
{
}

void ClipSlotList::objectOrderChanged()
{
}


}} // namespace tracktion { inline namespace engine
