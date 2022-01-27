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
    Represents a time point in an Edit.
    This differs from a beat/time position because it is tied to a TempoSequence which
    belongs to an Edit. This means you can convert it directly to a time/beat position.
*/
struct EditTime
{
    /** Creates an EditTime from a TimePosition. */
    EditTime (tracktion_graph::TimePosition, const TempoSequence&);

    /** Creates an EditTime from a BeatPosition. */
    EditTime (tracktion_graph::BeatPosition, const TempoSequence&);

    /** Converts this to a TimePosition.
        N.B. This may be a slow operation if this was created using a BeatPosition.
    */
    tracktion_graph::TimePosition toTime() const;

    /** Converts this to a BeatPosition.
        N.B. This may be a slow operation if this was created using a TimePosition.
    */
    tracktion_graph::BeatPosition toBeats() const;

private:
    friend EditTime operator+ (const EditTime&, tracktion_graph::TimeDuration);
    friend EditTime operator+ (const EditTime&, std::chrono::duration<double>);
    friend EditTime operator+ (const EditTime&, tracktion_graph::BeatDuration);
    friend EditTime operator- (const EditTime&, tracktion_graph::TimeDuration);
    friend EditTime operator- (const EditTime&, std::chrono::duration<double>);
    friend EditTime operator- (const EditTime&, tracktion_graph::BeatDuration);

    std::variant<tracktion_graph::TimePosition, tracktion_graph::BeatPosition> position;
    const TempoSequence& tempoSequence;
};


//==============================================================================
/** Adds a TimeDuration to an EditTime. */
EditTime operator+ (const EditTime&, tracktion_graph::TimeDuration);

/** Adds a chrono time to an EditTime. */
EditTime operator+ (const EditTime&, std::chrono::duration<double>);

/** Adds a BeatDuration to an EditTime. */
EditTime operator+ (const EditTime&, tracktion_graph::BeatDuration);

/** Subtracts a TimeDuration to an EditTime. */
EditTime operator- (const EditTime&, tracktion_graph::TimeDuration);

/** Subtracts a chrono time to an EditTime. */
EditTime operator- (const EditTime&, std::chrono::duration<double>);

/** Subtracts a BeatDuration to an EditTime. */
EditTime operator- (const EditTime&, tracktion_graph::BeatDuration);


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

inline EditTime::EditTime (tracktion_graph::TimePosition tp, const TempoSequence& ts)
    : position (tp), tempoSequence (ts)
{
}

inline EditTime::EditTime (tracktion_graph::BeatPosition bp, const TempoSequence& ts)
    : position (bp), tempoSequence (ts)
{
}

inline tracktion_graph::TimePosition EditTime::toTime() const
{
    // N.B. std::get unavailable prior to macOS 10.14
    if (const auto tp = std::get_if<tracktion_graph::TimePosition> (&position))
        return *tp;

    return tracktion_engine::toTime (*std::get_if<tracktion_graph::BeatPosition> (&position), tempoSequence);
}

inline tracktion_graph::BeatPosition EditTime::toBeats() const
{
    if (const auto bp = std::get_if<tracktion_graph::BeatPosition> (&position))
        return *bp;

    return tracktion_engine::toBeats (*std::get_if<tracktion_graph::TimePosition> (&position), tempoSequence);
}

//==============================================================================
inline EditTime operator+ (const EditTime& et, tracktion_graph::TimeDuration d)
{
    return EditTime (tracktion_graph::TimePosition::fromSeconds (et.toTime().inSeconds() + d.inSeconds()), et.tempoSequence);
}

inline EditTime operator+ (const EditTime& et, std::chrono::duration<double> t)
{
    return EditTime (tracktion_graph::TimePosition::fromSeconds (et.toTime().inSeconds() + t.count()), et.tempoSequence);
}

inline EditTime operator+ (const EditTime& et, tracktion_graph::BeatDuration d)
{
    return EditTime (tracktion_graph::BeatPosition::fromBeats (et.toBeats().inBeats() + d.inBeats()), et.tempoSequence);
}

inline EditTime operator- (const EditTime& et, tracktion_graph::TimeDuration d)
{
    return EditTime (tracktion_graph::TimePosition::fromSeconds (et.toTime().inSeconds() - d.inSeconds()), et.tempoSequence);
}

inline EditTime operator- (const EditTime& et, std::chrono::duration<double> t)
{
    return EditTime (tracktion_graph::TimePosition::fromSeconds (et.toTime().inSeconds() - t.count()), et.tempoSequence);
}

inline EditTime operator- (const EditTime& et, tracktion_graph::BeatDuration d)
{
    return EditTime (tracktion_graph::BeatPosition::fromBeats (et.toBeats().inBeats() - d.inBeats()), et.tempoSequence);
}


} // namespace tracktion_engine
