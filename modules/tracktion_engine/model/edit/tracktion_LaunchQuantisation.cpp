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

juce::StringArray getLaunchQTypeChoices()
{
    return { NEEDS_TRANS("None"),
             NEEDS_TRANS("8 Bars"),
             NEEDS_TRANS("4 Bars"),
             NEEDS_TRANS("2 Bars"),
             NEEDS_TRANS("1 Bar"),
             NEEDS_TRANS("1/2 T"),
             NEEDS_TRANS("1/2"),
             NEEDS_TRANS("1/2 D"),
             NEEDS_TRANS("1/4 T"),
             NEEDS_TRANS("1/4"),
             NEEDS_TRANS("1/4 D"),
             NEEDS_TRANS("1/8 T"),
             NEEDS_TRANS("1/8"),
             NEEDS_TRANS("1/8 D"),
             NEEDS_TRANS("1/16 T"),
             NEEDS_TRANS("1/16"),
             NEEDS_TRANS("1/16 D"),
             NEEDS_TRANS("1/32 T"),
             NEEDS_TRANS("1/32"),
             NEEDS_TRANS("1/32 D"),
             NEEDS_TRANS("1/64 T"),
             NEEDS_TRANS("1/64"),
             NEEDS_TRANS("1/64 D") };
}

std::optional<LaunchQType> launchQTypeFromName (const juce::String& name)
{
    auto index = getLaunchQTypeChoices().indexOf (name);

    if (juce::isPositiveAndNotGreaterThan (index, static_cast<int> (LaunchQType::sixtyFourthD)))
        return static_cast<LaunchQType> (index);

    return {};
}

juce::String getName (LaunchQType t)
{
    assert (t >= LaunchQType::none || t <= LaunchQType::sixtyFourthD);
    return getLaunchQTypeChoices()[static_cast<int> (t)];
}

double toBarFraction (LaunchQType q) noexcept
{
    assert (q >= LaunchQType::none || q <= LaunchQType::sixtyFourthD);

    constexpr auto dot = 1.5;
    constexpr auto triplet = 2.0 / 3.0;

    switch (q)
    {
        case LaunchQType::eightBars:     return 8.0;
        case LaunchQType::fourBars:      return 4.0;
        case LaunchQType::twoBars:       return 2.0;
        case LaunchQType::bar:           return 1.0;
        case LaunchQType::halfT:         return 1.0 / 2.0 * triplet;
        case LaunchQType::half:          return 1.0 / 2.0;
        case LaunchQType::halfD:         return 1.0 / 2.0 * dot;
        case LaunchQType::quarterT:      return 1.0 / 4.0 * triplet;
        case LaunchQType::quarter:       return 1.0 / 4.0;
        case LaunchQType::quarterD:      return 1.0 / 4.0 * dot;
        case LaunchQType::eighthT:       return 1.0 / 8.0 * triplet;
        case LaunchQType::eighth:        return 1.0 / 8.0;
        case LaunchQType::eighthD:       return 1.0 / 8.0 * dot;
        case LaunchQType::sixteenthT:    return 1.0 / 16.0 * triplet;
        case LaunchQType::sixteenth:     return 1.0 / 16.0;
        case LaunchQType::sixteenthD:    return 1.0 / 16.0 * dot;
        case LaunchQType::thirtySecondT: return 1.0 / 32.0 * triplet;
        case LaunchQType::thirtySecond:  return 1.0 / 32.0;
        case LaunchQType::thirtySecondD: return 1.0 / 32.0 * dot;
        case LaunchQType::sixtyFourthT:  return 1.0 / 64.0 * triplet;
        case LaunchQType::sixtyFourth:   return 1.0 / 64.0;
        case LaunchQType::sixtyFourthD:  return 1.0 / 64.0 * dot;
        case LaunchQType::none:          return 0.0;
    }

    assert (false);
    return 1.0;
}

LaunchQType fromBarFraction (double f) noexcept
{
    auto ret = LaunchQType::bar;

    magic_enum::enum_for_each<LaunchQType> ([f, &ret] (auto t)
    {
        if (juce::approximatelyEqual (f, toBarFraction (t)))
        {
            ret = t;
            return;
        }
    });

    return ret;
}

BeatPosition getNext (LaunchQType q, const tempo::Sequence& ts, BeatPosition pos) noexcept
{
    constexpr auto adjustment = 1.0 - 1.0e-10; // Round up adjustment

    if (q == LaunchQType::none)
        return pos;

    const auto qFraction = toBarFraction (q);
    auto barsBeats = ts.toBarsAndBeats (ts.toTime (pos));

    if (q <= LaunchQType::bar)
    {
        auto barsPlusBeats = barsBeats.bars + (barsBeats.beats.inBeats() / barsBeats.numerator);

        barsBeats.bars = static_cast<int> (qFraction * (int) std::floor (barsPlusBeats / qFraction + adjustment));
        barsBeats.beats = {};
    }

    // Fractional bars
    barsBeats.beats = BeatDuration::fromBeats (qFraction * std::floor ((barsBeats.beats.inBeats() / qFraction) + adjustment));

    return ts.toBeats (barsBeats);
}

BeatPosition getNext (LaunchQType q, const TempoSequence& ts, BeatPosition pos) noexcept
{
    return getNext (q, ts.getInternalSequence(), pos);
}


//==============================================================================
//==============================================================================
LaunchQuantisation::LaunchQuantisation (juce::ValueTree& v, Edit& e)
    : edit (e)
{
    type.referTo (v, IDs::launchQuantisation, &e.getUndoManager(), LaunchQType::bar);
}

BeatPosition LaunchQuantisation::getNext (BeatPosition p) const noexcept
{
    return engine::getNext (type, edit.tempoSequence, p);
}

}} // namespace tracktion { inline namespace engine
