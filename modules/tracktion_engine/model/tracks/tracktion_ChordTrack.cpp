/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


ChordTrack::ChordTrack (Edit& edit, const ValueTree& v)  : ClipTrack (edit, v, 20, 13, 60)
{
}

ChordTrack::~ChordTrack()
{
    notifyListenersOfDeletion();
}

String ChordTrack::getTrackWarning() const
{
    auto& clips = getClips();

    for (int i = 0; i < clips.size(); ++i)
    {
        auto pos1 = clips.getUnchecked(i)->getPosition().time;

        for (int j = i + 1; j < clips.size(); ++j)
            if (pos1.overlaps (clips.getUnchecked(j)->getPosition().time))
                return TRANS("This track contains overlapping chords. Portions of some chord are ignored.");
    }

    return {};
}
