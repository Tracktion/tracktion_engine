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

/**
*/
struct EditTimeRange
{
    /** */
    EditTimeRange() noexcept = default;
    /** */
    constexpr EditTimeRange (const EditTimeRange&) noexcept = default;
    /** */
    EditTimeRange& operator= (const EditTimeRange&) noexcept = default;
    /** */
    ~EditTimeRange() noexcept = default;

    /** */
    EditTimeRange (double start, double end) noexcept;
    /** */
    EditTimeRange (juce::Range<double> timeRange) noexcept;

    //==============================================================================
    /** Returns the range that lies between two positions (in either order). */
    static EditTimeRange between (double time1, double time2) noexcept;

    /** Returns a range with a given start and length. */
    static EditTimeRange withStartAndLength (double time1, double length) noexcept;

    /** Returns a range with the specified start position and a length of zero. */
    static EditTimeRange emptyRange (double start) noexcept;

    //==============================================================================
    /** */
    constexpr double getStart() const noexcept                  { return start; }
    /** */
    constexpr double getEnd() const noexcept                    { return end; }
    /** */
    constexpr double getLength() const noexcept                 { return end - start; }
    /** */
    constexpr double getCentre() const noexcept                 { return (start + end) / 2.0; }
    /** */
    double clipValue (double value) const noexcept              { return juce::jlimit (start, end, value); }

    /** */
    constexpr bool isEmpty() const noexcept                     { return end <= start; }

    /** */
    constexpr juce::Range<double> toRange() const noexcept      { return { start, end }; }

    //==============================================================================
    /** */
    constexpr bool overlaps (const EditTimeRange& other) const noexcept { return other.start < end && start < other.end; }
    /** */
    constexpr bool contains (const EditTimeRange& other) const noexcept { return other.start >= start && other.end <= end; }
    /** */
    constexpr bool contains (double time) const noexcept                { return time >= start && time < end; }
    /** */
    constexpr bool containsInclusive (double time) const noexcept       { return time >= start && time <= end; }

    //==============================================================================
    /** */
    EditTimeRange getUnionWith (const EditTimeRange& other) const noexcept;
    /** */
    EditTimeRange getIntersectionWith (const EditTimeRange& other) const noexcept;
    /** */
    EditTimeRange rescaled (double anchorTime, double factor) const noexcept;
    /** */
    EditTimeRange constrainRange (const EditTimeRange& rangeToConstrain) const noexcept;
    /** */
    EditTimeRange expanded (double amount) const noexcept;
    /** */
    EditTimeRange reduced (double amount) const noexcept;
    /** */
    EditTimeRange movedToStartAt (double newStart) const noexcept;
    /** */
    EditTimeRange movedToEndAt (double newEnd) const noexcept;
    /** */
    EditTimeRange withStart (double newStart) const noexcept;
    /** */
    EditTimeRange withEnd (double newEnd) const noexcept;
    /** */
    EditTimeRange withLength (double newLength) const noexcept;

    //==============================================================================
    /** */
    constexpr bool operator== (const EditTimeRange& other) const noexcept   { return start == other.start && end == other.end; }
    /** */
    constexpr bool operator!= (const EditTimeRange& other) const noexcept   { return ! operator== (other); }

    /** */
    EditTimeRange operator+ (double amount) const noexcept                  { return { start + amount, end + amount }; }
    /** */
    EditTimeRange operator- (double amount) const noexcept                  { return operator+ (-amount); }

    /** */
    EditTimeRange operator+= (double amount) noexcept 
    {
        start += amount;
        end += amount;
        return *this;
    }

    /** */
    EditTimeRange operator-= (double amount) noexcept
    {
        start -= amount;
        end -= amount;
        return *this;
    }

    //==============================================================================
    double start = 0.0, end = 0.0;
};

//==============================================================================
/**
*/
struct ClipPosition
{
    /** */
    ClipPosition() noexcept = default;
    /** */
    constexpr ClipPosition (const ClipPosition&) noexcept = default;
    /** */
    ClipPosition& operator= (const ClipPosition&) noexcept = default;
    /** */
    ~ClipPosition() noexcept = default;

    //==============================================================================
    /** */
    constexpr double getStart() const noexcept                              { return time.start; }
    /** */
    constexpr double getEnd() const noexcept                                { return time.end; }
    /** */
    constexpr double getLength() const noexcept                             { return time.getLength(); }
    /** */
    constexpr double getOffset() const noexcept                             { return offset; }
    /** */
    constexpr double getStartOfSource() const noexcept                      { return time.start - offset; }

    //==============================================================================
    /** */
    ClipPosition rescaled (double anchorTime, double factor) const noexcept;

    //==============================================================================
    /** */
    constexpr bool operator== (const ClipPosition& other) const noexcept    { return offset == other.offset && time == other.time; }
    /** */
    constexpr bool operator!= (const ClipPosition& other) const noexcept    { return ! operator== (other); }

    //==============================================================================
    EditTimeRange time;
    double offset = 0.0;
};

} // namespace tracktion_engine
