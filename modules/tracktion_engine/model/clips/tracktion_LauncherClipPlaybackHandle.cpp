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

LauncherClipPlaybackHandle LauncherClipPlaybackHandle::forLooping (BeatRange loopRange,
                                                                   BeatDuration offset)
{
    LauncherClipPlaybackHandle h;
    h.loopRange = loopRange;
    h.offset = offset;
    h.isLooping = true;

    return h;
}

LauncherClipPlaybackHandle LauncherClipPlaybackHandle::forOneShot (BeatRange clipRange)
{
    LauncherClipPlaybackHandle h;
    h.clipRange = clipRange;

    return h;
}

//==============================================================================
void LauncherClipPlaybackHandle::start (BeatPosition p)
{
    playStartPosition = p;
}

void LauncherClipPlaybackHandle::stop()
{
    playStartPosition = std::nullopt;
}

std::optional<BeatPosition> LauncherClipPlaybackHandle::getStart() const
{
    return playStartPosition;
}

LauncherClipPlaybackHandle::SplitBeatRange LauncherClipPlaybackHandle::timelineRangeToClipSourceRange (BeatRange r) const
{
    if (! playStartPosition)
        return {};

    const auto s = *playStartPosition;

    if (isLooping)
    {
        if (r.getEnd() < s)
            return {};

        // Case where range is off the start edge
        if (r.getStart() < s)
        {
            const auto duration1 = s - r.getStart();
            const auto duration2 = r.getEnd() - s;

            SplitBeatRange splitRange;
            splitRange.range1 = { toPosition (offset) - duration1, toPosition (offset) };
            splitRange.playing1 = false;
            splitRange.range2 = { toPosition (offset), duration2 };
            splitRange.playing2 = true;

            return splitRange;
        }

        // The whole range is within the looping region
        // Wrap each position to the loop length and then see if the end is
        // before the start to know it looped
        {
            assert (r.getStart () >= s);
            const auto sourceTimelineStart = s - offset;
            const BeatRange virtualClipTimelineRange (sourceTimelineStart, loopRange.getLength());

            const auto wrappedTimelineStart = BeatPosition::fromBeats (std::fmod (r.getStart().inBeats(), virtualClipTimelineRange.getLength().inBeats()));
            const auto wrappedTimelineEnd = BeatPosition::fromBeats (std::fmod (r.getEnd().inBeats(), virtualClipTimelineRange.getLength().inBeats()));
            assert (wrappedTimelineEnd != wrappedTimelineStart);
            assert (virtualClipTimelineRange.contains (wrappedTimelineStart));
            assert (virtualClipTimelineRange.contains (wrappedTimelineEnd));

            if (wrappedTimelineEnd > wrappedTimelineStart)
            {
                SplitBeatRange splitRange;
                splitRange.range1 = { wrappedTimelineStart - toDuration (sourceTimelineStart),
                                      wrappedTimelineEnd   - toDuration (sourceTimelineStart) };
                splitRange.playing1 = true;

                return splitRange;
            }

            // Case where the beat range straddles a loop point
            // The range has to be split in two
            assert (wrappedTimelineStart > wrappedTimelineEnd);
            SplitBeatRange splitRange;
            splitRange.range1 = { wrappedTimelineStart - toDuration (sourceTimelineStart),
                                  virtualClipTimelineRange.getEnd() - toDuration (sourceTimelineStart) };
            splitRange.playing1 = true;
            splitRange.range2 = { virtualClipTimelineRange.getStart() - toDuration (sourceTimelineStart),
                                  wrappedTimelineEnd - toDuration (sourceTimelineStart) };
            splitRange.playing2 = true;

            return splitRange;
        }
    }

    if (const BeatRange validClipRange (s, clipRange.getLength());
        validClipRange.intersects (r))
    {
        const auto clipStart = clipRange.getStart();
        const auto clipEnd = clipRange.getEnd();

        if (r.getEnd() < s)
            return {};

        // Case where range is off the start edge
        if (r.getStart() < s)
        {
            const auto duration1 = s - r.getStart();
            const auto duration2 = r.getEnd() - s;

            SplitBeatRange splitRange;
            splitRange.range1 = { clipStart - duration1, clipStart };
            splitRange.playing1 = false;
            splitRange.range2 = { clipStart, duration2 };
            splitRange.playing2 = true;

            return splitRange;
        }

        // Case where range is off the start edge
        if (r.getEnd() > validClipRange.getEnd())
        {
            const auto duration1 = validClipRange.getEnd() - r.getStart();
            const auto duration2 = r.getEnd() - validClipRange.getEnd();

            SplitBeatRange splitRange;
            splitRange.range1 = { clipEnd - duration1, clipEnd };
            splitRange.playing1 = true;
            splitRange.range2 = { clipEnd, duration2 };
            splitRange.playing2 = false;

            return splitRange;
        }

        // Case where range is within the clip range
        assert (validClipRange.contains (r));
        const auto positionInClip = r.getStart() - toDuration (s);

        SplitBeatRange splitRange;
        splitRange.range1 = { positionInClip + toDuration (clipStart), r.getLength() };
        splitRange.playing1 = true;

        return splitRange;
    }

    return {};
}

std::optional<float> LauncherClipPlaybackHandle::getProgress (BeatPosition p) const
{
    if (! playStartPosition)
        return std::nullopt;

    const auto s = *playStartPosition;

    if (isLooping)
    {
        if (p < s)
            return std::nullopt;

        const auto playedDuration = p - s;
        const auto numIterationsPlayed = (loopRange.getLength() - playedDuration) / loopRange.getLength();
        return static_cast<float> (numIterationsPlayed - static_cast<int> (numIterationsPlayed));
    }

    const BeatRange validRange (s, clipRange.getLength());

    if (validRange.contains (p))
        return static_cast<float> ((p - s) / validRange.getLength());

    return std::nullopt;
}

}} // namespace tracktion { inline namespace engine
