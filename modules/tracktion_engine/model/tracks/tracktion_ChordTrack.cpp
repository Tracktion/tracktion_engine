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

ChordTrack::ChordTrack (Edit& ed, const juce::ValueTree& v)  : ClipTrack (ed, v, 20, 13, 60)
{
}

ChordTrack::~ChordTrack()
{
    notifyListenersOfDeletion();
}

juce::String ChordTrack::getTrackWarning() const
{
    auto& clips = getClips();

    for (int i = 0; i < clips.size(); ++i)
    {
        auto pos1 = clips.getUnchecked(i)->getPosition().time;

        for (int j = i + 1; j < clips.size(); ++j)
            if (pos1.overlaps (clips.getUnchecked (j)->getPosition().time.reduced (TimeDuration::fromSeconds (0.0001))))
                return TRANS("This track contains overlapping chords. Portions of some chord are ignored.");
    }

    return {};
}

}} // namespace tracktion { inline namespace engine
