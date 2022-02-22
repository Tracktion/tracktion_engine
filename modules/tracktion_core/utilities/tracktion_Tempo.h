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
    /** Represents a number of bars and then beats in that bar. */
    struct BarsAndBeats
    {
        int bars = 0;       /**< The number of whole bars. */
        BeatDuration beats; /**< The number of beats in the current bar. */
        int numerator = 0;  /**< The number of beats in the current bar. */

        /** Returns the number bars elapsed. */
        double getTotalBars() const;

        /** Returns the number of whole beats. */
        int getWholeBeats() const;

        /** Returns the number of fractional beats. */
        BeatDuration getFractionalBeats() const;
    };

    struct TimeSignature
    {
        int numerator = 0;
        int denominator = 0;
    };

    //==============================================================================
    /** Represents a change in tempo at a specific beat. */
    struct TempoChange
    {
        BeatPosition startBeat; /**< The position of the change. */
        double bpm;             /**< The tempo of the change in beats per minute. */
        float curve;            /**< The curve of the change. 0 is a straight line +- < 0.5 are bezier curves. */
    };

    //==============================================================================
    /** Represents a change in time signature at a specific beat. */
    struct TimeSigChange
    {
        BeatPosition startBeat;      /**< The position of the change. */
        int numerator;               /**< The numerator of the time signature. */
        int denominator;             /**< The denominator of the time signature. */
        bool triplets;               /**< Whether the time signature represents triplets. */
    };

    //==============================================================================
    /** Used to determine the length of a beat in beat <-> time conversions. */
    enum class LengthOfOneBeat
    {
        dependsOnTimeSignature, /**< Signifies that the length (in seconds) of one "beat" at
                                     any point in an edit is considered to be the length of one division in the current
                                     bar's time signature.
                                     So for example at 120BPM, in a bar of 4/4, one beat would be the length of a
                                     quarter-note (0.5s), but in a bar of 4/8, one beat would be the length of an
                                     eighth-note (0.25s) */
        isAlwaysACrotchet       /**< Signifies the length of one beat always depends only the current BPM at that
                                     point in the edit, so where the BPM = 120, one beat is always 0.5s, regardless of
                                     the time-sig. E.g. 7/4 is treated the same as 7/8/ */
    };

    //==============================================================================
    /**
        Represents a tempo map with at least one TempoChange and TimeSigChange.
        Once constructed, this can be used to convert between time and beats.
    */
    struct Sequence
    {
        struct Section;

        //==============================================================================
        /** Creates a Sequence for at least one TempoChange and at least one TimeSigChange. */
        Sequence (std::vector<TempoChange>, std::vector<TimeSigChange>,
                  LengthOfOneBeat);

        /** Copies another Sequence. */
        Sequence (const Sequence&);

        /** Move constructor. */
        Sequence (Sequence&&);

        /** Copies another Sequence. */
        Sequence& operator= (const Sequence&);

        /** Moves another Sequence. */
        Sequence& operator= (Sequence&&);

        //==============================================================================
        /** Converts a time to a number of beats. */
        BeatPosition toBeats (TimePosition) const;

        /** Converts a number of BarsAndBeats to a position. */
        BeatPosition toBeats (BarsAndBeats) const;

        /** Converts a number of beats a time. */
        TimePosition toTime (BeatPosition) const;

        /** Converts a number of BarsAndBeats to a position. */
        TimePosition toTime (BarsAndBeats) const;

        /** Converts a time to a number of BarsAndBeats. */
        BarsAndBeats toBarsAndBeats (TimePosition) const;

        //==============================================================================
        /** Returns the tempo at a position. */
        double getBpmAt (TimePosition) const;

        /** Returns the number of beats per second at a position. */
        double getBeatsPerSecondAt (TimePosition) const;

        /** Returns a unique hash of this sequence. */
        size_t hash() const;

        //==============================================================================
        //==============================================================================
        /** A Sequence::Position is an iterator through a Sequence.
            You can set the position in time or beats and then use it to convert between
            times and beats much faster than a plain Sequence.

            N.B. This is optomised for setting a time, adding time and then converting that
            to beats. The other set/add operations are provided for convenience but will
            be slower to execute.
        */
        struct Position
        {
            //==============================================================================
            /** Creates a Position from a sequence.
                N.B. the Position keeps a reference to the Sequence so this must live longer
                than the Position.
            */
            Position (const Sequence&);

            /** Creates a copy of another Position. */
            Position (const Position&);

            //==============================================================================
            /** Returns the current time of the Position. */
            TimePosition getTime() const                                { return time; }

            /** Returns the current beats of the Position. */
            BeatPosition getBeats() const;

            /** Returns the current bars and beats of the Position. @see BarsAndBeats */
            BarsAndBeats getBarsBeats() const;

            /** Returns the current tempo of the Position. */
            double getTempo() const;

            /** Returns the current TimeSignature of the Position. */
            TimeSignature getTimeSignature() const;

            //==============================================================================
            /** Sets the Position to a new time. */
            void set (TimePosition);

            /** Sets the Position to a new number of beats. */
            void set (BeatPosition);

            /** Sets the Position to a new number of bars and beats. */
            void set (BarsAndBeats);

            /** Increments the position by a time duration. */
            void add (TimeDuration);

            /** Increments the position by a number of beats. */
            void add (BeatDuration);

            /** Increments the position by a number of bars. */
            void addBars (int bars);

            //==============================================================================
            /** Sets the position to a PPQ. */
            void setPPQTime (double ppq);

            /** Returns the position as a PPQ time. */
            double getPPQTime() const noexcept;

            /** Returns the PPQ start time of the bar the position is in. */
            double getPPQTimeOfBarStart() const noexcept;

        private:
            const Sequence& sequence;
            const std::vector<Section>& sections;
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

inline double BarsAndBeats::getTotalBars() const                { return bars + (numerator * beats.inBeats()); }
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

    inline BarsAndBeats toBarsAndBeats (const std::vector<Sequence::Section>& sections, TimePosition time)
    {
        for (int i = (int) sections.size(); --i > 0;)
        {
            auto& it = sections[(size_t) i];

            if (it.startTime <= time || i == 0)
            {
                const auto beatsSinceFirstBar = (time - it.timeOfFirstBar).inSeconds() * it.beatsPerSecond;

                if (beatsSinceFirstBar < 0)
                    return { it.barNumberOfFirstBar + (int) std::floor (beatsSinceFirstBar / it.numerator),
                             BeatDuration::fromBeats (std::fmod (std::fmod (beatsSinceFirstBar, it.numerator) + it.numerator, it.numerator)),
                             it.numerator };

                return { it.barNumberOfFirstBar + (int) std::floor (beatsSinceFirstBar / it.numerator),
                         BeatDuration::fromBeats (std::fmod (beatsSinceFirstBar, it.numerator)),
                         it.numerator };
            }
        }

        return { 0, {} };
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
inline Sequence::Sequence (std::vector<TempoChange> tempos, std::vector<TimeSigChange> timeSigs,
                           LengthOfOneBeat lengthOfOneBeat)
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

    const bool useDenominator = lengthOfOneBeat == LengthOfOneBeat::dependsOnTimeSignature;

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

inline Sequence& Sequence::operator= (const Sequence& o)
{
    sections = o.sections;
    hashCode = o.hashCode;
    return *this;
}

inline Sequence& Sequence::operator= (Sequence&& o)
{
    sections = std::move (o.sections);
    hashCode = o.hashCode;
    return *this;
}

//==============================================================================
inline BeatPosition Sequence::toBeats (TimePosition time) const
{
    return details::toBeats (sections, time);
}

inline BeatPosition Sequence::toBeats (BarsAndBeats barsAndBeats) const
{
    return toBeats (details::toTime (sections, barsAndBeats));
}

inline TimePosition Sequence::toTime (BeatPosition beats) const
{
    return details::toTime (sections, beats);
}

inline TimePosition Sequence::toTime (BarsAndBeats barsAndBeats) const
{
    return details::toTime (sections, barsAndBeats);
}

inline BarsAndBeats Sequence::toBarsAndBeats (TimePosition t) const
{
    return details::toBarsAndBeats (sections, t);
}

//==============================================================================
inline double Sequence::getBpmAt (TimePosition t) const
{
    for (int i = (int) sections.size(); --i >= 0;)
    {
        auto& it = sections[(size_t) i];

        if (it.startTime <= t || i == 0)
            return it.bpm;
    }

    assert(false);
    return 120.0;
}

inline double Sequence::getBeatsPerSecondAt (TimePosition t) const
{
    for (int i = (int) sections.size(); --i >= 0;)
    {
        auto& it = sections[(size_t) i];

        if (it.startTime <= t || i == 0)
            return it.beatsPerSecond;
    }

    assert(false);
    return 2.0;
}

inline size_t Sequence::hash() const
{
    return hashCode;
}


//==============================================================================
//==============================================================================
inline Sequence::Position::Position (const Sequence& sequenceToUse)
    : sequence (sequenceToUse), sections (sequence.sections)
{
    assert (sections.size() > 0);
}

inline Sequence::Position::Position (const Position& o)
    : sequence (o.sequence), sections (o.sections)
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
                 BeatDuration::fromBeats (std::fmod (std::fmod (beatsSinceFirstBar, it.numerator) + it.numerator, it.numerator)),
                 it.numerator };

    return { it.barNumberOfFirstBar + (int) std::floor (beatsSinceFirstBar / it.numerator),
             BeatDuration::fromBeats (std::fmod (beatsSinceFirstBar, it.numerator)),
             it.numerator };
}

inline double Sequence::Position::getTempo() const
{
    return sections[index].bpm;
}

inline TimeSignature Sequence::Position::getTimeSignature() const
{
    auto& it = sections[index];
    return { it.numerator, it.denominator };
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
