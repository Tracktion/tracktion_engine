/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace ModifierCommon
{
    constexpr double getBarFraction (RateType rt) noexcept
    {
        const double dot = 1.5;
        const double triplet = 2.0 / 3.0;

        switch (rt)
        {
            case fourBars:      return 4.0;
            case twoBars:       return 2.0;
            case bar:           return 1.0;
            case halfT:         return 1.0 / 2.0 * triplet;
            case half:          return 1.0 / 2.0;
            case halfD:         return 1.0 / 2.0 * dot;
            case quarterT:      return 1.0 / 4.0 * triplet;
            case quarter:       return 1.0 / 4.0;
            case quarterD:      return 1.0 / 4.0 * dot;
            case eighthT:       return 1.0 / 8.0 * triplet;
            case eighth:        return 1.0 / 8.0;
            case eighthD:       return 1.0 / 8.0 * dot;
            case sixteenthT:    return 1.0 / 16.0 * triplet;
            case sixteenth:     return 1.0 / 16.0;
            case sixteenthD:    return 1.0 / 16.0 * dot;
            case thirtySecondT: return 1.0 / 32.0 * triplet;
            case thirtySecond:  return 1.0 / 32.0;
            case thirtySecondD: return 1.0 / 32.0 * dot;
            case sixtyFourthT:  return 1.0 / 64.0 * triplet;
            case sixtyFourth:   return 1.0 / 64.0;
            case sixtyFourthD:  return 1.0 / 64.0 * dot;
            default:            return 1.0;
        };
    }
}
