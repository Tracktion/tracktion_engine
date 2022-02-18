/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include <cassert>
#include <algorithm>
#include <vector>

#include "tracktion_Time.h"
#include "tracktion_Bezier.h"

namespace tracktion { inline namespace core
{

//==============================================================================
//==============================================================================
namespace TimeSig
{
    struct Setting
    {
        BeatPosition startBeat;
        int numerator, denominator;
        bool triplets;
    };
}

//==============================================================================
//==============================================================================
namespace Tempo
{
    //==============================================================================
    struct Setting
    {
        BeatPosition startBeat;
        double bpm;
        float curve;
    };

    //==============================================================================
    struct Sequence
    {
        Sequence (std::vector<Setting>, std::vector<TimeSig::Setting>);

        BeatPosition toBeats (TimePosition) const;
        TimePosition toTime (BeatPosition) const;

        /** Returns a unique hash of this sequence. */
        size_t hash() const;

    private:
        struct Section;
        std::vector<Section> sections;
        size_t hash_code = 0;

        static double calcCurveBpm (double beat, const Setting, const Setting);
    };
}


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

namespace Tempo
{

//==============================================================================
struct Sequence::Section
{
    double bpm;
    TimePosition startTime;
    BeatPosition startBeat;
    double secondsPerBeat, beatsPerSecond, ppqAtStart;
    TimePosition timeOfFirstBar;
    BeatDuration beatsUntilFirstBar;
    int barNumberOfFirstBar, numerator, prevNumerator, denominator;
    bool triplets;
};

inline double Sequence::calcCurveBpm (double beat, const Setting t1, const Setting t2)
{
    const auto b1 = t1.startBeat.inBeats();
    const auto b2 = t2.startBeat.inBeats();
    const auto bpm1 = t1.bpm;
    const auto bpm2 = t2.bpm;
    const auto c = t1.curve;

    const auto [x, y] = getBezierPoint (b1, bpm1, b2, bpm2, c);

    if (c >= -0.5 && c <= 0.5)
        return getBezierYFromX (beat,
                                b1, bpm1, x, y, b2, bpm2);

    double x1end = 0;
    double x2end = 0;
    double y1end = 0;
    double y2end = 0;

    getBezierEnds (b1, bpm1, b2, bpm2, c,
                   x1end, y1end, x2end, y2end);

    if (beat >= b1 && beat <= x1end)
        return y1end;

    if (beat >= x2end && beat <= b2)
        return y2end;

    return getBezierYFromX (beat, x1end, y1end, x, y, x2end, y2end);
}

//==============================================================================
//==============================================================================
inline Sequence::Sequence (std::vector<Setting> tempos, std::vector<TimeSig::Setting> timeSigs)
{
    assert (tempos.size() > 0 && timeSigs.size() > 0);
    assert (tempos[0].startBeat == BeatPosition());
    assert (timeSigs[0].startBeat == BeatPosition());

    // Find the beats with changes
    std::vector<BeatPosition> beatsWithChanges;
    beatsWithChanges.reserve (tempos.size() + timeSigs.size());

    for (const auto& tempo : tempos)
    {
        beatsWithChanges.push_back (tempo.startBeat);

        hash_combine (hash_code, tempo.startBeat.inBeats());
        hash_combine (hash_code, tempo.bpm);
        hash_combine (hash_code, tempo.curve);
    }

    for (const auto& timeSig : timeSigs)
    {
        beatsWithChanges.push_back (timeSig.startBeat);

        hash_combine (hash_code, timeSig.startBeat.inBeats());
        hash_combine (hash_code, timeSig.numerator);
        hash_combine (hash_code, timeSig.denominator);
        hash_combine (hash_code, timeSig.triplets);
    }

    std::sort (beatsWithChanges.begin(), beatsWithChanges.end());
    beatsWithChanges.erase (std::unique (beatsWithChanges.begin(), beatsWithChanges.end()),
                            beatsWithChanges.end());

    // Build the sections
    TimePosition time;
    BeatPosition beatNum;
    double ppq          = 0.0;
    size_t timeSigIdx   = 0;
    size_t tempoIdx     = 0;

    auto currTempo   = tempos[tempoIdx++];
    auto currTimeSig = timeSigs[timeSigIdx++];

    const bool useDenominator = true; //edit.engine.getEngineBehaviour().lengthOfOneBeatDependsOnTimeSignature();

    for (size_t i = 0; i < beatsWithChanges.size(); ++i)
    {
        const auto currentBeat = beatsWithChanges[i];
        assert (std::abs ((currentBeat - beatNum).inBeats()) < 0.001);

        while (tempoIdx < tempos.size() && tempos[tempoIdx].startBeat == currentBeat)
            currTempo = tempos[tempoIdx++];

        if (timeSigIdx < timeSigs.size() && timeSigs[timeSigIdx].startBeat == currentBeat)
            currTimeSig = timeSigs[timeSigIdx++];

        const bool nextTempoValid = tempoIdx < tempos.size();
        double bpm = nextTempoValid ? calcCurveBpm (currTempo.startBeat.inBeats(), currTempo, tempos[tempoIdx])
                               : currTempo.bpm;

        int numSubdivisions = 1;

        if (nextTempoValid && (currTempo.curve != -1.0f && currTempo.curve != 1.0f))
            numSubdivisions = static_cast<int> (std::clamp (4.0 * (tempos[tempoIdx].startBeat - currentBeat).inBeats(), 1.0, 100.0));

        const auto numBeats = (i < beatsWithChanges.size() - 1)
                                    ? ((beatsWithChanges[i + 1] - currentBeat).inBeats() / (double) numSubdivisions)
                                    : 1.0e6;

        for (int k = 0; k < numSubdivisions; ++k)
        {
            Section it;

            it.bpm              = bpm;
            it.numerator        = currTimeSig.numerator;
            it.prevNumerator    = it.numerator;
            it.denominator      = currTimeSig.denominator;
            it.triplets         = currTimeSig.triplets;
            it.startTime        = time;
            it.startBeat        = beatNum;

            it.secondsPerBeat   = useDenominator ? (240.0 / (bpm * it.denominator))
                                                 : (60.0 / bpm);
            it.beatsPerSecond   = 1.0 / it.secondsPerBeat;

            it.ppqAtStart = ppq;
            ppq += 4 * numBeats / it.denominator;

            if (sections.empty())
            {
                it.barNumberOfFirstBar = 0;
                it.beatsUntilFirstBar = {};
                it.timeOfFirstBar = {};
            }
            else
            {
                const auto& prevSection = sections[sections.size() - 1];

                const auto beatsSincePreviousBarUntilStart = (time - prevSection.timeOfFirstBar).inSeconds() * prevSection.beatsPerSecond;
                const auto barsSincePrevBar = (int) std::ceil (beatsSincePreviousBarUntilStart / prevSection.numerator - 1.0e-5);

                it.barNumberOfFirstBar = prevSection.barNumberOfFirstBar + barsSincePrevBar;

                const auto beatNumInEditOfNextBar = BeatPosition::fromBeats ((int) std::lround ((prevSection.startBeat + prevSection.beatsUntilFirstBar).inBeats())
                                                                             + (barsSincePrevBar * prevSection.numerator));

                it.beatsUntilFirstBar = beatNumInEditOfNextBar - it.startBeat;
                it.timeOfFirstBar = time + TimeDuration::fromSeconds (it.beatsUntilFirstBar.inBeats() * it.secondsPerBeat);

                for (int j = (int) sections.size(); --j >= 0;)
                {
                    auto& tempo = sections[(size_t) j];

                    if (tempo.barNumberOfFirstBar < it.barNumberOfFirstBar)
                    {
                        it.prevNumerator = tempo.numerator;
                        break;
                    }
                }
            }

            sections.push_back (it);

            time = time + TimeDuration::fromSeconds (numBeats * it.secondsPerBeat);
            beatNum = beatNum + BeatDuration::fromBeats (numBeats);

            bpm = nextTempoValid ? calcCurveBpm (beatNum.inBeats(), currTempo, tempos[tempoIdx])
                                 : currTempo.bpm;
        }
    }
}

inline size_t Sequence::hash() const
{
    return hash_code;
}

inline BeatPosition Sequence::toBeats (TimePosition time) const
{
    for (int i = (int) sections.size(); --i > 0;)
    {
        auto& it = sections[(size_t) i];

        if (it.startTime <= time)
            return it.startBeat + BeatDuration::fromBeats ((time - it.startTime).inSeconds() * it.beatsPerSecond);
    }

    auto& it = sections[0];
    return it.startBeat + BeatDuration::fromBeats (((time - it.startTime).inSeconds() * it.beatsPerSecond));
}

inline TimePosition Sequence::toTime (BeatPosition beats) const
{
    for (int i = (int) sections.size(); --i > 0;)
    {
        auto& it = sections[(size_t) i];

        if (toPosition (beats - it.startBeat) >= BeatPosition())
            return it.startTime + TimeDuration::fromSeconds (it.secondsPerBeat * (beats - it.startBeat).inBeats());
    }

    auto& it = sections[0];
    return it.startTime + TimeDuration::fromSeconds (it.secondsPerBeat * (beats - it.startBeat).inBeats());
}

} // namespace Tempo

}} // namespace tracktion
