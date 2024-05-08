/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
*/

namespace tracktion { inline namespace engine
{

ChordTrack::ChordTrack (Edit& ed, const juce::ValueTree& v)
    : ClipTrack (ed, v, false)
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
