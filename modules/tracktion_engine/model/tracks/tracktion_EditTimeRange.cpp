/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

EditTimeRange::EditTimeRange (double s, double e) : start (s), end (e)
{
    jassert (s <= e);
}

EditTimeRange::EditTimeRange (juce::Range<double> timeRange)
    : EditTimeRange (timeRange.getStart(), timeRange.getEnd())
{
}

EditTimeRange EditTimeRange::between (double time1, double time2)
{
    return time1 <= time2 ? EditTimeRange (time1, time2)
                          : EditTimeRange (time2, time1);
}

EditTimeRange EditTimeRange::getUnionWith (EditTimeRange other) const
{
    return { std::min (start, other.start),
             std::max (end, other.end) };
}

EditTimeRange EditTimeRange::getIntersectionWith (EditTimeRange other) const
{
    auto newStart = std::max (start, other.start);
    return { newStart, std::max (newStart, std::min (end, other.end)) };
}

EditTimeRange EditTimeRange::constrainRange (EditTimeRange rangeToConstrain) const
{
    auto otherLen = rangeToConstrain.getLength();

    return getLength() <= otherLen
            ? *this
            : rangeToConstrain.movedToStartAt (jlimit (start, end - otherLen, rangeToConstrain.getStart()));
}

EditTimeRange EditTimeRange::rescaled (double anchorTime, double factor) const
{
    jassert (factor > 0);
    return { anchorTime + (start - anchorTime) * factor,
             anchorTime + (end   - anchorTime) * factor };
}

EditTimeRange EditTimeRange::expanded (double amount) const
{
    jassert (amount >= 0);
    return { start - amount, end + amount };
}

EditTimeRange EditTimeRange::reduced (double amount) const
{
    jassert (amount >= 0);
    amount = std::min (amount, getLength() / 2.0);
    return { start + amount, end - amount };
}

EditTimeRange EditTimeRange::movedToStartAt (double newStart) const
{
    return { newStart, end + (newStart - start) };
}

EditTimeRange EditTimeRange::movedToEndAt (double newEnd) const
{
    return { start + (newEnd - end), newEnd };
}

EditTimeRange EditTimeRange::withStart (double newStart) const
{
    jassert (newStart <= end);
    return { newStart, std::max (end, newStart) };
}

EditTimeRange EditTimeRange::withEnd (double newEnd) const
{
    jassert (newEnd >= start);
    return { std::min (start, newEnd), newEnd };
}

EditTimeRange EditTimeRange::withLength (double newLength) const
{
    jassert (newLength >= 0);
    return { start, start + std::max (0.0, newLength) };
}

EditTimeRange EditTimeRange::operator+ (double amount) const
{
    return { start + amount, end + amount };
}

ClipPosition ClipPosition::rescaled (double anchorTime, double factor) const
{
    return { time.rescaled (anchorTime, factor), offset * factor };
}
