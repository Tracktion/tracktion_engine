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

/** Converts a TimePosition to a BeatPosition given a TempoSequence. */
BeatPosition toBeats (TimePosition, const TempoSequence&);

/** Converts a BeatPosition to a TimePosition given a TempoSequence. */
TimePosition toTime (BeatPosition, const TempoSequence&);

//==============================================================================
/** Converts a TimeRange to a BeatRange given a TempoSequence. */
BeatRange toBeats (TimeRange, const TempoSequence&);

/** Converts a BeatRange to a TimeRange given a TempoSequence. */
TimeRange toTime (BeatRange, const TempoSequence&);


//==============================================================================
//==============================================================================
/**
    Represents a time point in an Edit stored as either time or beats.
    This is basically a variant to simplify APIs that can accept either time base.
*/
struct EditTime
{
    /** Creates an empty EditTime, starting at 0. */
    EditTime();

    /** Creates an EditTime from a TimePosition. */
    EditTime (TimePosition);

    /** Creates an EditTime from a BeatPosition. */
    EditTime (BeatPosition);

private:
    friend TimePosition toTime (EditTime, const TempoSequence&);
    friend BeatPosition toBeats (EditTime, const TempoSequence&);

    std::variant<TimePosition, BeatPosition> position;
};

//==============================================================================
/** Converts an EditTime to a TimePosition.
    N.B. This may be a slow operation if this was created using a BeatPosition.
*/
TimePosition toTime (EditTime, const TempoSequence&);

/** Converts an EditTime to a BeatPosition.
    N.B. This may be a slow operation if this was created using a TimePosition.
*/
BeatPosition toBeats (EditTime, const TempoSequence&);


//==============================================================================
//==============================================================================
/**
    Represents a time range in an Edit stored as either time or beats.
    This is basically a variant to simplify APIs that can accept either time base.
*/
struct EditTimeRange
{
    /** Creates an EditTimeRange from a TimeRange. */
    EditTimeRange (TimeRange);

    /** Creates an EditTimeRange from a BeatRange. */
    EditTimeRange (BeatRange);

private:
    friend TimeRange toTime (EditTimeRange, const TempoSequence&);
    friend BeatRange toBeats (EditTimeRange, const TempoSequence&);

    std::variant<TimeRange, BeatRange> range;
};

/** Converts an EditTimeRange to a TimeRange.
    N.B. This may be a slow operation if this was created using a BeatRange.
*/
TimeRange toTime (EditTimeRange, const TempoSequence&);

/** Converts an EditTimeRange to a BeatRange.
    N.B. This may be a slow operation if this was created using a TimeRange.
*/
BeatRange toBeats (EditTimeRange, const TempoSequence&);


//==============================================================================
/** Represents the position of a clip on a timeline.
*/
struct ClipPosition
{
    TimeRange time;             /**< The TimeRange this ClipPosition occupies. */
    TimeDuration offset = {};   /**< The offset this ClipPosition has.
                                     Offset is a bit unintuitive as the position this
                                     relates to in the source material will depend on
                                     lots of factors including looping and any time-stretching.
                                     It can generally be thought of as an offset from
                                     either the start of the file or the loop start position,
                                     scaled by any time-stretching.
                                */

    /** Returns the start time. */
    TimePosition getStart() const             { return time.getStart(); }
    /** Returns the end time. */
    TimePosition getEnd() const               { return time.getEnd(); }
    /** Returns the length. */
    TimeDuration getLength() const            { return time.getLength(); }
    /** Returns the offset. */
    TimeDuration getOffset() const            { return offset; }
    /** Returns what would be the the start of the source material in the timeline. */
    TimePosition getStartOfSource() const     { return time.getStart() - offset; }

    /** Compares two ClipPositions. */
    bool operator== (const ClipPosition& other) const  { return offset == other.offset && time == other.time; }
    /** Compares two ClipPositions. */
    bool operator!= (const ClipPosition& other) const  { return ! operator== (other); }

    /** Returns a ClipPosition scaled around an anchor point. Useful for stretching a clip. */
    ClipPosition rescaled (TimePosition anchorTime, double factor) const;
};


//==============================================================================
//==============================================================================
}} // namespace tracktion { inline namespace engine

namespace juce
{
    template<>
    struct VariantConverter<tracktion::core::TimePosition>
    {
        static tracktion::core::TimePosition fromVar (const var& v)   { return tracktion::core::TimePosition::fromSeconds (static_cast<double> (v)); }
        static var toVar (tracktion::core::TimePosition v)            { return v.inSeconds(); }
    };

    template<>
    struct VariantConverter<tracktion::core::TimeDuration>
    {
        static tracktion::core::TimeDuration fromVar (const var& v)   { return tracktion::core::TimeDuration::fromSeconds (static_cast<double> (v)); }
        static var toVar (tracktion::core::TimeDuration v)            { return v.inSeconds(); }
    };

    template<>
    struct VariantConverter<tracktion::core::BeatPosition>
    {
        static tracktion::core::BeatPosition fromVar (const var& v)   { return tracktion::core::BeatPosition::fromBeats (static_cast<double> (v)); }
        static var toVar (tracktion::core::BeatPosition v)            { return v.inBeats(); }
    };

    template<>
    struct VariantConverter<tracktion::core::BeatDuration>
    {
        static tracktion::core::BeatDuration fromVar (const var& v)   { return tracktion::core::BeatDuration::fromBeats (static_cast<double> (v)); }
        static var toVar (tracktion::core::BeatDuration v)            { return v.inBeats(); }
    };
}

namespace tracktion { inline namespace engine {

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

inline EditTime::EditTime()
    : EditTime (TimePosition())
{
}

inline EditTime::EditTime (TimePosition tp)
    : position (tp)
{
}

inline EditTime::EditTime (BeatPosition bp)
    : position (bp)
{
}

//==============================================================================
inline TimePosition toTime (EditTime et, const TempoSequence& ts)
{
    // N.B. std::get unavailable prior to macOS 10.14
    if (const auto tp = std::get_if<TimePosition> (&et.position))
        return *tp;

    return tracktion::engine::toTime (*std::get_if<BeatPosition> (&et.position), ts);
}

inline BeatPosition toBeats (EditTime et, const TempoSequence& ts)
{
    if (const auto bp = std::get_if<BeatPosition> (&et.position))
        return *bp;

    return tracktion::engine::toBeats (*std::get_if<TimePosition> (&et.position), ts);
}

//==============================================================================
inline EditTimeRange::EditTimeRange (TimeRange r)
    : range (r)
{
}

inline EditTimeRange::EditTimeRange (BeatRange r)
    : range (r)
{
}

//==============================================================================
inline TimeRange toTime (EditTimeRange r, const TempoSequence& ts)
{
    // N.B. std::get unavailable prior to macOS 10.14
    if (const auto tr = std::get_if<TimeRange> (&r.range))
        return *tr;

    return tracktion::engine::toTime (*std::get_if<BeatRange> (&r.range), ts);
}

inline BeatRange toBeats (EditTimeRange r, const TempoSequence& ts)
{
    if (const auto br = std::get_if<BeatRange> (&r.range))
        return *br;

    return tracktion::engine::toBeats (*std::get_if<TimeRange> (&r.range), ts);
}

//==============================================================================
inline ClipPosition ClipPosition::rescaled (TimePosition anchorTime, double factor) const
{
    return { time.rescaled (anchorTime, factor), offset * factor };
}


}} // namespace tracktion { inline namespace engine
