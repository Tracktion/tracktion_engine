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

struct TimelineClock
{
    typedef std::chrono::duration<double, std::chrono::seconds::period> duration;
    typedef duration::rep                                               rep;
    typedef duration::period                                            period;
    typedef std::chrono::time_point<TimelineClock>                      time_point;
    static const bool is_steady =                                       false;

    static time_point now() noexcept
    {
        return {};
    }
};

using TimelinePoint = std::chrono::time_point<TimelineClock>;
using Duration = TimelinePoint::duration;


//==============================================================================
//==============================================================================
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

    /** Returns the TimePosition as a number of seconds. */
    double inSeconds() const;

private:
    double seconds = 0.0;
};


//==============================================================================
//==============================================================================
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

    /** Returns the TimeDuration as a number of seconds. */
    double inSeconds() const;

private:
    double seconds = 0.0;
};


//==============================================================================
/** Adds two TimeDurations together. */
TimeDuration operator+ (const TimeDuration&, const TimeDuration&);

/** Adds a TimeDuration to a TimePosition. */
TimePosition operator+ (const TimePosition&, const TimeDuration&);

/** Subtracts a TimeDuration from another one. */
TimeDuration operator- (const TimeDuration&, const TimeDuration&);

/** Subtracts a TimeDuration from a TimePosition. */
TimePosition operator- (const TimePosition&, const TimeDuration&);


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

inline double TimePosition::inSeconds() const
{
    return seconds;
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

inline double TimeDuration::inSeconds() const
{
    return seconds;
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

} // namespace tracktion_graph
