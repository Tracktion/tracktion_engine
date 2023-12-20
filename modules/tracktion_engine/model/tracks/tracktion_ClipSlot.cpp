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
    assert (itemID.isValid());
    initialiseClipOwner (track.edit, state);
    edit.clipSlotCache.addItem (*this);
}

ClipSlot::~ClipSlot()
{
    notifyListenersOfDeletion();
    edit.clipSlotCache.removeItem (*this);
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

int ClipSlot::getIndex()
{
    if (auto at = dynamic_cast<AudioTrack*> (&track))
        return at->getClipSlotList().getClipSlots().indexOf (this);

    jassertfalse;
    return -1;
}

InputDeviceInstance::Destination* ClipSlot::getInputDestination()
{
    for (auto in : track.edit.getAllInputDevices())
        for (auto dest : in->destinations)
            if (dest->targetID == state[IDs::id])
                return dest;

    return nullptr;
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

EditItemID ClipSlot::getClipOwnerID()
{
    return itemID;
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
    jassert (getClips().size() <= 1);
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

    for (auto child : state)
        if (child.hasType (IDs::CLIPSLOT))
            if (EditItemID::fromID (child).isInvalid())
                track.edit.createNewItemID().writeID (child, nullptr);

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
    {
        auto newSlotState = juce::ValueTree (IDs::CLIPSLOT);
        track.edit.createNewItemID().writeID (newSlotState, nullptr);
        parent.appendChild (newSlotState, &track.edit.getUndoManager());
    }

    assert (objects.size() >= numSlots);
}

void ClipSlotList::setNumberOfSlots (int numSlots)
{
    if (numSlots > size())
    {
        ensureNumberOfSlots (numSlots);
    }
    else if (size() > numSlots)
    {
        while (size() > numSlots)
            deleteSlot (*getClipSlots().getLast());
    }
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
