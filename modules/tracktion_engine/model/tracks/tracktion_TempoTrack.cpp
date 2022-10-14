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

TempoTrack::TempoTrack (Edit& e, const juce::ValueTree& v)
    : Track (e, v, 40, 13, 200)
{
}

TempoTrack::~TempoTrack()
{
    notifyListenersOfDeletion();
}

bool TempoTrack::isTempoTrack() const                  { return true; }
juce::String TempoTrack::getName()                     { return TRANS ("Tempo"); }
juce::String TempoTrack::getSelectableDescription()    { return TRANS("Global Track"); }
bool TempoTrack::canContainPlugin (Plugin*) const      { return false; }

int TempoTrack::getNumTrackItems() const
{
    return edit.tempoSequence.getNumTimeSigs()
         + edit.pitchSequence.getNumPitches();
}

TrackItem* TempoTrack::getTrackItem (int index) const
{
    return buildTrackItemList()[index];
}

int TempoTrack::indexOfTrackItem (TrackItem* ti) const
{
    return buildTrackItemList().indexOf (ti);
}

int TempoTrack::getIndexOfNextTrackItemAt (TimePosition time)
{
    return findIndexOfNextItemAt (buildTrackItemList(), time);
}

TrackItem* TempoTrack::getNextTrackItemAt (TimePosition time)
{
    return getTrackItem (getIndexOfNextTrackItemAt (time));
}

void TempoTrack::insertSpaceIntoTrack (TimePosition time, TimeDuration amountOfSpace)
{
    Track::insertSpaceIntoTrack (time, amountOfSpace);

    // Insert in to pitch sequence first of the tempo calculations will be different
    edit.pitchSequence.insertSpaceIntoSequence (time, amountOfSpace, false);
    edit.tempoSequence.insertSpaceIntoSequence (time, amountOfSpace, false);
}

juce::Array<TrackItem*> TempoTrack::buildTrackItemList() const
{
    juce::Array<TrackItem*> items;

    items.addArray (edit.tempoSequence.getTimeSigs());
    items.addArray (edit.pitchSequence.getPitches());

    std::sort (items.begin(), items.end(),
               [] (TrackItem* a, TrackItem* b)
               {
                   auto t1 = a->getPosition().getStart();
                   auto t2 = b->getPosition().getStart();

                   if (t1 != t2)
                       return t1 < t2;

                   if (typeid (*a) == typeid (*b))
                       return false;

                   return typeid (*a) == typeid (TimeSigSetting);
               });

    return items;
}

}} // namespace tracktion { inline namespace engine
