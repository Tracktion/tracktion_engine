/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


TempoTrack::TempoTrack (Edit& edit, const ValueTree& v)
    : Track (edit, v, 40, 13, 200)
{
}

TempoTrack::~TempoTrack()
{
    notifyListenersOfDeletion();
}

bool TempoTrack::isTempoTrack() const               { return true; }
juce::String TempoTrack::getName()                  { return TRANS ("Tempo"); }
String TempoTrack::getSelectableDescription()       { return TRANS("Global Track"); }
bool TempoTrack::canContainPlugin (Plugin*) const   { return false; }

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

int TempoTrack::getIndexOfNextTrackItemAt (double time)
{
    return findIndexOfNextItemAt (buildTrackItemList(), time);
}

TrackItem* TempoTrack::getNextTrackItemAt (double time)
{
    return getTrackItem (getIndexOfNextTrackItemAt (time));
}

void TempoTrack::insertSpaceIntoTrack (double time, double amountOfSpace)
{
    Track::insertSpaceIntoTrack (time, amountOfSpace);

    // Insert in to pitch sequence first of the tempo calculations will be different
    edit.pitchSequence.insertSpaceIntoSequence (time, amountOfSpace, false);
    edit.tempoSequence.insertSpaceIntoSequence (time, amountOfSpace, false);
}

struct TempoTrackSorter
{
    static int compareElements (TrackItem* first, TrackItem* second)
    {
        auto t1 = first->getPosition().getStart();
        auto t2 = second->getPosition().getStart();

        if (t1 < t2)  return -1;
        if (t1 > t2)  return 1;

        if (typeid (*first) == typeid (*second))
            return 0;

        if (typeid (*first) == typeid (TimeSigSetting))
            return -1;

        return 1;
    }
};

juce::Array<TrackItem*> TempoTrack::buildTrackItemList() const
{
    juce::Array<TrackItem*> items;

    auto& ts = edit.tempoSequence;
    auto& ps = edit.pitchSequence;

    for (int i = 0; i < ts.getNumTimeSigs(); ++i)
        items.add (ts.getTimeSig (i));

    for (int i = 0; i < ps.getNumPitches(); ++i)
        items.add (ps.getPitch (i));

    TempoTrackSorter tts;
    items.sort (tts);

    return items;
}
