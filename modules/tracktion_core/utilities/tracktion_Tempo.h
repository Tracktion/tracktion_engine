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
namespace tempo
{
    //==============================================================================
    struct BarsAndBeats
    {
        int bars = 0;
        BeatDuration beats;

        int getWholeBeats() const;
        BeatDuration getFractionalBeats() const;
    };

    //==============================================================================
    struct TempoChange
    {
        BeatPosition startBeat;
        double bpm;
        float curve;
    };

    //==============================================================================
    struct TimeSigChange
    {
        BeatPosition startBeat;
        int numerator, denominator;
        bool triplets;
    };

    //==============================================================================
    struct Sequence
    {
        struct Section;

        /** Creates a Sequence for at least one TempoChange and at least one TimeSigChange. */
        Sequence (std::vector<TempoChange>, std::vector<TimeSigChange>);

        /** Copies another Sequence. */
        Sequence (const Sequence&);

        /** Move constructor. */
        Sequence (Sequence&&);

        /** Converts a time to a number of beats. */
        BeatPosition toBeats (TimePosition) const;

        /** Converts a number of beats a time. */
        TimePosition toTime (BeatPosition) const;

        /** Returns a unique hash of this sequence. */
        size_t hash() const;

        //==============================================================================
        //==============================================================================
        /** A Sequence::Position is an iterator through a Sequence.
            You can set the position in time or beats and then use it to convert between
            times and beats much faster than a plain Sequence.
        */
        struct Position
        {
            //==============================================================================
            /** Creates a Position from a sequence.
                N.B. the Position takes a copy of the relevent data so the source
                Sequence doesn't need to outlive the Position and there will be no
                data-races between access to them.
            */
            Position (const Sequence&);

            //==============================================================================
            TimePosition getTime() const                                { return time; }
            BeatPosition getBeats() const;
            BarsAndBeats getBarsBeats() const;

            //==============================================================================
            void set (TimePosition);
            void set (BeatPosition);
            void set (BarsAndBeats);

            void addBars (int bars);
            void add (BeatDuration);
            void add (TimeDuration);

            //==============================================================================
            void setPPQTime (double ppq);
            double getPPQTime() const noexcept;
            double getPPQTimeOfBarStart() const noexcept;

        private:
            std::vector<Section> sections;
            TimePosition time;
            size_t index = 0;
        };

    private:
        std::vector<Section> sections;
        size_t hashCode = 0;

        static double calcCurveBpm (double beat, const TempoChange, const TempoChange);
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

namespace tempo
{

inline int BarsAndBeats::getWholeBeats() const                  { return (int) std::floor (beats.inBeats()); }
inline BeatDuration BarsAndBeats::getFractionalBeats() const    { return BeatDuration::fromBeats (beats.inBeats() - std::floor (beats.inBeats())); }

//==============================================================================
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

namespace details
{
    inline BeatPosition toBeats (const std::vector<Sequence::Section>& sections, TimePosition time)
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

    inline TimePosition toTime (const std::vector<Sequence::Section>& sections, BeatPosition beats)
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

    inline TimePosition toTime (const std::vector<Sequence::Section>& sections, BarsAndBeats barsBeats)
    {
        for (int i = (int) sections.size(); --i >= 0;)
        {
            const auto& it = sections[(size_t) i];

            if (it.barNumberOfFirstBar == barsBeats.bars + 1
                  && barsBeats.beats.inBeats() >= it.prevNumerator - it.beatsUntilFirstBar.inBeats())
                return TimePosition::fromSeconds (it.timeOfFirstBar.inSeconds() - it.secondsPerBeat * (it.prevNumerator - barsBeats.beats.inBeats()));

            if (it.barNumberOfFirstBar <= barsBeats.bars || i == 0)
                return TimePosition::fromSeconds (it.timeOfFirstBar.inSeconds() + it.secondsPerBeat * (((barsBeats.bars - it.barNumberOfFirstBar) * it.numerator) + barsBeats.beats.inBeats()));
        }

        return {};
    }
}

//==============================================================================
//==============================================================================
inline double Sequence::calcCurveBpm (double beat, const TempoChange t1, const TempoChange t2)
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
inline Sequence::Sequence (std::vector<TempoChange> tempos, std::vector<TimeSigChange> timeSigs)
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

        hash_combine (hashCode, tempo.startBeat.inBeats());
        hash_combine (hashCode, tempo.bpm);
        hash_combine (hashCode, tempo.curve);
    }

    for (const auto& timeSig : timeSigs)
    {
        beatsWithChanges.push_back (timeSig.startBeat);

        hash_combine (hashCode, timeSig.startBeat.inBeats());
        hash_combine (hashCode, timeSig.numerator);
        hash_combine (hashCode, timeSig.denominator);
        hash_combine (hashCode, timeSig.triplets);
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

inline Sequence::Sequence (const Sequence& o)
    : sections (o.sections), hashCode (o.hashCode)
{
}

inline Sequence::Sequence (Sequence&& o)
    : sections (std::move (o.sections)), hashCode (o.hashCode)
{
}

inline size_t Sequence::hash() const
{
    return hashCode;
}

inline BeatPosition Sequence::toBeats (TimePosition time) const
{
    return details::toBeats (sections, time);
}

inline TimePosition Sequence::toTime (BeatPosition beats) const
{
    return details::toTime (sections, beats);
}


//==============================================================================
//==============================================================================
inline Sequence::Position::Position (const Sequence& sequence)
    : sections (sequence.sections)
{
    assert (sections.size() > 0);
}

inline BeatPosition Sequence::Position::getBeats() const
{
    const auto& it = sections[index];
    return BeatPosition::fromBeats (it.startBeat.inBeats() + (time - it.startTime).inSeconds() * it.beatsPerSecond);
}

inline BarsAndBeats Sequence::Position::getBarsBeats() const
{
    const auto& it = sections[index];
    const auto beatsSinceFirstBar = (time - it.timeOfFirstBar).inSeconds() * it.beatsPerSecond;

    if (beatsSinceFirstBar < 0)
        return { it.barNumberOfFirstBar + (int) std::floor (beatsSinceFirstBar / it.numerator),
                 BeatDuration::fromBeats (std::fmod (std::fmod (beatsSinceFirstBar, it.numerator) + it.numerator, it.numerator)) };

    return { it.barNumberOfFirstBar + (int) std::floor (beatsSinceFirstBar / it.numerator),
             BeatDuration::fromBeats (std::fmod (beatsSinceFirstBar, it.numerator)) };
}

//==============================================================================
inline void Sequence::Position::set (TimePosition t)
{
    const auto maxIndex = sections.size() - 1;

    if (index > maxIndex)
    {
        index = maxIndex;
        time = sections[index].startTime;
    }

    if (t >= time)
    {
        while (index < maxIndex && sections[index + 1].startTime <= t)
            ++index;
    }
    else
    {
        while (index > 0 && sections[index].startTime > t)
            --index;
    }

    time = t;
}

inline void Sequence::Position::set (BeatPosition t)
{
    set (details::toTime (sections, t));
}

inline void Sequence::Position::set (BarsAndBeats t)
{
    set (details::toTime (sections, t));
}

inline void Sequence::Position::addBars (int bars)
{
    if (bars > 0)
    {
        while (--bars >= 0)
            add (BeatDuration::fromBeats (sections[index].numerator));
    }
    else
    {
        while (++bars <= 0)
            add (BeatDuration::fromBeats (-sections[index].numerator));
    }
}

inline void Sequence::Position::add (BeatDuration beats)
{
    if (beats > BeatDuration())
    {
        for (;;)
        {
            auto maxIndex = sections.size() - 1;
            const auto& it = sections[index];
            auto beatTime = TimeDuration::fromSeconds (it.secondsPerBeat * beats.inBeats());

            if (index >= maxIndex
                 || sections[index + 1].startTime > (time + beatTime))
            {
                time = time + beatTime;
                break;
            }

            ++index;
            auto nextStart = sections[index].startTime;
            beats = beats - BeatDuration::fromBeats ((nextStart - time).inSeconds() * it.beatsPerSecond);
            time = nextStart;
        }
    }
    else
    {
        for (;;)
        {
            const auto& it = sections[index];
            auto beatTime = TimeDuration::fromSeconds (it.secondsPerBeat * beats.inBeats());

            if (index <= 0
                 || sections[index].startTime <= (time + beatTime))
            {
                time = time + beatTime;
                break;
            }

            beats = beats + BeatDuration::fromBeats ((time - it.startTime).inSeconds() * it.beatsPerSecond);
            time = sections[index].startTime;
            --index;
        }
    }
}

inline void Sequence::Position::add (TimeDuration d)
{
    set (time + d);
}

//==============================================================================
inline void Sequence::Position::setPPQTime (double ppq)
{
    for (int i = (int) sections.size(); --i >= 0;)
    {
        index = (size_t) i;

        if (sections[index].ppqAtStart <= ppq)
            break;
    }

    const auto& it = sections[index];
    auto beatsSinceStart = ((ppq - it.ppqAtStart) * it.denominator) / 4.0;
    time = TimePosition::fromSeconds ((beatsSinceStart * it.secondsPerBeat)) + toDuration (it.startTime);
}

inline double Sequence::Position::getPPQTime() const noexcept
{
    const auto& it = sections[index];
    auto beatsSinceStart = (time - it.startTime).inSeconds() * it.beatsPerSecond;

    return it.ppqAtStart + 4.0 * beatsSinceStart / it.denominator;
}

inline double Sequence::Position::getPPQTimeOfBarStart() const noexcept
{
    for (int i = int (index) + 1; --i >= 0;)
    {
        const auto& it = sections[(size_t) i];
        const double beatsSinceFirstBar = (time - it.timeOfFirstBar).inSeconds() * it.beatsPerSecond;

        if (beatsSinceFirstBar >= -it.beatsUntilFirstBar.inBeats() || i == 0)
        {
            const double beatNumberOfLastBarSinceFirstBar = it.numerator * std::floor (beatsSinceFirstBar / it.numerator);

            return it.ppqAtStart + 4.0 * (it.beatsUntilFirstBar.inBeats() + beatNumberOfLastBarSinceFirstBar) / it.denominator;
        }
    }

    return 0.0;
}

} // namespace Tempo

}} // namespace tracktion
