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
    constexpr TimePosition() = default;

    /** Creates a copy of another TimePosition. */
    constexpr TimePosition (const TimePosition&) = default;

    /** Creates a position from a std::chrono.
        This can be a std::chrono::literal.
    */
    constexpr TimePosition (std::chrono::duration<double>);

    /** Create a TimePosition from a number of seconds. */
    template<typename T>
    static constexpr TimePosition fromSeconds (T positionInSeconds);

    /** Create a TimePosition from a number of samples and a sample rate. */
    template<typename IntType>
    static constexpr TimePosition fromSamples (IntType numSamples, double sampleRate);

    /** Returns the TimePosition as a number of seconds. */
    constexpr  double inSeconds() const;

private:
    double seconds = 0.0;
};

/** Converts a TimePosition to a number of samples. */
constexpr int64_t toSamples (TimePosition, double sampleRate);

//==============================================================================
//==============================================================================
/**
    Represents a duration in real-life time.
    E.g. The time between two points on a timeline.
*/
struct TimeDuration
{
    /** Creates a position at a time of 0. */
    constexpr TimeDuration() = default;

    /** Creates a copy of another TimeDuration. */
    constexpr TimeDuration (const TimeDuration&) = default;

    /** Creates a position from a std::chrono.
        This can be a std::chrono::literal.
    */
    constexpr TimeDuration (std::chrono::duration<double>);

    /** Create a TimeDuration from a number of seconds. */
    template<typename T>
    static constexpr TimeDuration fromSeconds (T positionInSeconds);

    /** Create a TimeDuration from a number of samples and a sample rate. */
    template<typename IntType>
    static constexpr TimeDuration fromSamples (IntType numSamples, double sampleRate);

    /** Returns the TimeDuration as a number of seconds. */
    constexpr double inSeconds() const;

private:
    double seconds = 0.0;
};

/** Converts a TimeDuration to a number of samples. */
constexpr int64_t toSamples (TimeDuration, double sampleRate);

//==============================================================================
/** Adds two TimeDurations together. */
constexpr TimeDuration operator+ (const TimeDuration&, const TimeDuration&);

/** Adds a time to a TimeDuration. */
constexpr TimeDuration operator+ (const TimeDuration&, std::chrono::duration<double>);

/** Adds a TimeDuration to a TimePosition. */
constexpr TimePosition operator+ (const TimePosition&, const TimeDuration&);

/** Adds a time to a TimePosition. */
constexpr TimePosition operator+ (const TimePosition&, std::chrono::duration<double>);

/** Subtracts a TimePosition from another one, returning the duration bewteen them. */
constexpr TimeDuration operator- (const TimePosition&, const TimePosition&);

/** Subtracts a TimeDuration from another one. */
constexpr TimeDuration operator- (const TimeDuration&, const TimeDuration&);

/** Subtracts a time from a TimeDuration. */
constexpr TimeDuration operator- (const TimeDuration&, std::chrono::duration<double>);

/** Subtracts a TimeDuration from a TimePosition. */
constexpr TimePosition operator- (const TimePosition&, const TimeDuration&);

/** Subtracts a time from a TimePosition. */
constexpr TimePosition operator- (const TimePosition&, std::chrono::duration<double>);

//==============================================================================
/** Compares two TimePositions. */
constexpr bool operator== (const TimePosition&, const TimePosition&);

/** Compares two TimeDurations. */
constexpr bool operator!= (const TimePosition&, const TimePosition&);

/** Compares two TimeDurations. */
constexpr bool operator== (const TimeDuration&, const TimeDuration&);

/** Compares two TimeDurations. */
constexpr bool operator!= (const TimeDuration&, const TimeDuration&);

/** Compares two TimePositions. */
constexpr bool operator< (const TimePosition&, const TimePosition&);

/** Compares two TimePosition. */
constexpr bool operator<= (const TimePosition&, const TimePosition&);

/** Compares two TimePosition. */
constexpr bool operator> (const TimePosition&, const TimePosition&);

/** Compares two TimePosition. */
constexpr bool operator>= (const TimePosition&, const TimePosition&);

/** Compares two TimeDurations. */
constexpr bool operator< (const TimeDuration&, const TimeDuration&);

/** Compares two TimeDurations. */
constexpr bool operator<= (const TimeDuration&, const TimeDuration&);

/** Compares two TimeDurations. */
constexpr bool operator> (const TimeDuration&, const TimeDuration&);

/** Compares two TimeDurations. */
constexpr bool operator>= (const TimeDuration&, const TimeDuration&);

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
    constexpr BeatPosition() = default;

    /** Creates a copy of another BeatPosition. */
    constexpr BeatPosition (const BeatPosition&) = default;

    /** Create a BeatPosition from a number of beats. */
    template<typename T>
    static constexpr BeatPosition fromBeats (T positionInBeats);

    /** Returns the position as a number of beats. */
    constexpr double inBeats() const;

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
    constexpr BeatDuration() = default;

    /** Creates a copy of another BeatDuration. */
    constexpr BeatDuration (const BeatDuration&) = default;

    /** Create a BeatPosition from a number of beats. */
    template<typename T>
    static constexpr BeatDuration fromBeats (T durationInBeats);

    /** Returns the position as a number of beats. */
    constexpr double inBeats() const;

private:
    double numBeats = 0.0;
};


//==============================================================================
/** Adds two BeatDurations together. */
constexpr BeatDuration operator+ (const BeatDuration&, const BeatDuration&);

/** Adds a BeatDuration to a BeatPosition. */
constexpr BeatPosition operator+ (const BeatPosition&, const BeatDuration&);

/** Subtracts a BeatPosition from another one, returning the duration between them. */
constexpr BeatDuration operator- (const BeatPosition&, const BeatPosition&);

/** Subtracts a BeatDuration from another one. */
constexpr BeatDuration operator- (const BeatDuration&, const BeatDuration&);

/** Subtracts a BeatDuration from a BeatPosition. */
constexpr BeatPosition operator- (const BeatPosition&, const BeatDuration&);

//==============================================================================
/** Compares two BeatPositions. */
constexpr bool operator== (const BeatPosition&, const BeatPosition&);

/** Compares two BeatPositions. */
constexpr bool operator!= (const BeatPosition&, const BeatPosition&);

/** Compares two BeatDurations. */
constexpr bool operator== (const BeatDuration&, const BeatDuration&);

/** Compares two BeatDurations. */
constexpr bool operator!= (const BeatDuration&, const BeatDuration&);

/** Compares two BeatDurations. */
constexpr bool operator< (const BeatPosition&, const BeatPosition&);

/** Compares two BeatDurations. */
constexpr bool operator<= (const BeatPosition&, const BeatPosition&);

/** Compares two BeatDurations. */
constexpr bool operator> (const BeatPosition&, const BeatPosition&);

/** Compares two BeatDurations. */
constexpr bool operator>= (const BeatPosition&, const BeatPosition&);

/** Compares two BeatDurations. */
constexpr bool operator< (const BeatDuration&, const BeatDuration&);

/** Compares two BeatDurations. */
constexpr bool operator<= (const BeatDuration&, const BeatDuration&);

/** Compares two BeatDurations. */
constexpr bool operator> (const BeatDuration&, const BeatDuration&);

/** Compares two BeatDurations. */
constexpr bool operator>= (const BeatDuration&, const BeatDuration&);

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

inline constexpr TimePosition::TimePosition (std::chrono::duration<double> duration)
    : seconds (duration.count())
{
}

template<typename T>
inline constexpr TimePosition TimePosition::fromSeconds (T positionInSeconds)
{
    TimePosition pos;
    pos.seconds = static_cast<double> (positionInSeconds);
    return pos;
}

template<typename IntType>
inline constexpr TimePosition TimePosition::fromSamples (IntType samplePosition, double sampleRate)
{
    return TimePosition::fromSeconds (samplePosition / sampleRate);
}

inline constexpr double TimePosition::inSeconds() const
{
    return seconds;
}

inline constexpr int64_t toSamples (TimePosition p, double sampleRate)
{
    return static_cast<int64_t> ((p.inSeconds() * sampleRate)
                                 + (p.inSeconds() >= 0.0 ? 0.5 : -0.5));
}

//==============================================================================
inline constexpr TimeDuration::TimeDuration (std::chrono::duration<double> duration)
    : seconds (duration.count())
{
}

template<typename T>
inline constexpr TimeDuration TimeDuration::fromSeconds (T positionInSeconds)
{
    TimeDuration pos;
    pos.seconds = static_cast<double> (positionInSeconds);
    return pos;
}

template<typename IntType>
inline constexpr TimeDuration TimeDuration::fromSamples (IntType samplePosition, double sampleRate)
{
    return TimeDuration::fromSeconds (samplePosition / sampleRate);
}

inline constexpr double TimeDuration::inSeconds() const
{
    return seconds;
}

inline constexpr int64_t toSamples (TimeDuration p, double sampleRate)
{
    return static_cast<int64_t> ((p.inSeconds() * sampleRate)
                                 + (p.inSeconds() >= 0.0 ? 0.5 : -0.5));
}


//==============================================================================
inline constexpr TimeDuration operator+ (const TimeDuration& t1, const TimeDuration& t2)
{
    return TimeDuration::fromSeconds (t1.inSeconds() + t2.inSeconds());
}

inline constexpr TimePosition operator+ (const TimePosition& t1, const TimeDuration& t2)
{
    return TimePosition::fromSeconds (t1.inSeconds() + t2.inSeconds());
}

inline constexpr TimeDuration operator- (const TimePosition& t1, const TimePosition& t2)
{
    return TimeDuration::fromSeconds (t1.inSeconds() - t2.inSeconds());
}

inline constexpr TimeDuration operator- (const TimeDuration& t1, const TimeDuration& t2)
{
    return TimeDuration::fromSeconds (t1.inSeconds() - t2.inSeconds());
}

inline constexpr TimePosition operator- (const TimePosition& t1, const TimeDuration& t2)
{
    return TimePosition::fromSeconds (t1.inSeconds() - t2.inSeconds());
}

inline constexpr TimeDuration operator+ (const TimeDuration& t1, std::chrono::duration<double> t2)
{
    return TimeDuration::fromSeconds (t1.inSeconds() + t2.count());
}

inline constexpr TimePosition operator+ (const TimePosition& t1, std::chrono::duration<double> t2)
{
    return TimePosition::fromSeconds (t1.inSeconds() + t2.count());
}

inline constexpr TimeDuration operator- (const TimeDuration& t1, std::chrono::duration<double> t2)
{
    return TimeDuration::fromSeconds (t1.inSeconds() - t2.count());
}

inline constexpr TimePosition operator- (const TimePosition& t1, std::chrono::duration<double> t2)
{
    return TimePosition::fromSeconds (t1.inSeconds() - t2.count());
}

inline constexpr bool operator== (const TimePosition& t1, const TimePosition& t2)   { return t1.inSeconds() == t2.inSeconds(); }
inline constexpr bool operator!= (const TimePosition& t1, const TimePosition& t2)   { return ! (t1 == t2); }

inline constexpr bool operator== (const TimeDuration& t1, const TimeDuration& t2)   { return t1.inSeconds() == t2.inSeconds(); }
inline constexpr bool operator!= (const TimeDuration& t1, const TimeDuration& t2)   { return ! (t1 == t2); }

inline constexpr bool operator<     (const TimePosition& t1, const TimePosition& t2)    { return t1.inSeconds() < t2.inSeconds(); }
inline constexpr bool operator<=    (const TimePosition& t1, const TimePosition& t2)    { return t1.inSeconds() <= t2.inSeconds(); }
inline constexpr bool operator>     (const TimePosition& t1, const TimePosition& t2)    { return t1.inSeconds() > t2.inSeconds(); }
inline constexpr bool operator>=    (const TimePosition& t1, const TimePosition& t2)    { return t1.inSeconds() >= t2.inSeconds(); }

inline constexpr bool operator<     (const TimeDuration& t1, const TimeDuration& t2)    { return t1.inSeconds() < t2.inSeconds(); }
inline constexpr bool operator<=    (const TimeDuration& t1, const TimeDuration& t2)    { return t1.inSeconds() <= t2.inSeconds(); }
inline constexpr bool operator>     (const TimeDuration& t1, const TimeDuration& t2)    { return t1.inSeconds() > t2.inSeconds(); }
inline constexpr bool operator>=    (const TimeDuration& t1, const TimeDuration& t2)    { return t1.inSeconds() >= t2.inSeconds(); }

//==============================================================================
template<typename T>
inline constexpr BeatPosition BeatPosition::fromBeats (T positionInBeats)
{
    BeatPosition pos;
    pos.numBeats = static_cast<double> (positionInBeats);
    return pos;
}

inline constexpr double BeatPosition::inBeats() const
{
    return numBeats;
}


//==============================================================================
//==============================================================================
template<typename T>
inline constexpr BeatDuration BeatDuration::fromBeats (T durationInBeats)
{
    BeatDuration pos;
    pos.numBeats = static_cast<double> (durationInBeats);
    return pos;
}

inline constexpr double BeatDuration::inBeats() const
{
    return numBeats;
}


//==============================================================================
inline constexpr BeatDuration operator+ (const BeatDuration& t1, const BeatDuration& t2)
{
    return BeatDuration::fromBeats (t1.inBeats() + t2.inBeats());
}

inline constexpr BeatPosition operator+ (const BeatPosition& t1, const BeatDuration& t2)
{
    return BeatPosition::fromBeats (t1.inBeats() + t2.inBeats());
}

inline constexpr BeatDuration operator- (const BeatPosition& t1, const BeatPosition& t2)
{
    return BeatDuration::fromBeats (t1.inBeats() - t2.inBeats());
}

inline constexpr BeatDuration operator- (const BeatDuration& t1, const BeatDuration& t2)
{
    return BeatDuration::fromBeats (t1.inBeats() - t2.inBeats());
}

inline constexpr BeatPosition operator- (const BeatPosition& t1, const BeatDuration& t2)
{
    return BeatPosition::fromBeats (t1.inBeats() - t2.inBeats());
}

inline constexpr bool operator== (const BeatPosition& t1, const BeatPosition& t2)   { return t1.inBeats() == t2.inBeats(); }
inline constexpr bool operator!= (const BeatPosition& t1, const BeatPosition& t2)   { return ! (t1 == t2); }

inline constexpr bool operator== (const BeatDuration& t1, const BeatDuration& t2)   { return t1.inBeats() == t2.inBeats(); }
inline constexpr bool operator!= (const BeatDuration& t1, const BeatDuration& t2)   { return ! (t1 == t2); }

inline constexpr bool operator<     (const BeatPosition& t1, const BeatPosition& t2)    { return t1.inBeats() < t2.inBeats(); }
inline constexpr bool operator<=    (const BeatPosition& t1, const BeatPosition& t2)    { return t1.inBeats() <= t2.inBeats(); }
inline constexpr bool operator>     (const BeatPosition& t1, const BeatPosition& t2)    { return t1.inBeats() > t2.inBeats(); }
inline constexpr bool operator>=    (const BeatPosition& t1, const BeatPosition& t2)    { return t1.inBeats() >= t2.inBeats(); }

inline constexpr bool operator<     (const BeatDuration& t1, const BeatDuration& t2)    { return t1.inBeats() < t2.inBeats(); }
inline constexpr bool operator<=    (const BeatDuration& t1, const BeatDuration& t2)    { return t1.inBeats() <= t2.inBeats(); }
inline constexpr bool operator>     (const BeatDuration& t1, const BeatDuration& t2)    { return t1.inBeats() > t2.inBeats(); }
inline constexpr bool operator>=    (const BeatDuration& t1, const BeatDuration& t2)    { return t1.inBeats() >= t2.inBeats(); }


} // namespace tracktion_graph
