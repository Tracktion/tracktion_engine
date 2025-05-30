/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

BeatPosition toBeats (TimePosition tp, const TempoSequence& ts)
{
    return ts.toBeats (tp);
}

BeatPosition toBeats (TimePosition tp, const tempo::Sequence& ts)
{
    return ts.toBeats (tp);
}

TimePosition toTime (BeatPosition bp, const TempoSequence& ts)
{
    return ts.toTime (bp);
}

TimePosition toTime (BeatPosition bp, const tempo::Sequence& ts)
{
    return ts.toTime (bp);
}

BeatRange toBeats (TimeRange r, const TempoSequence& ts)
{
    return { toBeats (r.getStart(), ts), toBeats (r.getEnd(), ts) };
}

TimeRange toTime (BeatRange r, const TempoSequence& ts)
{
    return { toTime (r.getStart(), ts), toTime (r.getEnd(), ts) };
}

BeatRange toBeats (TimeRange r, const tempo::Sequence& ts)
{
    return { toBeats (r.getStart(), ts), toBeats (r.getEnd(), ts) };
}

TimeRange toTime (BeatRange r, const tempo::Sequence& ts)
{
    return { toTime (r.getStart(), ts), toTime (r.getEnd(), ts) };
}

ClipPosition createClipPosition (const TempoSequence&, TimeRange range, TimeDuration offset)
{
    return { range, offset };
}

ClipPosition createClipPosition (const TempoSequence& ts, BeatRange range, BeatDuration offset)
{
    return { ts.toTime (range), toDuration (ts.toTime (toPosition (offset))) };
}



}} // namespace tracktion { inline namespace engine
