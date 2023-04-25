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

namespace tracktion { inline namespace core
{

template<typename PositionType>
struct RangeType;

//==============================================================================
//==============================================================================
/**
    A RangeType based on real time (i.e. seconds).
    This uses the TimePosition and TimeDuration objects to represent a range in time.
*/
using TimeRange = RangeType<TimePosition>;

/**
    A RangeType based on beats.
    This uses the BeatPosition and BeatDuration objects to represent a range in beats.

    The length in time will vary depending on the tempo and time signature applied
    to the beats.
*/
using BeatRange = RangeType<BeatPosition>;


//==============================================================================
/** Converts a TimeRange to a range of samples. */
[[ nodiscard ]] juce::Range<int64_t> toSamples (TimeRange, double sampleRate);

/** Creates a TimeRange from a range of samples. */
[[ nodiscard ]] TimeRange timeRangeFromSamples (juce::Range<int64_t> sampleRange, double sampleRate);

/** Creates a TimeRange from a range of seconds. */
template<typename SourceRangeType>
[[ nodiscard ]] TimeRange timeRangeFromSeconds (SourceRangeType);

//==============================================================================
//==============================================================================
/**
    Describes a range of two positions with a duration separating them.
    The end can never be before the start.

    This is templated on the units the range should be in. However, you should
    never have to instantiate a range using the PositionType explicitly. Instead,
    use one of the explict template instantiations: TimeRange and BeatRange

    @see TimeRange, BeatRange
*/
template<typename PositionType>
struct RangeType
{
    using Position = PositionType;                      /**< The position type of the range. */
    using Duration = typename Position::DurationType;   /**< The duration type of the range. */

    //==============================================================================
    /** Creates an empty range. */
    RangeType() = default;

    /** Creates a copy of another range. */
    RangeType (const RangeType&) = default;

    /** Creates a copy of another range. */
    RangeType& operator= (const RangeType&) = default;

    /** Creates a Range from a start and and end position. */
    RangeType (Position start, Position end);

    /** Creates a Range from a start position and duration. */
    RangeType (Position start, Duration);

    /** Returns the range that lies between two positions (in either order). */
    static RangeType between (Position, Position);

    /** Returns a range with the specified start position and a length of zero. */
    static RangeType emptyRange (Position);

    //==============================================================================
    /** Returns the start of the range. */
    constexpr Position getStart() const;

    /** Returns the end of the range. */
    constexpr Position getEnd() const;

    /** Returns the length of the range. */
    constexpr Duration getLength() const;

    /** Returns the centre position of the range. */
    constexpr Position getCentre() const;

    /** Clamps the given position to this range. */
    Position clipPosition (Position) const;

    //==============================================================================
    /** Returns true if this range has a 0 length duration. */
    bool isEmpty() const;

    /** Returns true if this range overlaps the provided one. */
    bool overlaps (const RangeType&) const;

    /** Returns true if this range contains the provided one. */
    bool contains (const RangeType&) const;

    /** Returns true if the given range intersects this one. */
    bool intersects (const RangeType&) const;

    /** Returns true if this range overlaps the provided position. */
    bool contains (Position) const;

    /** Returns true if this range contains the provided position even if it lies at the end position. */
    bool containsInclusive (Position) const;

    //==============================================================================
    /** Returns the range that contains both of these ranges. */
    [[ nodiscard ]] RangeType getUnionWith (RangeType) const;

    /** Returns the intersection of this range with the given one. */
    [[ nodiscard ]] RangeType getIntersectionWith (RangeType) const;

    /** Returns a range that has been expanded or contracted around the given position. */
    [[ nodiscard ]] RangeType rescaled (Position anchorTime, double factor) const;

    /** Returns a given range, after moving it forwards or backwards to fit it
        within this range.
    */
    [[ nodiscard ]] RangeType constrainRange (RangeType) const;

    /** Expands the start and end of this range by the given ammount. */
    [[ nodiscard ]] RangeType expanded (Duration) const;

    /** Reduces the start and end of this range by the given ammount. */
    [[ nodiscard ]] RangeType reduced (Duration) const;

    /** Returns a range with the same duration as this one but a new start position. */
    [[ nodiscard ]] RangeType movedToStartAt (Position) const;

    /** Returns a range with the same duration as this one but a new end position. */
    [[ nodiscard ]] RangeType movedToEndAt (Position) const;

    /** Returns a range with the same end position as this one but a new start position. */
    [[ nodiscard ]] RangeType withStart (Position) const;

    /** Returns a range with the same start position as this one but a new end position. */
    [[ nodiscard ]] RangeType withEnd (Position) const;

    /** Returns a range with the same start position as this one but a new duration length. */
    [[ nodiscard ]] RangeType withLength (Duration) const;

private:
    Position start, end;

    void checkInvariants() const;
};

//==============================================================================
/** Compares two ranges to check if they are equal. */
template<typename PositionType>
[[ nodiscard ]] bool operator== (const RangeType<PositionType>&, const RangeType<PositionType>&);

/** Compares two ranges to check if they are not equal. */
template<typename PositionType>
[[ nodiscard ]] bool operator!= (const RangeType<PositionType>&, const RangeType<PositionType>&);

/** Adds a duration to a range, moving it forwards. */
template<typename PositionType>
[[ nodiscard ]] RangeType<PositionType> operator+ (const RangeType<PositionType>&, typename RangeType<PositionType>::Duration);

/** Subtracts a duration to a range, moving it backwards. */
template<typename PositionType>
[[ nodiscard ]] RangeType<PositionType> operator- (const RangeType<PositionType>&, typename RangeType<PositionType>::Duration);


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
template<typename PositionType>
PositionType fromUnderlyingType (double);

template<> inline TimePosition fromUnderlyingType<TimePosition> (double t) { return TimePosition::fromSeconds (t); }
template<> inline TimeDuration fromUnderlyingType<TimeDuration> (double t) { return TimeDuration::fromSeconds (t); }
inline double toUnderlyingType (TimePosition t) { return t.inSeconds(); }
inline double toUnderlyingType (TimeDuration t) { return t.inSeconds(); }

template<> inline BeatPosition fromUnderlyingType<BeatPosition> (double t) { return BeatPosition::fromBeats (t); }
template<> inline BeatDuration fromUnderlyingType<BeatDuration> (double t) { return BeatDuration::fromBeats (t); }
inline double toUnderlyingType (BeatPosition t) { return t.inBeats(); }
inline double toUnderlyingType (BeatDuration t) { return t.inBeats(); }

inline TimeRange timeRangeFromSamples (juce::Range<int64_t> r, double sampleRate)
{
    return { TimePosition::fromSamples (r.getStart(), sampleRate),
             TimePosition::fromSamples (r.getEnd(), sampleRate) };
}

template<typename SourceRangeType>
inline TimeRange timeRangeFromSeconds (SourceRangeType r)
{
    return { TimePosition::fromSeconds (r.getStart()),
             TimePosition::fromSeconds (r.getEnd()) };
}

template<typename PositionType>
inline RangeType<PositionType>::RangeType (Position s, Position e)
    : start (s), end (e)
{
    checkInvariants();
}

template<typename PositionType>
inline RangeType<PositionType>::RangeType (Position s, Duration d)
    : start (s), end (s + d)
{
    checkInvariants();
}

template<typename PositionType>
inline RangeType<PositionType> RangeType<PositionType>::between (Position p1, Position p2)
{
    return p1 < p2 ? RangeType (p1, p2)
                   : RangeType (p2, p1);
}

template<typename PositionType>
inline RangeType<PositionType> RangeType<PositionType>::emptyRange (Position p)
{
    return { p, p };
}

template<typename PositionType> inline constexpr typename RangeType<PositionType>::Position RangeType<PositionType>::getStart() const                          { return start; }
template<typename PositionType> inline constexpr typename RangeType<PositionType>::Position RangeType<PositionType>::getEnd() const                            { return end; }
template<typename PositionType> inline constexpr typename RangeType<PositionType>::Duration RangeType<PositionType>::getLength() const                         { return end - start; }
template<typename PositionType> inline constexpr typename RangeType<PositionType>::Position RangeType<PositionType>::getCentre() const                         { return fromUnderlyingType<Position> ((toUnderlyingType (start) + toUnderlyingType (end)) * 0.5); }
template<typename PositionType> inline typename RangeType<PositionType>::Position RangeType<PositionType>::clipPosition (Position position) const    { return juce::jlimit (start, end, position); }

template<typename PositionType> inline bool RangeType<PositionType>::isEmpty() const                                          { return end <= start; }
template<typename PositionType> inline bool RangeType<PositionType>::overlaps (const RangeType& other) const                  { return other.start < end && start < other.end; }
template<typename PositionType> inline bool RangeType<PositionType>::contains (const RangeType& other) const                  { return other.start >= start && other.end <= end; }
template<typename PositionType> inline bool RangeType<PositionType>::intersects (const RangeType& other) const                { return other.start < end && start < other.end; }

template<typename PositionType> inline bool RangeType<PositionType>::contains (Position time) const                           { return time >= start && time < end; }
template<typename PositionType> inline bool RangeType<PositionType>::containsInclusive (Position time) const                  { return time >= start && time <= end; }

template<typename PositionType>
inline RangeType<PositionType> RangeType<PositionType>::getUnionWith (RangeType o) const
{
    return { std::min (start, o.start),
             std::max (end, o.end) };
}

template<typename PositionType>
inline RangeType<PositionType> RangeType<PositionType>::getIntersectionWith (RangeType o) const
{
    auto newStart = std::max (start, o.start);
    return { newStart, std::max (newStart, std::min (end, o.end)) };
}

template<typename PositionType>
inline RangeType<PositionType> RangeType<PositionType>::rescaled (Position anchorTime, double factor) const
{
    jassert (factor > 0);
    return { anchorTime + fromUnderlyingType<Duration> (toUnderlyingType (start - anchorTime) * factor),
             anchorTime + fromUnderlyingType<Duration> (toUnderlyingType (end   - anchorTime) * factor) };
}

template<typename PositionType>
inline RangeType<PositionType> RangeType<PositionType>::constrainRange (RangeType rangeToConstrain) const
{
    auto otherLen = rangeToConstrain.getLength();

    return getLength() <= otherLen
            ? *this
            : rangeToConstrain.movedToStartAt (juce::jlimit (start, end - otherLen,
                                                             rangeToConstrain.getStart()));
}

template<typename PositionType>
inline RangeType<PositionType> RangeType<PositionType>::expanded (Duration amount) const
{
    jassert (amount >= Duration());
    return { start - amount, end + amount };
}

template<typename PositionType>
inline RangeType<PositionType> RangeType<PositionType>::reduced (Duration amount) const
{
    jassert (amount >= Duration());
    amount = std::min (amount, fromUnderlyingType<Duration> (toUnderlyingType (getLength()) / 2.0));
    return { start + amount, end - amount };
}

template<typename PositionType>
inline RangeType<PositionType> RangeType<PositionType>::movedToStartAt (Position newStart) const
{
    return { newStart, end + (newStart - start) };
}

template<typename PositionType>
inline RangeType<PositionType> RangeType<PositionType>::movedToEndAt (Position newEnd) const
{
    return { start + (newEnd - end), newEnd };
}

template<typename PositionType>
inline RangeType<PositionType> RangeType<PositionType>::withStart (Position newStart) const
{
    jassert (newStart <= end);
    return { newStart, std::max (end, newStart) };
}

template<typename PositionType>
inline RangeType<PositionType> RangeType<PositionType>::withEnd (Position newEnd) const
{
    jassert (newEnd >= start);
    return { std::min (start, newEnd), newEnd };
}

template<typename PositionType>
inline RangeType<PositionType> RangeType<PositionType>::withLength (Duration newLength) const
{
    jassert (newLength >= Duration());
    return { start, start + std::max (Duration(), newLength) };
}


template<typename PositionType>
inline void RangeType<PositionType>::checkInvariants() const
{
    jassert (end >= start);
}

inline juce::Range<int64_t> toSamples (TimeRange r, double sampleRate)
{
    return { toSamples (r.getStart(), sampleRate),
             toSamples (r.getEnd(), sampleRate) };
}

template<typename PositionType>
inline bool operator== (const RangeType<PositionType>& r1, const RangeType<PositionType>& r2)  { return r1.getStart() == r2.getStart() && r1.getEnd() == r2.getEnd(); }
template<typename PositionType>
inline bool operator!= (const RangeType<PositionType>& r1, const RangeType<PositionType>& r2)  { return ! operator== (r1, r2); }

template<typename PositionType>
inline RangeType<PositionType> operator+ (const RangeType<PositionType>& r, typename RangeType<PositionType>::Duration d)    { return RangeType<PositionType> (r.getStart() + d, r.getEnd() + d); }
template<typename PositionType>
inline RangeType<PositionType> operator- (const RangeType<PositionType>& r, typename RangeType<PositionType>::Duration d)    { return RangeType<PositionType> (r.getStart() - d, r.getEnd() - d); }

}} // namespace tracktion


//==============================================================================
//==============================================================================
template<>
struct std::hash<tracktion::TimeRange>
{
    std::size_t operator()(const tracktion::TimeRange tr) const noexcept
    {
        std::size_t h1 = std::hash<double>{} (tr.getStart().inSeconds());
        std::size_t h2 = std::hash<double>{} (tr.getEnd().inSeconds());

        return tracktion::hash (h1, h2);
    }
};

template<>
struct std::hash<tracktion::BeatRange>
{
    std::size_t operator()(const tracktion::BeatRange tr) const noexcept
    {
        std::size_t h1 = std::hash<double>{} (tr.getStart().inBeats());
        std::size_t h2 = std::hash<double>{} (tr.getEnd().inBeats());

        return tracktion::hash (h1, h2);
    }
};
