/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine { namespace legacy
{

struct EditTimeRange
{
    EditTimeRange() = default;
    EditTimeRange (const EditTimeRange&) = default;
    EditTimeRange& operator= (const EditTimeRange&) = default;

    EditTimeRange (double start, double end);
    EditTimeRange (juce::Range<double> timeRange);

    /** Returns the range that lies between two positions (in either order). */
    static EditTimeRange between (double time1, double time2);

    /** Returns a range with a given start and length. */
    static EditTimeRange withStartAndLength (double time1, double length);

    /** Returns a range with the specified start position and a length of zero. */
    static EditTimeRange emptyRange (double start);

    double start = 0;
    double end = 0;

    double getStart() const                 { return start; }
    double getEnd() const                   { return end; }
    double getLength() const                { return end - start; }
    double getCentre() const                { return (start + end) * 0.5; }
    double clipValue (double value) const   { return juce::jlimit (start, end, value); }

    bool isEmpty() const                    { return end <= start; }

    bool operator== (const EditTimeRange& other) const  { return start == other.start && end == other.end; }
    bool operator!= (const EditTimeRange& other) const  { return ! operator== (other); }

    bool overlaps (const EditTimeRange& other) const    { return other.start < end && start < other.end; }
    bool contains (const EditTimeRange& other) const    { return other.start >= start && other.end <= end; }
    bool contains (double time) const                   { return time >= start && time < end; }
    bool containsInclusive (double time) const          { return time >= start && time <= end; }

    EditTimeRange getUnionWith (EditTimeRange other) const;
    EditTimeRange getIntersectionWith (EditTimeRange other) const;
    EditTimeRange rescaled (double anchorTime, double factor) const;
    EditTimeRange constrainRange (EditTimeRange rangeToConstrain) const;
    EditTimeRange expanded (double amount) const;
    EditTimeRange reduced (double amount) const;
    EditTimeRange movedToStartAt (double newStart) const;
    EditTimeRange movedToEndAt (double newEnd) const;
    EditTimeRange withStart (double newStart) const;
    EditTimeRange withEnd (double newEnd) const;
    EditTimeRange withLength (double newLength) const;

    EditTimeRange operator+ (double amount) const;
    EditTimeRange operator- (double amount) const       { return operator+ (-amount); }
};

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

inline EditTimeRange::EditTimeRange (double s, double e) : start (s), end (e)
{
    jassert (s <= e);
}

inline EditTimeRange::EditTimeRange (juce::Range<double> timeRange)
    : EditTimeRange (timeRange.getStart(), timeRange.getEnd())
{
}

inline EditTimeRange EditTimeRange::between (double time1, double time2)
{
    return time1 <= time2 ? EditTimeRange (time1, time2)
                          : EditTimeRange (time2, time1);
}

inline EditTimeRange EditTimeRange::withStartAndLength (double startValue, double length)
{
    jassert (length >= 0.0);
    return juce::Range<double>::withStartAndLength (startValue, length);
}

inline EditTimeRange EditTimeRange::emptyRange (double start)
{
    return EditTimeRange (start, start);
}

inline EditTimeRange EditTimeRange::getUnionWith (EditTimeRange other) const
{
    return { std::min (start, other.start),
             std::max (end, other.end) };
}

inline EditTimeRange EditTimeRange::getIntersectionWith (EditTimeRange other) const
{
    auto newStart = std::max (start, other.start);
    return { newStart, std::max (newStart, std::min (end, other.end)) };
}

inline EditTimeRange EditTimeRange::constrainRange (EditTimeRange rangeToConstrain) const
{
    auto otherLen = rangeToConstrain.getLength();

    return getLength() <= otherLen
            ? *this
            : rangeToConstrain.movedToStartAt (juce::jlimit (start, end - otherLen,
                                                             rangeToConstrain.getStart()));
}

inline EditTimeRange EditTimeRange::rescaled (double anchorTime, double factor) const
{
    jassert (factor > 0);
    return { anchorTime + (start - anchorTime) * factor,
             anchorTime + (end   - anchorTime) * factor };
}

inline EditTimeRange EditTimeRange::expanded (double amount) const
{
    jassert (amount >= 0);
    return { start - amount, end + amount };
}

inline EditTimeRange EditTimeRange::reduced (double amount) const
{
    jassert (amount >= 0);
    amount = std::min (amount, getLength() / 2.0);
    return { start + amount, end - amount };
}

inline EditTimeRange EditTimeRange::movedToStartAt (double newStart) const
{
    return { newStart, end + (newStart - start) };
}

inline EditTimeRange EditTimeRange::movedToEndAt (double newEnd) const
{
    return { start + (newEnd - end), newEnd };
}

inline EditTimeRange EditTimeRange::withStart (double newStart) const
{
    jassert (newStart <= end);
    return { newStart, std::max (end, newStart) };
}

inline EditTimeRange EditTimeRange::withEnd (double newEnd) const
{
    jassert (newEnd >= start);
    return { std::min (start, newEnd), newEnd };
}

inline EditTimeRange EditTimeRange::withLength (double newLength) const
{
    jassert (newLength >= 0);
    return { start, start + std::max (0.0, newLength) };
}

inline EditTimeRange EditTimeRange::operator+ (double amount) const
{
    return { start + amount, end + amount };
}

} // namespace legacy

/** @temporary */
inline TimeRange toTimeRange (legacy::EditTimeRange r)
{
    return { TimePosition::fromSeconds (r.getStart()), TimePosition::fromSeconds (r.getEnd()) };
}

/** @temporary */
inline legacy::EditTimeRange toEditTimeRange (TimeRange r)
{
    return { r.getStart().inSeconds(), r.getEnd().inSeconds() };
}

}} // namespace tracktion { inline namespace engine
