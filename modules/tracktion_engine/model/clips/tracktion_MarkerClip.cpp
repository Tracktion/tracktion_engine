/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


MarkerClip::MarkerClip (const ValueTree& v, EditItemID id, ClipTrack& targetTrack)
   : Clip (v, targetTrack, id, Type::marker)
{
}

MarkerClip::~MarkerClip()
{
    notifyListenersOfDeletion();
}

void MarkerClip::initialise()
{
    Clip::initialise();

    markerID.referTo (state, IDs::markerID, &edit.getUndoManager());

    if (! state.hasProperty (IDs::markerID))
        markerID = edit.getMarkerManager().getNextUniqueID();

    if (clipName == TRANS("New Marker"))
        clipName = TRANS("Marker") + " " + String (markerID);

    speedRatio = 1.0; // not used
}

String MarkerClip::getSelectableDescription()
{
    return TRANS("Marker Clip") + " - \"" + getName() + "\"";
}

Colour MarkerClip::getDefaultColour() const
{
    return Colours::red.withHue (1.0f / 9.0f);
}

void MarkerClip::setMarkerID (int newID)
{
    if (getName() == (TRANS("Marker") + " " + String (markerID)))
        setName (TRANS("Marker") + " " + String (newID));

    markerID = newID;
}

//==============================================================================
bool MarkerClip::canGoOnTrack (Track& t)
{
    return t.isMarkerTrack();
}

void MarkerClip::valueTreePropertyChanged (ValueTree& v, const Identifier& i)
{
    if (v == state)
    {
        if (i == IDs::markerID)
        {
            markerID.forceUpdateOfCachedValue();
            changed();
        }
        else if (i == IDs::sync)
        {
            SelectionManager::refreshAllPropertyPanels();
            changed();
        }
    }

    Clip::valueTreePropertyChanged (v, i);
}

Colour MarkerClip::getColour() const
{
    if (Clip::getColour() == getDefaultColour())
    {
        if (isSyncAbsolute())   return Colours::red.withHue (0.0f);
        if (isSyncBarsBeats())  return Colours::red.withHue (1.0f / 9.0f);
    }

    return Clip::getColour();
}
