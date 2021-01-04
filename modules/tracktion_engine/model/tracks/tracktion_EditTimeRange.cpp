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

EditTimeRange::EditTimeRange (double s, double e) noexcept
    : start (s), end (e)
{
    jassert (s <= e);
}

EditTimeRange::EditTimeRange (juce::Range<double> timeRange) noexcept
    : EditTimeRange (timeRange.getStart(), timeRange.getEnd())
{
}

EditTimeRange EditTimeRange::between (double time1, double time2) noexcept
{
    return { std::min (time1, time2), std::max (time1, time2) };
}

EditTimeRange EditTimeRange::withStartAndLength (double startValue, double length) noexcept
{
    jassert (length >= 0.0);
    return Range<double>::withStartAndLength (startValue, length);
}

EditTimeRange EditTimeRange::emptyRange (double start) noexcept
{
    return { start, start };
}

EditTimeRange EditTimeRange::getUnionWith (const EditTimeRange& other) const noexcept
{
    return { std::min (start, other.start),
             std::max (end, other.end) };
}

EditTimeRange EditTimeRange::getIntersectionWith (const EditTimeRange& other) const noexcept
{
    const auto newStart = std::max (start, other.start);
    return { newStart, std::max (newStart, std::min (end, other.end)) };
}

EditTimeRange EditTimeRange::constrainRange (const EditTimeRange& rangeToConstrain) const noexcept
{
    const auto otherLen = rangeToConstrain.getLength();

    return getLength() <= otherLen
            ? *this
            : rangeToConstrain.movedToStartAt (jlimit (start, end - otherLen, rangeToConstrain.getStart()));
}

EditTimeRange EditTimeRange::rescaled (double anchorTime, double factor) const noexcept
{
    jassert (factor > 0);
    return { anchorTime + (start - anchorTime) * factor,
             anchorTime + (end   - anchorTime) * factor };
}

EditTimeRange EditTimeRange::expanded (double amount) const noexcept
{
    jassert (amount >= 0);
    return { start - amount, end + amount };
}

EditTimeRange EditTimeRange::reduced (double amount) const noexcept
{
    jassert (amount >= 0);
    amount = std::min (amount, getLength() / 2.0);
    return { start + amount, end - amount };
}

EditTimeRange EditTimeRange::movedToStartAt (double newStart) const noexcept
{
    return { newStart, end + (newStart - start) };
}

EditTimeRange EditTimeRange::movedToEndAt (double newEnd) const noexcept
{
    return { start + (newEnd - end), newEnd };
}

EditTimeRange EditTimeRange::withStart (double newStart) const noexcept
{
    jassert (newStart <= end);
    return { newStart, std::max (end, newStart) };
}

EditTimeRange EditTimeRange::withEnd (double newEnd) const noexcept
{
    jassert (newEnd >= start);
    return { std::min (start, newEnd), newEnd };
}

EditTimeRange EditTimeRange::withLength (double newLength) const noexcept
{
    jassert (newLength >= 0);
    return { start, start + std::max (0.0, newLength) };
}

ClipPosition ClipPosition::rescaled (double anchorTime, double factor) const noexcept
{
    return { time.rescaled (anchorTime, factor), offset * factor };
}

}
