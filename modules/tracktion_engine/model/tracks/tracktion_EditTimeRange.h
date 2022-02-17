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

/**
*/
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
/**
*/
struct ClipPosition
{
    TimeRange time;
    TimePosition offset;

    TimePosition getStart() const             { return time.getStart(); }
    TimePosition getEnd() const               { return time.getEnd(); }
    TimeDuration getLength() const            { return time.getLength(); }
    TimePosition getOffset() const            { return offset; }
    TimePosition getStartOfSource() const     { return time.getStart() - TimeDuration::fromSeconds (offset.inSeconds()); }

    bool operator== (const ClipPosition& other) const  { return offset == other.offset && time == other.time; }
    bool operator!= (const ClipPosition& other) const  { return ! operator== (other); }

    ClipPosition rescaled (TimePosition anchorTime, double factor) const;
};

/** @temporary */
inline TimeRange toTimeRange (EditTimeRange r)
{
    return { TimePosition::fromSeconds (r.getStart()), TimePosition::fromSeconds (r.getEnd()) };
}

/** @temporary */
inline EditTimeRange toEditTimeRange (TimeRange r)
{
    return { r.getStart().inSeconds(), r.getEnd().inSeconds() };
}

}} // namespace tracktion { inline namespace engine
