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

/** Converts a TimePosition to a BeatPosition given a TempoSequence. */
tracktion_graph::BeatPosition toBeats (tracktion_graph::TimePosition, const TempoSequence&);

/** Converts a BeatPosition to a TimePosition given a TempoSequence. */
tracktion_graph::TimePosition toTime (tracktion_graph::BeatPosition, const TempoSequence&);


//==============================================================================
//==============================================================================
/**
    Represents a time point in an Edit stored as either time or beats.
    This is basically a variant to simplify APIs.
*/
struct EditTime
{
    /** Creates an EditTime from a TimePosition. */
    EditTime (tracktion_graph::TimePosition);

    /** Creates an EditTime from a BeatPosition. */
    EditTime (tracktion_graph::BeatPosition);

private:
    friend tracktion_graph::TimePosition toTime (EditTime, const TempoSequence&);
    friend tracktion_graph::BeatPosition toBeats (EditTime, const TempoSequence&);

    std::variant<tracktion_graph::TimePosition, tracktion_graph::BeatPosition> position;
};

//==============================================================================
/** Converts an EditTime to a TimePosition.
    N.B. This may be a slow operation if this was created using a BeatPosition.
*/
tracktion_graph::TimePosition toTime (EditTime, const TempoSequence&);

/** Converts an EditTime to a BeatPosition.
    N.B. This may be a slow operation if this was created using a TimePosition.
*/
tracktion_graph::BeatPosition toBeats (EditTime, const TempoSequence&);


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

inline EditTime::EditTime (tracktion_graph::TimePosition tp)
    : position (tp)
{
}

inline EditTime::EditTime (tracktion_graph::BeatPosition bp)
    : position (bp)
{
}

inline tracktion_graph::TimePosition toTime (EditTime et, const TempoSequence& ts)
{
    // N.B. std::get unavailable prior to macOS 10.14
    if (const auto tp = std::get_if<tracktion_graph::TimePosition> (&et.position))
        return *tp;

    return tracktion_engine::toTime (*std::get_if<tracktion_graph::BeatPosition> (&et.position), ts);
}

inline tracktion_graph::BeatPosition toBeats (EditTime et, const TempoSequence& ts)
{
    if (const auto bp = std::get_if<tracktion_graph::BeatPosition> (&et.position))
        return *bp;

    return tracktion_engine::toBeats (*std::get_if<tracktion_graph::TimePosition> (&et.position), ts);
}


} // namespace tracktion_engine
