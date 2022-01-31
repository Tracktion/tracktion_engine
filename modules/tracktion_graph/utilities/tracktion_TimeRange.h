/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "tracktion_Time.h"

namespace tracktion_graph
{

/**
    Describes a range of two positions with a duration separating them.
    The end can never be before the start.
*/
struct TimeRange
{
    //==============================================================================
    /** Creates an empty range. */
    TimeRange() = default;

    /** Creates a copy of another range. */
    TimeRange (const TimeRange&) = default;

    /** Creates a copy of another range. */
    TimeRange& operator= (const TimeRange&) = default;

    /** Creates a Range from a start and and end position. */
    TimeRange (TimePosition start, TimePosition end);

    /** Creates a Range from a start position and duration. */
    TimeRange (TimePosition start, TimeDuration duration);

    /** Returns the range that lies between two positions (in either order). */
    static TimeRange between (TimePosition, TimePosition);

    /** Returns a range with the specified start position and a length of zero. */
    static TimeRange emptyRange (TimePosition);

    //==============================================================================
    /** Returns the start of the range. */
    TimePosition getStart() const;

    /** Returns the end of the range. */
    TimePosition getEnd() const;

    /** Returns the length of the range. */
    TimeDuration getLength() const;

    /** Returns the centre position of the range. */
    TimePosition getCentre() const;

    /** Clamps the given position to this range. */
    TimePosition clipPosition (TimePosition) const;

    //==============================================================================
    /** Returns true if this range has a 0 length duration. */
    bool isEmpty() const;

    /** Returns true if this range overlaps the provided one. */
    bool overlaps (const TimeRange&) const;

    /** Returns true if this range contains the provided one. */
    bool contains (const TimeRange&) const;

    /** Returns true if this range overlaps the provided position. */
    bool contains (TimePosition) const;

    /** Returns true if this range contains the provided position even if it lies at the end position. */
    bool containsInclusive (TimePosition) const;

    //==============================================================================
    /** Returns the range that contains both of these ranges. */
    TimeRange getUnionWith (TimeRange) const;

    /** Returns the intersection of this range with the given one. */
    TimeRange getIntersectionWith (TimeRange) const;

    /** Returns a range that has been expanded or contracted around the given position. */
    TimeRange rescaled (TimePosition anchorTime, double factor) const;

    /** Returns a given range, after moving it forwards or backwards to fit it
        within this range.
    */
    TimeRange constrainRange (TimeRange) const;

    /** Expands the start and end of this range by the given ammount. */
    TimeRange expanded (TimeDuration) const;

    /** Reduces the start and end of this range by the given ammount. */
    TimeRange reduced (TimeDuration) const;

    /** Returns a range with the same duration as this one but a new start position. */
    TimeRange movedToStartAt (TimePosition) const;

    /** Returns a range with the same duration as this one but a new end position. */
    TimeRange movedToEndAt (TimePosition) const;

    /** Returns a range with the same end position as this one but a new start position. */
    TimeRange withStart (TimePosition) const;

    /** Returns a range with the same start position as this one but a new end position. */
    TimeRange withEnd (TimePosition) const;

    /** Returns a range with the same start position as this one but a new duration length. */
    TimeRange withLength (TimeDuration) const;

private:
    TimePosition start, end;

    void checkInvariants() const;
};

//==============================================================================
/** Compares two ranges to check if they are equal. */
bool operator== (const TimeRange&, const TimeRange&);

/** Compares two ranges to check if they are not equal. */
bool operator!= (const TimeRange&, const TimeRange&);

/** Adds a duration to a range, moving it forwards. */
TimeRange operator+ (const TimeRange&, TimeDuration);

/** Subtracts a duration to a range, moving it backwards. */
TimeRange operator- (const TimeRange&, TimeDuration);


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
inline TimeRange::TimeRange (TimePosition s, TimePosition e)
    : start (s), end (e)
{
    checkInvariants();
}

inline TimeRange::TimeRange (TimePosition s, TimeDuration d)
    : start (s), end (s + d)
{
    checkInvariants();
}

inline TimeRange TimeRange::between (TimePosition p1, TimePosition p2)
{
    return p1 < p2 ? TimeRange (p1, p2)
                   : TimeRange (p2, p1);
}

inline TimeRange TimeRange::emptyRange (TimePosition p)
{
    return { p, p };
}

inline TimePosition TimeRange::getStart() const                                 { return start; }
inline TimePosition TimeRange::getEnd() const                                   { return end; }
inline TimeDuration TimeRange::getLength() const                                { return end - start; }
inline TimePosition TimeRange::getCentre() const                                { return TimePosition::fromSeconds ((start.inSeconds() + end.inSeconds()) * 0.5); }
inline TimePosition TimeRange::clipPosition (TimePosition position) const       { return juce::jlimit (start, end, position); }

inline bool TimeRange::isEmpty() const                                          { return end <= start; }

inline bool TimeRange::overlaps (const TimeRange& other) const                  { return other.start < end && start < other.end; }
inline bool TimeRange::contains (const TimeRange& other) const                  { return other.start >= start && other.end <= end; }
inline bool TimeRange::contains (TimePosition time) const                       { return time >= start && time < end; }
inline bool TimeRange::containsInclusive (TimePosition time) const              { return time >= start && time <= end; }

inline TimeRange TimeRange::getUnionWith (TimeRange o) const
{
    return { std::min (start, o.start),
             std::max (end, o.end) };
}

inline TimeRange TimeRange::getIntersectionWith (TimeRange o) const
{
    auto newStart = std::max (start, o.start);
    return { newStart, std::max (newStart, std::min (end, o.end)) };
}

inline TimeRange TimeRange::rescaled (TimePosition anchorTime, double factor) const
{
    jassert (factor > 0);
    return { anchorTime + TimeDuration::fromSeconds ((start - anchorTime).inSeconds() * factor),
             anchorTime + TimeDuration::fromSeconds ((end   - anchorTime).inSeconds() * factor) };
}

inline TimeRange TimeRange::constrainRange (TimeRange rangeToConstrain) const
{
    auto otherLen = rangeToConstrain.getLength();

    return getLength() <= otherLen
            ? *this
            : rangeToConstrain.movedToStartAt (juce::jlimit (start, end - otherLen,
                                                             rangeToConstrain.getStart()));
}

inline TimeRange TimeRange::expanded (TimeDuration amount) const
{
    jassert (amount >= TimeDuration());
    return { start - amount, end + amount };
}

inline TimeRange TimeRange::reduced (TimeDuration amount) const
{
    jassert (amount >= TimeDuration());
    amount = std::min (amount, TimeDuration::fromSeconds (getLength().inSeconds() / 2.0));
    return { start + amount, end - amount };
}

inline TimeRange TimeRange::movedToStartAt (TimePosition newStart) const
{
    return { newStart, end + (newStart - start) };
}

inline TimeRange TimeRange::movedToEndAt (TimePosition newEnd) const
{
    return { start + (newEnd - end), newEnd };
}

inline TimeRange TimeRange::withStart (TimePosition newStart) const
{
    jassert (newStart <= end);
    return { newStart, std::max (end, newStart) };
}

inline TimeRange TimeRange::withEnd (TimePosition newEnd) const
{
    jassert (newEnd >= start);
    return { std::min (start, newEnd), newEnd };
}

inline TimeRange TimeRange::withLength (TimeDuration newLength) const
{
    jassert (newLength >= TimeDuration());
    return { start, start + std::max (TimeDuration(), newLength) };
}


inline void  TimeRange::checkInvariants() const
{
    jassert (end > start);
}


inline bool operator== (const TimeRange& r1, const TimeRange& r2)  { return r1.getStart() == r2.getStart() && r1.getEnd() == r2.getEnd(); }
inline bool operator!= (const TimeRange& r1, const TimeRange& r2)  { return ! operator== (r1, r2); }

inline TimeRange operator+ (const TimeRange& r, TimeDuration d)    { return TimeRange (r.getStart() + d, r.getEnd() + d); }
inline TimeRange operator- (const TimeRange& r, TimeDuration d)    { return TimeRange (r.getStart() - d, r.getEnd() - d); }

} // namespace tracktion_graph
