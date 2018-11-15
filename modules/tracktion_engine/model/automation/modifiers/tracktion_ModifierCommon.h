/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

namespace ModifierCommon
{
    enum RateType
    {
        hertz,
        fourBars,
        twoBars,
        bar,
        halfT,
        half,
        halfD,
        quarterT,
        quarter,
        quarterD,
        eighthT,
        eighth,
        eighthD,
        sixteenthT,
        sixteenth,
        sixteenthD,
        thirtySecondT,
        thirtySecond,
        thirtySecondD,
        sixtyFourthT,
        sixtyFourth,
        sixtyFourthD
    };

    inline juce::StringArray getRateTypeChoices()
    {
        return { TRANS("Hertz"),
                 TRANS("4 Bars"),
                 TRANS("2 Bars"),
                 TRANS("1 Bar"),
                 TRANS("1/2 T"),
                 TRANS("1/2"),
                 TRANS("1/2 D"),
                 TRANS("1/4 T"),
                 TRANS("1/4"),
                 TRANS("1/4 D"),
                 TRANS("1/8 T"),
                 TRANS("1/8"),
                 TRANS("1/8 D"),
                 TRANS("1/16 T"),
                 TRANS("1/16"),
                 TRANS("1/16 D"),
                 TRANS("1/32 T"),
                 TRANS("1/32"),
                 TRANS("1/32 D"),
                 TRANS("1/64 T"),
                 TRANS("1/64"),
                 TRANS("1/64 D") };
    }

    /** Returns the fraction of a bar to be used for a given rate type. */
    static constexpr double getBarFraction (RateType) noexcept;

    enum SyncType
    {
        free,
        transport,
        note
    };

    inline juce::StringArray getSyncTypeChoices()
    {
        return { TRANS("Free"), TRANS("Transport"), TRANS("Note") };
    }
}

} // namespace tracktion_engine
