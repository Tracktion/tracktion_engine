/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace graph
{

//==============================================================================
//==============================================================================
/** Converts an integer sample number to a time in seconds. */
template<typename IntType>
constexpr double sampleToTime (IntType samplePosition, double sampleRate)
{
    return samplePosition / sampleRate;
}

/** Converts a time in seconds to a sample number. */
constexpr int64_t timeToSample (double timeInSeconds, double sampleRate)
{
    return static_cast<int64_t> ((timeInSeconds * sampleRate) + 0.5);
}

/** Converts an integer sample range to a time range in seconds. */
template<typename IntType>
constexpr juce::Range<double> sampleToTime (juce::Range<IntType> sampleRange, double sampleRate)
{
    return { sampleToTime (sampleRange.getStart(), sampleRate),
             sampleToTime (sampleRange.getEnd(), sampleRate) };
}

/** Converts a time range in seconds to a range of sample numbers. */
constexpr juce::Range<int64_t> timeToSample (juce::Range<double> timeInSeconds, double sampleRate)
{
    return { timeToSample (timeInSeconds.getStart(), sampleRate),
             timeToSample (timeInSeconds.getEnd(), sampleRate) };
}

/** Converts a time range in seconds to a range of sample numbers. */
template<typename RangeType>
constexpr juce::Range<int64_t> timeToSample (RangeType timeInSeconds, double sampleRate)
{
    return { timeToSample (timeInSeconds.getStart(), sampleRate),
             timeToSample (timeInSeconds.getEnd(), sampleRate) };
}

}}
