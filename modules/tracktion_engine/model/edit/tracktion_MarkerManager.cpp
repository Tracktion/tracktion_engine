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

MarkerManager::MarkerManager (Edit& e, const juce::ValueTree& v)
    : edit (e), state (v)
{
    state.addListener (this);
}

MarkerManager::~MarkerManager()
{
    state.removeListener (this);
}

juce::ReferenceCountedArray<MarkerClip> MarkerManager::getMarkers() const
{
    CRASH_TRACER
    juce::ReferenceCountedArray<MarkerClip> results;

    if (auto mt = edit.getMarkerTrack())
        for (auto clip : mt->getClips())
            if (auto mc = dynamic_cast<MarkerClip*> (clip))
                results.add (mc);

    TrackItem::sortByTime (results);
    return results;
}

int MarkerManager::getNextUniqueID (int start)
{
    juce::SortedSet<int> uniqueIDs;

    for (auto m : getMarkers())
        uniqueIDs.add (m->getMarkerID());

    while (uniqueIDs.contains (start))
        ++start;

    return start;
}

void MarkerManager::checkForDuplicates (MarkerClip& clip, bool changeOthers)
{
    for (auto m : getMarkers())
    {
        if (&clip != m && clip.getMarkerID() == m->getMarkerID())
        {
            if (changeOthers)
                m->setMarkerID (getNextUniqueID (clip.getMarkerID()));
            else
                clip.setMarkerID (getNextUniqueID (clip.getMarkerID()));
        }
    }
}

MarkerClip* MarkerManager::getMarkerByID (int id)
{
    for (auto m : getMarkers())
        if (m->getMarkerID() == id)
            return m;

    return {};
}

MarkerClip* MarkerManager::getNextMarker (double nowTime)
{
    MarkerClip* next = nullptr;

    for (auto m : getMarkers())
    {
        double diff     = m->getPosition().getStart() - nowTime;
        double nextDiff = next ? next->getPosition().getStart() - nowTime : 0.0;

        if (diff > 0.001 && ((next == nullptr) || (diff < nextDiff)))
            next = m;
    }

    return next;
}

MarkerClip* MarkerManager::getPrevMarker (double nowTime)
{
    MarkerClip* prev = nullptr;

    for (auto m : getMarkers())
    {
        double diff     = m->getPosition().getStart() - nowTime;
        double prevDiff = prev ? prev->getPosition().getStart() - nowTime : 0.0;

        if (diff < -0.001 && ((prev == nullptr) || (diff > prevDiff)))
            prev = m;
    }

    return prev;
}

MarkerClip::Ptr MarkerManager::createMarker (int number, double pos, double length,
                                             Clip::SyncType type, SelectionManager* sm)
{
    if (length == 0.0)
    {
        auto& tempo = edit.tempoSequence.getTempoAt (pos);
        length = tempo.getMatchingTimeSig().numerator * tempo.getApproxBeatLength();
    }

    if (auto track = edit.getMarkerTrack())
    {
        if (auto clip = dynamic_cast<MarkerClip*> (track->insertNewClip (TrackItem::Type::marker,
                                                                         { pos, pos + length }, sm)))
        {
            clip->setSyncType (type);
            clip->setStart (pos, true, true);

            if (length > 0.0)
                clip->setLength (length, true);

            if (number >= 0)
                clip->setMarkerID (number);

            return clip;
        }
    }

    jassertfalse;
    return {};
}

Clip::SyncType MarkerManager::getNewMarkerMode() const
{
    const int newMarkerMode = edit.engine.getPropertyStorage().getProperty (SettingID::newMarker);

    if (newMarkerMode == 0 || (newMarkerMode == 2 && edit.getTimecodeFormat().isBarsBeats()))
        return Clip::syncBarsBeats;

    return Clip::syncAbsolute;
}

MarkerClip::Ptr MarkerManager::createMarker (int number, double pos, double length, SelectionManager* sm)
{
    return createMarker (number, pos, length, getNewMarkerMode(), sm);
}

void MarkerManager::valueTreeChanged (const juce::ValueTree& v)
{
    if (v.hasType (IDs::MARKERTRACK) || v.getParent().hasType (IDs::MARKERTRACK))
        sendChangeMessage();
}

void MarkerManager::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier&)  { valueTreeChanged (v); }
void MarkerManager::valueTreeChildAdded (juce::ValueTree& p, juce::ValueTree&)              { valueTreeChanged (p); }
void MarkerManager::valueTreeChildRemoved (juce::ValueTree& p, juce::ValueTree&, int)       { valueTreeChanged (p); }
void MarkerManager::valueTreeParentChanged (juce::ValueTree& v)                             { valueTreeChanged (v); }

}
