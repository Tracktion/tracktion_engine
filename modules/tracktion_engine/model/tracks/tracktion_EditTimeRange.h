/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
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

    static EditTimeRange between (double time1, double time2);

    EditTimeRange operator+ (double amount) const;
    EditTimeRange operator- (double amount) const       { return operator+ (-amount); }
};

//==============================================================================
/**
*/
struct ClipPosition
{
    EditTimeRange time;
    double offset = 0;

    double getStart() const             { return time.start; }
    double getEnd() const               { return time.end; }
    double getLength() const            { return time.getLength(); }
    double getOffset() const            { return offset; }
    double getStartOfSource() const     { return time.start - offset; }

    bool operator== (const ClipPosition& other) const  { return offset == other.offset && time == other.time; }
    bool operator!= (const ClipPosition& other) const  { return ! operator== (other); }

    ClipPosition rescaled (double anchorTime, double factor) const;
};

} // namespace tracktion_engine
