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

inline tracktion_graph::BeatPosition toBeats (tracktion_graph::TimePosition tp, const TempoSequence& ts)
{
    return tracktion_graph::BeatPosition::fromBeats (ts.timeToBeats (tp.inSeconds()));
}

inline tracktion_graph::TimePosition toTime (tracktion_graph::BeatPosition bp, const TempoSequence& ts)
{
    return tracktion_graph::TimePosition::fromSeconds (ts.beatsToTime (bp.inBeats()));
}

} // namespace tracktion_engine
