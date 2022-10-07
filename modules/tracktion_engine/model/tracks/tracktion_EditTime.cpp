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

BeatPosition toBeats (TimePosition tp, const TempoSequence& ts)
{
    return ts.toBeats (tp);
}

TimePosition toTime (BeatPosition bp, const TempoSequence& ts)
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

}} // namespace tracktion { inline namespace engine
