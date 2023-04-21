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

MarkerClip::MarkerClip (const juce::ValueTree& v, EditItemID id, ClipTrack& targetTrack)
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
        clipName = TRANS("Marker") + " " + juce::String (markerID);

    speedRatio = 1.0; // not used
}

juce::String MarkerClip::getSelectableDescription()
{
    return TRANS("Marker Clip") + " - \"" + getName() + "\"";
}

juce::Colour MarkerClip::getDefaultColour() const
{
    return juce::Colours::red.withHue (1.0f / 9.0f);
}

void MarkerClip::setMarkerID (int newID)
{
    if (getName() == (TRANS("Marker") + " " + juce::String (markerID)))
        setName (TRANS("Marker") + " " + juce::String (newID));

    markerID = newID;
}

//==============================================================================
bool MarkerClip::canGoOnTrack (Track& t)
{
    return t.isMarkerTrack();
}

void MarkerClip::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
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

juce::Colour MarkerClip::getColour() const
{
    if (Clip::getColour() == getDefaultColour())
    {
        if (isSyncAbsolute())   return juce::Colours::red.withHue (0.0f);
        if (isSyncBarsBeats())  return juce::Colours::red.withHue (1.0f / 9.0f);
    }

    return Clip::getColour();
}

}} // namespace tracktion { inline namespace engine
