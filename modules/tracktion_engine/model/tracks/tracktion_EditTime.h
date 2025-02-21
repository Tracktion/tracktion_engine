/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
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
    Alias for a duration.
    Note this isn't a proper type like EditPosition as a duration by itself
    doesn't make sense for beats, you can't convert to/from Time without a
    position as well. So most of the time you'd specify this as an EditRange.
*/
using EditDuration = std::variant<TimeDuration, BeatDuration>;

/** Returns the raw value of beats or seconds, only really useful for
    comparing two EditDurations with the same time base.
*/
double toUnderlying (EditDuration);

//==============================================================================
//==============================================================================
/**
    Represents a time point in an Edit stored as either time or beats.
    This is basically a variant to simplify APIs that can accept either time base.
*/
struct EditPosition
{
    /** Creates an empty EditTime, starting at 0. */
    EditPosition();

    /** Creates an EditTime from a TimePosition. */
    EditPosition (TimePosition);

    /** Creates an EditTime from a BeatPosition. */
    EditPosition (BeatPosition);

    /** Returns true if the time is stored as beats, false if stored as a TimePosition. */
    bool isBeats() const;

private:
    friend TimePosition toTime (EditPosition, const TempoSequence&);
    friend BeatPosition toBeats (EditPosition, const TempoSequence&);
    friend double toUnderlying (EditPosition);

    std::variant<TimePosition, BeatPosition> position;
};

/** Alias provided for compatibility, may be removed in future. */
using EditTime = EditPosition;

inline EditPosition plus (const EditPosition& lhs, const EditDuration& rhs, const TempoSequence& ts)
{
    // Do this in terms of the duration's time base
    if (auto beatDur = std::get_if<BeatDuration> (&rhs))
        return toBeats (lhs, ts) + *beatDur;

    auto timeDur = std::get<TimeDuration> (rhs);
    return toTime (lhs, ts) + timeDur;
}

inline EditPosition minus (const EditPosition& lhs, const EditDuration& rhs, const TempoSequence& ts)
{
    // Do this in terms of the duration's time base
    if (auto beatDur = std::get_if<BeatDuration> (&rhs))
        return toBeats (lhs, ts) - *beatDur;

    auto timeDur = std::get<TimeDuration> (rhs);
    return toTime (lhs, ts) - timeDur;
}

inline bool equals (const EditPosition& lhs, const EditPosition& rhs, const TempoSequence& ts)
{
    // Convert to beats if the lhs is beats
    if (lhs.isBeats())
        return toBeats (lhs, ts) == toBeats (rhs, ts);

    // Otherwise convert to time
    return toTime (lhs, ts) == toTime (rhs, ts);
}

inline EditDuration minus (const EditPosition& lhs, const EditPosition& rhs, const TempoSequence& ts)
{
    // Convert to beats if the lhs is beats
    if (lhs.isBeats())
        return toBeats (lhs, ts) - toBeats (rhs, ts);

    // Otherwise convert to time
    return toTime (lhs, ts) - toTime (rhs, ts);
}

inline bool less (const EditPosition& lhs, const EditPosition& rhs, const TempoSequence& ts)
{
    if (rhs.isBeats() == lhs.isBeats())
        return toUnderlying (lhs) < toUnderlying (rhs);

    return toTime (lhs, ts) < toTime (rhs, ts);
}

inline bool greater (const EditPosition& lhs, const EditPosition& rhs, const TempoSequence& ts)
{
    if (rhs.isBeats() == lhs.isBeats())
        return toUnderlying (lhs) > toUnderlying (rhs);

    return toTime (lhs, ts) > toTime (rhs, ts);
}

inline bool lessThanOrEqualTo (const EditPosition& lhs, const EditPosition& rhs, const TempoSequence& ts)
{
    if (rhs.isBeats() == lhs.isBeats())
        return toUnderlying (lhs) <= toUnderlying (rhs);

    return toTime (lhs, ts) <= toTime (rhs, ts);
}

inline bool greaterThanOrEqualTo (const EditPosition& lhs, const EditPosition& rhs, const TempoSequence& ts)
{
    if (rhs.isBeats() == lhs.isBeats())
        return toUnderlying (lhs) >= toUnderlying (rhs);

    return toTime (lhs, ts) >= toTime (rhs, ts);
}

inline EditPosition min (const EditPosition& lhs, const EditPosition& rhs, const TempoSequence& ts)
{
    if (lhs.isBeats())
    {
        BeatPosition rhsBeats = rhs.isBeats() ? BeatPosition::fromBeats (toUnderlying (rhs))
                                              : toBeats (rhs, ts);
        return BeatPosition::fromBeats (std::min (toUnderlying (lhs), rhsBeats.inBeats()));
    }

    TimePosition rhsTime = rhs.isBeats() ? toTime (rhs, ts)
                                         : TimePosition::fromSeconds (toUnderlying (rhs));
    return TimePosition::fromSeconds (std::min (toUnderlying (lhs), rhsTime.inSeconds()));
}

inline EditPosition max (const EditPosition& lhs, const EditPosition& rhs, const TempoSequence& ts)
{
    if (lhs.isBeats())
    {
        BeatPosition rhsBeats = rhs.isBeats() ? BeatPosition::fromBeats (toUnderlying (rhs))
                                              : toBeats (rhs, ts);
        return BeatPosition::fromBeats (std::max (toUnderlying (lhs), rhsBeats.inBeats()));
    }

    TimePosition rhsTime = rhs.isBeats() ? toTime (rhs, ts)
                                         : TimePosition::fromSeconds (toUnderlying (rhs));
    return TimePosition::fromSeconds (std::max (toUnderlying (lhs), rhsTime.inSeconds()));
}

//==============================================================================
//==============================================================================
class EditPositionWithTempoSequence
{
public:
    EditPositionWithTempoSequence (EditPosition, const TempoSequence&);

    EditPosition getPosition() const                { return position; }
    const TempoSequence& getTempoSequence() const   { return sequence; }

private:
    EditPosition position;
    const TempoSequence& sequence;
};

inline EditPositionWithTempoSequence::EditPositionWithTempoSequence (EditPosition ep, const TempoSequence& ts)
    : position (std::move (ep)), sequence (ts)
{
}

inline bool operator== (const EditPositionWithTempoSequence& lhs, const EditPositionWithTempoSequence& rhs)
{
    assert (&lhs.getTempoSequence() == &rhs.getTempoSequence());
    return equals (lhs.getPosition(), rhs.getPosition(), lhs.getTempoSequence());
}

inline bool operator< (const EditPositionWithTempoSequence& lhs, const EditPositionWithTempoSequence& rhs)
{
    assert (&lhs.getTempoSequence() == &rhs.getTempoSequence());
    return less (lhs.getPosition(), rhs.getPosition(), lhs.getTempoSequence());
}

inline bool operator<= (const EditPositionWithTempoSequence& lhs, const EditPositionWithTempoSequence& rhs)
{
    assert (&lhs.getTempoSequence() == &rhs.getTempoSequence());
    return lessThanOrEqualTo (lhs.getPosition(), rhs.getPosition(), lhs.getTempoSequence());
}

inline bool operator> (const EditPositionWithTempoSequence& lhs, const EditPositionWithTempoSequence& rhs)
{
    assert (&lhs.getTempoSequence() == &rhs.getTempoSequence());
    return greater (lhs.getPosition(), rhs.getPosition(), lhs.getTempoSequence());
}

inline bool operator>= (const EditPositionWithTempoSequence& lhs, const EditPositionWithTempoSequence& rhs)
{
    assert (&lhs.getTempoSequence() == &rhs.getTempoSequence());
    return greaterThanOrEqualTo (lhs.getPosition(), rhs.getPosition(), lhs.getTempoSequence());
}

inline EditDuration operator- (const EditPositionWithTempoSequence& lhs, const EditPositionWithTempoSequence& rhs)
{
    assert (&lhs.getTempoSequence() == &rhs.getTempoSequence());
    return minus (lhs.getPosition(), rhs.getPosition(), lhs.getTempoSequence());
}

inline EditPosition operator+ (const EditPositionWithTempoSequence& lhs, const EditDuration& rhs)
{
    return plus (lhs.getPosition(), rhs, lhs.getTempoSequence());
}

inline EditPosition operator- (const EditPositionWithTempoSequence& lhs, const EditDuration& rhs)
{
    return minus (lhs.getPosition(), rhs, lhs.getTempoSequence());
}

//==============================================================================
/** Converts an EditTime to a TimePosition.
    N.B. This may be a slow operation if this was created using a BeatPosition.
*/
TimePosition toTime (EditPosition, const TempoSequence&);

/** Converts an EditTime to a BeatPosition.
    N.B. This may be a slow operation if this was created using a TimePosition.
*/
BeatPosition toBeats (EditPosition, const TempoSequence&);

/** Returns the raw number of seconds or beats.
    Mostly useful for testing.
*/
double toUnderlying (EditPosition);


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

    /** Creates an EditTimeRange from a TimeRange. */
    EditTimeRange (TimePosition, TimePosition);

    /** Creates an EditTimeRange from a TimeRange. */
    EditTimeRange (TimePosition, TimeDuration);

    /** Creates an EditTimeRange from a BeatRange. */
    EditTimeRange (BeatRange);

    /** Creates an EditTimeRange from a BeatRange. */
    EditTimeRange (BeatPosition, BeatPosition);

    /** Creates an EditTimeRange from a BeatRange. */
    EditTimeRange (BeatPosition, BeatDuration);

    /** Returns the start of the range. */
    EditPosition getStart() const;

    /** Returns the end of the range. */
    EditPosition getEnd() const;

    /** Returns the length of the range. */
    EditDuration getLength() const;

    /** Returns true if the time is stored as beats, false if stored as a TimePosition. */
    bool isBeats() const;

private:
    friend TimeRange toTime (EditTimeRange, const TempoSequence&);
    friend BeatRange toBeats (EditTimeRange, const TempoSequence&);

    std::variant<TimeRange, BeatRange> range;
};

/** Returns the raw number of seconds or beats.
    Mostly useful for testing.
*/
juce::Range<double> toUnderlying (EditTimeRange);

/** Returns true if the given position is contained within the given range. */
bool contains (EditTimeRange, EditPosition, const TempoSequence&);

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

/** Creates a ClipPosition from either a time or beat range. */
ClipPosition createClipPosition (const TempoSequence&, TimeRange, TimeDuration offset = {});

/** Creates a ClipPosition from either a time or beat range. */
ClipPosition createClipPosition (const TempoSequence&, BeatRange, BeatDuration offset = {});


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

inline double toUnderlying (EditDuration duration)
{
    // N.B. std::get unavailable prior to macOS 10.14
    if (const auto tp = std::get_if<TimeDuration> (&duration))
        return tp->inSeconds();

    assert (std::holds_alternative<BeatDuration> (duration));
    return std::get<BeatDuration> (duration).inBeats();
}


//==============================================================================
//==============================================================================
inline EditPosition::EditPosition()
    : EditPosition (TimePosition())
{
}

inline EditPosition::EditPosition (TimePosition tp)
    : position (tp)
{
}

inline EditPosition::EditPosition (BeatPosition bp)
    : position (bp)
{
}

inline bool EditPosition::isBeats() const
{
    return std::holds_alternative<BeatPosition> (position);
}

//==============================================================================
inline TimePosition toTime (EditPosition et, const TempoSequence& ts)
{
    // N.B. std::get unavailable prior to macOS 10.14
    if (const auto tp = std::get_if<TimePosition> (&et.position))
        return *tp;

    return tracktion::engine::toTime (*std::get_if<BeatPosition> (&et.position), ts);
}

inline BeatPosition toBeats (EditPosition et, const TempoSequence& ts)
{
    if (const auto bp = std::get_if<BeatPosition> (&et.position))
        return *bp;

    return tracktion::engine::toBeats (*std::get_if<TimePosition> (&et.position), ts);
}

inline double toUnderlying (EditPosition et)
{
    // N.B. std::get unavailable prior to macOS 10.14
    if (const auto tp = std::get_if<TimePosition> (&et.position))
        return tp->inSeconds();

    assert (std::holds_alternative<BeatPosition> (et.position));
    return std::get<BeatPosition> (et.position).inBeats();
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

inline EditTimeRange::EditTimeRange (TimePosition start, TimePosition end)
    : range (TimeRange (start, end))
{
}

inline EditTimeRange::EditTimeRange (TimePosition start, TimeDuration length)
    : range (TimeRange (start, length))
{
}

inline EditTimeRange::EditTimeRange (BeatPosition start, BeatPosition end)
    : range (BeatRange (start, end))
{
}

inline EditTimeRange::EditTimeRange (BeatPosition start, BeatDuration length)
    : range (BeatRange (start, length))
{
}

inline EditPosition EditTimeRange::getStart() const
{
    if (const auto tr = std::get_if<TimeRange> (&range))
        return tr->getStart();

    assert (isBeats());
    return std::get_if<BeatRange> (&range)->getStart();
}

inline EditPosition EditTimeRange::getEnd() const
{
    if (const auto tr = std::get_if<TimeRange> (&range))
        return tr->getEnd();

    assert (isBeats());
    return std::get_if<BeatRange> (&range)->getEnd();
}

inline EditDuration EditTimeRange::getLength() const
{
    if (const auto tr = std::get_if<TimeRange> (&range))
        return tr->getLength();

    assert (isBeats());
    return std::get_if<BeatRange> (&range)->getLength();
}

inline bool EditTimeRange::isBeats() const
{
    return std::holds_alternative<BeatRange> (range);
}

//==============================================================================
inline juce::Range<double> toUnderlying (EditTimeRange r)
{
    return { toUnderlying (r.getStart()),
             toUnderlying (r.getEnd()) };
}

inline bool contains (EditTimeRange r, EditPosition pos, const TempoSequence& ts)
{
    return greaterThanOrEqualTo (pos, r.getStart(), ts)
        && less (pos, r.getEnd(), ts);
}

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
