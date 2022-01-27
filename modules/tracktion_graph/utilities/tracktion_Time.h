/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include <chrono>

namespace tracktion_graph
{

//==============================================================================
//==============================================================================
/**
    Represents a position in real-life time.
    E.g. A position on a timeline.
*/
struct TimePosition
{
    /** Creates a position at a time of 0. */
    TimePosition() = default;

    /** Creates a position from a std::chrono.
        This can be a std::chrono::literal.
    */
    TimePosition (std::chrono::duration<double>);

    /** Create a TimePosition from a number of seconds. */
    template<typename T>
    static TimePosition fromSeconds (T positionInSeconds);

    /** Create a TimePosition from a number of samples and a sample rate. */
    template<typename IntType>
    static TimePosition fromSamples (IntType numSamples, double sampleRate);

    /** Returns the TimePosition as a number of seconds. */
    double inSeconds() const;

private:
    double seconds = 0.0;
};

/** Converts a TimePosition to a number of samples. */
int64_t toSamples (TimePosition, double sampleRate);

//==============================================================================
//==============================================================================
/**
    Represents a duration in real-life time.
    E.g. The time between two points on a timeline.
*/
struct TimeDuration
{
    /** Creates a position at a time of 0. */
    TimeDuration() = default;

    /** Creates a position from a std::chrono.
        This can be a std::chrono::literal.
    */
    TimeDuration (std::chrono::duration<double>);

    /** Create a TimeDuration from a number of seconds. */
    template<typename T>
    static TimeDuration fromSeconds (T positionInSeconds);

    /** Create a TimeDuration from a number of samples and a sample rate. */
    template<typename IntType>
    static TimeDuration fromSamples (IntType numSamples, double sampleRate);

    /** Returns the TimeDuration as a number of seconds. */
    double inSeconds() const;

private:
    double seconds = 0.0;
};

/** Converts a TimeDuration to a number of samples. */
int64_t toSamples (TimeDuration, double sampleRate);

//==============================================================================
/** Adds two TimeDurations together. */
TimeDuration operator+ (const TimeDuration&, const TimeDuration&);

/** Adds a time to a TimeDuration. */
TimeDuration operator+ (const TimeDuration&, std::chrono::duration<double>);

/** Adds a TimeDuration to a TimePosition. */
TimePosition operator+ (const TimePosition&, const TimeDuration&);

/** Adds a time to a TimePosition. */
TimePosition operator+ (const TimePosition&, std::chrono::duration<double>);

/** Subtracts a TimeDuration from another one. */
TimeDuration operator- (const TimeDuration&, const TimeDuration&);

/** Subtracts a time from a TimeDuration. */
TimeDuration operator- (const TimeDuration&, std::chrono::duration<double>);

/** Subtracts a TimeDuration from a TimePosition. */
TimePosition operator- (const TimePosition&, const TimeDuration&);

/** Subtracts a time from a TimePosition. */
TimePosition operator- (const TimePosition&, std::chrono::duration<double>);

//==============================================================================
//==============================================================================
/**
    Represents a position in beats.
    E.g. A beat position on a timeline.

    The time duration of a beat depends on musical information such
    as tempo and time signature.
*/
struct BeatPosition
{
    /** Creates a position at a beat of 0. */
    BeatPosition() = default;

    /** Create a BeatPosition from a number of beats. */
    template<typename T>
    static BeatPosition fromBeats (T positionInBeats);

    /** Returns the position as a number of beats. */
    double inBeats() const;

private:
    double numBeats = 0.0;
};


//==============================================================================
//==============================================================================
/**
    Represents a duration in beats.
    E.g. The time between two beat positions on a timeline.

    The time duration of a beat depends on musical information such
    as tempo and time signature.
*/
struct BeatDuration
{
    /** Creates a position at a beat of 0. */
    BeatDuration() = default;

    /** Create a BeatPosition from a number of beats. */
    template<typename T>
    static BeatDuration fromBeats (T durationInBeats);

    /** Returns the position as a number of beats. */
    double inBeats() const;

private:
    double numBeats = 0.0;
};


//==============================================================================
/** Adds two BeatDurations together. */
BeatDuration operator+ (const BeatDuration&, const BeatDuration&);

/** Adds a BeatDuration to a BeatPosition. */
BeatPosition operator+ (const BeatPosition&, const BeatDuration&);

/** Subtracts a BeatDuration from another one. */
BeatDuration operator- (const BeatDuration&, const BeatDuration&);

/** Subtracts a BeatDuration from a BeatPosition. */
BeatPosition operator- (const BeatPosition&, const BeatDuration&);


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

inline TimePosition::TimePosition (std::chrono::duration<double> duration)
    : seconds (duration.count())
{
}

template<typename T>
inline TimePosition TimePosition::fromSeconds (T positionInSeconds)
{
    TimePosition pos;
    pos.seconds = static_cast<double> (positionInSeconds);
    return pos;
}

template<typename IntType>
inline TimePosition TimePosition::fromSamples (IntType samplePosition, double sampleRate)
{
    return TimePosition::fromSeconds (samplePosition / sampleRate);
}

inline double TimePosition::inSeconds() const
{
    return seconds;
}

inline int64_t toSamples (TimePosition p, double sampleRate)
{
    return static_cast<int64_t> ((p.inSeconds() * sampleRate)
                                 + (p.inSeconds() >= 0.0 ? 0.5 : -0.5));
}

//==============================================================================
inline TimeDuration::TimeDuration (std::chrono::duration<double> duration)
    : seconds (duration.count())
{
}

template<typename T>
inline TimeDuration TimeDuration::fromSeconds (T positionInSeconds)
{
    TimeDuration pos;
    pos.seconds = static_cast<double> (positionInSeconds);
    return pos;
}

template<typename IntType>
inline TimeDuration TimeDuration::fromSamples (IntType samplePosition, double sampleRate)
{
    return TimeDuration::fromSeconds (samplePosition / sampleRate);
}

inline double TimeDuration::inSeconds() const
{
    return seconds;
}

inline int64_t toSamples (TimeDuration p, double sampleRate)
{
    return static_cast<int64_t> ((p.inSeconds() * sampleRate)
                                 + (p.inSeconds() >= 0.0 ? 0.5 : -0.5));
}


//==============================================================================
inline TimeDuration operator+ (const TimeDuration& t1, const TimeDuration& t2)
{
    return TimeDuration::fromSeconds (t1.inSeconds() + t2.inSeconds());
}

inline TimePosition operator+ (const TimePosition& t1, const TimeDuration& t2)
{
    return TimePosition::fromSeconds (t1.inSeconds() + t2.inSeconds());
}

inline TimeDuration operator- (const TimeDuration& t1, const TimeDuration& t2)
{
    return TimeDuration::fromSeconds (t1.inSeconds() - t2.inSeconds());
}

inline TimePosition operator- (const TimePosition& t1, const TimeDuration& t2)
{
    return TimePosition::fromSeconds (t1.inSeconds() - t2.inSeconds());
}

inline TimeDuration operator+ (const TimeDuration& t1, std::chrono::duration<double> t2)
{
    return TimeDuration::fromSeconds (t1.inSeconds() + t2.count());
}

inline TimePosition operator+ (const TimePosition& t1, std::chrono::duration<double> t2)
{
    return TimePosition::fromSeconds (t1.inSeconds() + t2.count());
}

inline TimeDuration operator- (const TimeDuration& t1, std::chrono::duration<double> t2)
{
    return TimeDuration::fromSeconds (t1.inSeconds() - t2.count());
}

inline TimePosition operator- (const TimePosition& t1, std::chrono::duration<double> t2)
{
    return TimePosition::fromSeconds (t1.inSeconds() - t2.count());
}

//==============================================================================
template<typename T>
inline BeatPosition BeatPosition::fromBeats (T positionInBeats)
{
    BeatPosition pos;
    pos.numBeats = static_cast<double> (positionInBeats);
    return pos;
}

inline double BeatPosition::inBeats() const
{
    return numBeats;
}


//==============================================================================
//==============================================================================
template<typename T>
inline BeatDuration BeatDuration::fromBeats (T durationInBeats)
{
    BeatDuration pos;
    pos.numBeats = static_cast<double> (durationInBeats);
    return pos;
}

inline double BeatDuration::inBeats() const
{
    return numBeats;
}


//==============================================================================
inline BeatDuration operator+ (const BeatDuration& t1, const BeatDuration& t2)
{
    return BeatDuration::fromBeats (t1.inBeats() + t2.inBeats());
}

inline BeatPosition operator+ (const BeatPosition& t1, const BeatDuration& t2)
{
    return BeatPosition::fromBeats (t1.inBeats() + t2.inBeats());
}

inline BeatDuration operator- (const BeatDuration& t1, const BeatDuration& t2)
{
    return BeatDuration::fromBeats (t1.inBeats() - t2.inBeats());
}

inline BeatPosition operator- (const BeatPosition& t1, const BeatDuration& t2)
{
    return BeatPosition::fromBeats (t1.inBeats() - t2.inBeats());
}

} // namespace tracktion_graph
