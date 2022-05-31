/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

TimecodeDuration::TimecodeDuration (std::optional<TimeDuration> s, std::optional<BeatDuration> b, int bpb)
    : seconds (s), beats (b), beatsPerBar (bpb)
{
}

bool TimecodeDuration::operator== (const TimecodeDuration& o) const
{
    if (seconds.has_value() != o.seconds.has_value()) return false;
    if (beats.has_value()   != o.beats.has_value())   return false;

    if (seconds.has_value())
        if (*seconds != *o.seconds)
            return false;

    if (beats.has_value())
        if (*beats != *o.beats)
            return false;

    return true;
}

bool TimecodeDuration::operator!= (const TimecodeDuration& o) const
{
    return ! (*this == o);
}

TimecodeDuration TimecodeDuration::fromSeconds (Edit& e, TimePosition start, TimePosition end)
{
    return TimecodeDuration (end - start,
                     e.tempoSequence.toBeats (end) - e.tempoSequence.toBeats (start),
                     e.tempoSequence.getTimeSigAt (start).numerator);
}

TimecodeDuration TimecodeDuration::fromSecondsOnly (TimeDuration duration)
{
    return TimecodeDuration (duration, {}, {});
}

TimecodeDuration TimecodeDuration::fromBeatsOnly (BeatDuration duration, int beatsPerBar)
{
    return TimecodeDuration ({}, duration, beatsPerBar);
}

struct TimeAndName
{
    double time;
    const char* name;
};

static const TimeAndName minSecDivisions[] =
{
    { 0.001,            NEEDS_TRANS("1 millisec") },
    { 0.01,             NEEDS_TRANS("10 millisecs") },
    { 0.1,              NEEDS_TRANS("100 millisecs") },
    { 0.5,              NEEDS_TRANS("1/2 second") },
    { 1.0,              NEEDS_TRANS("second") },
    { 2.0,              NEEDS_TRANS("2 seconds") },
    { 5.0,              NEEDS_TRANS("5 seconds") },
    { 10.0,             NEEDS_TRANS("10 seconds") },
    { 30.0,             NEEDS_TRANS("30 seconds") },
    { 60.0,             NEEDS_TRANS("minute") },
    { 5 * 60.0,         NEEDS_TRANS("5 minutes") },
    { 15 * 60.0,        NEEDS_TRANS("15 minutes") },
    { 30 * 60.0,        NEEDS_TRANS("30 minutes") },
    { 60 * 60.0,        NEEDS_TRANS("hour") },
    { 5 * 60 * 60.0,    NEEDS_TRANS("5 hours") }
};


static const TimeAndName subBeatFractions[] =
{
    { 1.0   / 960,      NEEDS_TRANS("1 tick") },
    { 2.0   / 960,      NEEDS_TRANS("2 ticks") },
    { 5.0   / 960,      NEEDS_TRANS("5 ticks") },
    { 15.0  / 960,      NEEDS_TRANS("1/64 beat") },
    { 30.0  / 960,      NEEDS_TRANS("1/32 beat") },
    { 60.0  / 960,      NEEDS_TRANS("1/16 beat") },
    { 120.0 / 960,      NEEDS_TRANS("1/8 beat") },
    { 240.0 / 960,      NEEDS_TRANS("1/4 beat") },
    { 480.0 / 960,      NEEDS_TRANS("1/2 beat") }
};

static const TimeAndName subBeatFractionsTriplets[] =
{
    { 1.0 / 960,        NEEDS_TRANS("1 tick") },
    { 2.0 / 960,        NEEDS_TRANS("2 ticks") },
    { 5.0 / 960,        NEEDS_TRANS("5 ticks") },
    { 1.0 / 48.0,       NEEDS_TRANS("1/48 beat") },
    { 1.0 / 24.0,       NEEDS_TRANS("1/24 beat") },
    { 1.0 / 12.0,       NEEDS_TRANS("1/12 beat") },
    { 1.0 / 9.0,        NEEDS_TRANS("1/9 beat") },
    { 1.0 / 6.0,        NEEDS_TRANS("1/6 beat") },
    { 1.0 / 3.0,        NEEDS_TRANS("1/3 beat") }
};

static const int subSecDivisionsForType[] = { 1000, Edit::ticksPerQuarterNote, 24, 25, 30 };

static const int barMultiples[] = { 1, 2, 4, 8, 16, 64, 128, 256, 1024, 4096, 16384, 65536 };

// maximum value we can add without shoving a time over into the next 'slot'
static const TimeDuration nudge = TimeDuration::fromSeconds (0.05 / 96000.0);


//==============================================================================
static TimeAndName getMinSecDivisions (int level) noexcept
{
    jassert (level >= 0);
    return minSecDivisions [std::min (13, level)];
}

juce::String TimecodeSnapType::getDescription (const TempoSetting& tempo, bool isTripletOverride) const
{
    if (type == TimecodeType::barsBeats)
    {
        if (level < 9)
        {
            bool triplets = isTripletOverride || tempo.getMatchingTimeSig().triplets;
            return TRANS (triplets ? subBeatFractionsTriplets[level].name
                                   : subBeatFractions[level].name);
        }

        if (level == 9)   return TRANS("Beat");
        if (level == 10)  return TRANS("Bar");

        return TRANS("33 bars").replace ("33", juce::String (barMultiples[std::min (level, 19) - 10]));
    }

    if (type == TimecodeType::millisecs)
        return TRANS(getMinSecDivisions (level).name);

    if (level == 0) return TRANS("1/100 frame");
    if (level == 1) return TRANS("Frame");

    return TRANS(getMinSecDivisions (level + 2).name);
}

TimeDuration TimecodeSnapType::getApproxIntervalTime (const TempoSetting& tempo) const
{
    return getApproxIntervalTime (tempo, false);
}

TimeDuration TimecodeSnapType::getApproxIntervalTime (const TempoSetting& tempo, bool isTripletsOverride) const
{
    if (type == TimecodeType::barsBeats)
    {
        const auto beatLen = tempo.getApproxBeatLength();

        if (level < 9)
        {
            if (isTripletsOverride || tempo.getMatchingTimeSig().triplets)
                return beatLen * subBeatFractionsTriplets [level].time;

            return beatLen * subBeatFractions [level].time;
        }

        if (level == 9)
            return beatLen;

        auto barLength = beatLen * tempo.getMatchingTimeSig().numerator.get();

        return barLength * barMultiples[level - 10];
    }

    return getIntervalNonBarsBeats();
}

TimeDuration TimecodeSnapType::getIntervalNonBarsBeats() const
{
    if (type == TimecodeType::millisecs)
        return TimeDuration::fromSeconds (getMinSecDivisions (level).time);

    jassert (type != TimecodeType::barsBeats);

    auto oneFrame = 1.0 / subSecDivisionsForType[static_cast<int> (type)];

    if (level == 0)
        return TimeDuration::fromSeconds (oneFrame * 0.01);

    if (level == 1)
        return TimeDuration::fromSeconds (oneFrame);

    return TimeDuration::fromSeconds (getMinSecDivisions (level + 2).time);
}

juce::String TimecodeSnapType::getTimecodeString (TimePosition time, const TempoSequence& sequence, bool useStartLabelIfZero) const
{
    if (type == TimecodeType::barsBeats)
    {
        if (time == TimePosition() && useStartLabelIfZero)
            return TRANS("Bar 1");

        auto barsBeats = sequence.toBarsAndBeats (time + nudge);
        auto bars = barsBeats.bars + 1;
        auto beats = barsBeats.getWholeBeats() + 1;

        if (level < 9)  return juce::String::formatted ("%d|%d|%03d", bars, beats, (int) (barsBeats.getFractionalBeats().inBeats() * Edit::ticksPerQuarterNote));
        if (level == 9) return juce::String::formatted ("%d|%d", bars, beats);

        return TRANS("Bar") + " " + juce::String (bars);
    }

    if (type == TimecodeType::millisecs)
    {
        if (time == TimePosition() && useStartLabelIfZero)
            return "0";

        if (level >= 9)
            return juce::RelativeTime (time.inSeconds()).getDescription();

        if (level >= 4)
            return TimecodeDisplayFormat::toFullTimecode (time, 0, false);

        return TimecodeDisplayFormat::toFullTimecode (time, 1000, false);
    }

    if (time == TimePosition() && useStartLabelIfZero)
        return "0";

    if (level >= 7)
        return juce::RelativeTime (time.inSeconds()).getDescription();

    if (level >= 2)
        return TimecodeDisplayFormat::toFullTimecode (time, 0, false);

    return TimecodeDisplayFormat::toFullTimecode (time, subSecDivisionsForType[static_cast<int> (type)], false);
}

int TimecodeSnapType::getOneBarLevel() const noexcept
{
    return type == TimecodeType::barsBeats ? 10 : (type == TimecodeType::millisecs ? 4 : 2);
}

TimePosition TimecodeSnapType::roundTimeDown (TimePosition t, const TempoSequence& sequence) const
{
    return roundTime (t, sequence, 0.0);
}

TimePosition TimecodeSnapType::roundTimeDown (TimePosition t, const TempoSequence& sequence, bool isTripletsOverride) const
{
    return roundTime (t, sequence, 0.0, isTripletsOverride);
}

TimePosition TimecodeSnapType::roundTimeNearest (TimePosition t, const TempoSequence& sequence) const
{
    return roundTime (t, sequence, 0.5 - 1.0e-10);
}

TimePosition TimecodeSnapType::roundTimeNearest (TimePosition t, const TempoSequence& sequence, bool isTripletsOverride) const
{
    return roundTime (t, sequence, 0.5 - 1.0e-10, isTripletsOverride);
}

TimePosition TimecodeSnapType::roundTimeUp (TimePosition t, const TempoSequence& sequence) const
{
    return roundTime (t, sequence, 1.0 - 1.0e-10);
}

TimePosition TimecodeSnapType::roundTimeUp (TimePosition t, const TempoSequence& sequence, bool isTripletsOverride) const
{
    return roundTime (t, sequence, 1.0 - 1.0e-10, isTripletsOverride);
}

TimePosition TimecodeSnapType::roundTime (TimePosition t, const TempoSequence& sequence, double adjustment) const
{
    return roundTime (t, sequence, adjustment, sequence.isTripletsAtTime (t));
}

TimePosition TimecodeSnapType::roundTime (TimePosition t, const TempoSequence& sequence, double adjustment, bool tripletsOverride) const
{
    if (type == TimecodeType::barsBeats)
    {
        auto barsBeats = sequence.toBarsAndBeats (t);
        auto& tempo = sequence.getTempoAt (t);

        if (level < 9)
        {
            auto q = tripletsOverride ? subBeatFractionsTriplets[level].time
                                      : subBeatFractions[level].time;

            barsBeats.beats = BeatDuration::fromBeats (q * std::floor (barsBeats.beats.inBeats() / q + adjustment));
        }
        else if (level == 9)
        {
            barsBeats.beats = BeatDuration::fromBeats (std::floor (barsBeats.beats.inBeats() + adjustment));
        }
        else
        {
            auto barsPlusBeats = barsBeats.bars + barsBeats.beats.inBeats() / tempo.getMatchingTimeSig().numerator;
            auto q = barMultiples[level - 10];

            barsBeats.bars = q * (int) std::floor (barsPlusBeats / q + adjustment);
            barsBeats.beats = {};
        }

        return sequence.toTime (barsBeats);
    }

    auto q = getIntervalNonBarsBeats();
    return toPosition (q * std::floor ((t / q) + adjustment));
}

TimecodeSnapType TimecodeSnapType::getSnapTypeForMaximumSnapLevelOf (TimePosition t, const TempoSequence& sequence) const
{
    return getSnapTypeForMaximumSnapLevelOf (t, sequence, sequence.isTripletsAtTime (t));
}

TimecodeSnapType TimecodeSnapType::getSnapTypeForMaximumSnapLevelOf (TimePosition t, const TempoSequence& sequence, bool isTripletsOverride) const
{
    const TimecodeDisplayFormat format (type);
    auto numTypes = format.getNumSnapTypes();
    int i;

    for (i = level; i < numTypes; ++i)
    {
        TimecodeSnapType snap (type, i);

        if (std::abs ((t - snap.roundTimeNearest (t, sequence, isTripletsOverride)).inSeconds()) > 1.0e-6)
        {
            --i;
            break;
        }
    }

    return format.getSnapType (i);
}

TimecodeSnapType TimecodeSnapType::get1BeatSnapType()
{
    return TimecodeSnapType (TimecodeType::barsBeats, 9);
}

//==============================================================================
bool TimecodeDisplayFormat::isBarsBeats() const       { return type == TimecodeType::barsBeats; }
bool TimecodeDisplayFormat::isMilliseconds() const    { return type == TimecodeType::millisecs; }
bool TimecodeDisplayFormat::isSMPTE() const           { return type == TimecodeType::fps24 || type == TimecodeType::fps25 || type == TimecodeType::fps30; }

int TimecodeDisplayFormat::getFPS() const
{
    const int defaultFPS = 24;

    switch (type)
    {
        case TimecodeType::millisecs:       return defaultFPS;
        case TimecodeType::barsBeats:       return defaultFPS;
        case TimecodeType::fps24:           return 24;
        case TimecodeType::fps25:           return 25;
        case TimecodeType::fps30:           return 30;
        default: jassertfalse;              return defaultFPS;
    }
}

juce::String TimecodeDisplayFormat::getRoundingDescription() const
{
    switch (type)
    {
        case TimecodeType::millisecs:       return TRANS("Snap to nearest round number");
        case TimecodeType::barsBeats:       return TRANS("Snap to nearest beat or subdivision");
        case TimecodeType::fps24:
        case TimecodeType::fps25:
        case TimecodeType::fps30:           return TRANS("Snap to nearest frame");
        default: jassertfalse;              break;
    }

    return {};
}

int TimecodeDisplayFormat::getSubSecondDivisions() const
{
    return subSecDivisionsForType[static_cast<int> (type)];
}

juce::String TimecodeDisplayFormat::getString (const TempoSequence& tempo, const TimePosition time, bool isRelative) const
{
    if (type == TimecodeType::barsBeats)
    {
        tempo::BarsAndBeats barsBeats;
        int bars, beats;
        BeatDuration fraction;

        if (! isRelative)
        {
            barsBeats = tempo.toBarsAndBeats (time + nudge);
            bars = barsBeats.bars + 1;
            beats = barsBeats.getWholeBeats() + 1;
            fraction = barsBeats.getFractionalBeats();
        }
        else if (time < 0s)
        {
            barsBeats = tempo.toBarsAndBeats (time - nudge);
            bars = -barsBeats.bars - 1;
            beats = (tempo.getTimeSig(0)->numerator - 1) - barsBeats.getWholeBeats();
            fraction = BeatDuration::fromBeats (1.0) - barsBeats.getFractionalBeats();
        }
        else
        {
            barsBeats = tempo.toBarsAndBeats (time + nudge);
            bars = barsBeats.bars + 1;
            beats = barsBeats.getWholeBeats() + 1;
            fraction = barsBeats.getFractionalBeats();
        }

        auto s = juce::String::formatted ("%d|%d|%03d", bars, beats, (int) (fraction.inBeats() * Edit::ticksPerQuarterNote));
        return time < 0s ? ("-" + s) : s;
    }

    return TimecodeDisplayFormat::toFullTimecode (time, getSubSecondDivisions());
}

int TimecodeDisplayFormat::getNumParts() const
{
    return type == TimecodeType::barsBeats ? 3 : 4;
}

juce::String TimecodeDisplayFormat::getSeparator (int part) const
{
    return (type == TimecodeType::barsBeats) ? ","
                                             : ((part == 0 && type == TimecodeType::millisecs) ? "." : ":");
}

int TimecodeDisplayFormat::getMaxCharsInPart (int part, bool canBeNegative) const
{
    if (canBeNegative)
    {
        const char m[5][4] = { { 3, 2, 2, 2 },
                               { 3, 2, 4, 4 },
                               { 2, 2, 2, 3 },
                               { 2, 2, 2, 3 },
                               { 2, 2, 2, 3 } };

        return m[static_cast<int> (type)][part];
    }

    const char m[5][4] = { { 3, 2, 2, 2 },
                           { 3, 2, 4, 4 },
                           { 2, 2, 2, 2 },
                           { 2, 2, 2, 2 },
                           { 2, 2, 2, 2 } };

    return m[static_cast<int> (type)][part];
}

int TimecodeDisplayFormat::getMaxValueOfPart (const TempoSequence& sequence, TimecodeDuration currentTime, int part, bool isRelative) const
{
    if (type == TimecodeType::barsBeats && part == 1)
    {
        if (currentTime.beatsPerBar != 0.0)
            return currentTime.beatsPerBar - (isRelative ? 1 : 0);

        return sequence.getTimeSigAt (toPosition (*currentTime.seconds)).numerator - (isRelative ? 1 : 0);
    }

    const short m[5][4] = { { 999, 59, 59,  48 },
                            { 959, 99, 999, 9999 },
                            { 23,  59, 59,  48 },
                            { 24,  59, 59,  48 },
                            { 29,  59, 59,  48 } };

    return m[static_cast<int> (type)][part];
}

int TimecodeDisplayFormat::getMinValueOfPart (int part, bool isRelative) const
{
    return (type == TimecodeType::barsBeats && part > 0 && ! isRelative) ? 1 : 0;
}

static juce::String twoCharString (int n)
{
    if (n < 10)
        return juce::String::charToString ('0') + (juce::juce_wchar) (n + '0');

    return juce::String (n);
}

void TimecodeDisplayFormat::getPartStrings (TimecodeDuration duration,
                                            const TempoSequence& tempo,
                                            bool isRelative,
                                            juce::String results[4]) const
{
    if (type == TimecodeType::barsBeats)
    {
        tempo::BarsAndBeats barsBeats;

        if (duration.beats.has_value())
        {
            auto t = (*duration.beats).inBeats() + nudge.inSeconds();

            barsBeats.bars  = int (t / duration.beatsPerBar);
            barsBeats.beats = BeatDuration::fromBeats (t - (barsBeats.bars * duration.beatsPerBar));
        }
        else if (duration.seconds.has_value())
        {
            auto time = *duration.seconds;

            if (time < 0s)
            {
                time = -time;
                results[2] = "-";
            }

            barsBeats = tempo.toBarsAndBeats (toPosition (time + nudge));
        }

        {
            auto val = (int) (barsBeats.getFractionalBeats().inBeats() * Edit::ticksPerQuarterNote);

            char text[4];
            text[0] = (char) ('0' + (val / 100) % 10);
            text[1] = (char) ('0' + (val / 10) % 10);
            text[2] = (char) ('0' + val % 10);
            text[3] = 0;
            results[0] = text;
        }

        for (int part = 1; part < 3; ++part)
        {
            int val = (part == 1) ? barsBeats.getWholeBeats() : barsBeats.bars;

            if (! isRelative)
                ++val;

            results[part] << val;
        }
    }
    else if (duration.seconds.has_value())
    {
        auto t = (tracktion::abs (*duration.seconds) + nudge).inSeconds();

        if (type == TimecodeType::millisecs)
        {
            auto val =  ((int) (t * 1000.0)) % 1000;

            char text[4];
            text[0] = (char) ('0' + (val / 100) % 10);
            text[1] = (char) ('0' + (val / 10) % 10);
            text[2] = (char) ('0' + val % 10);
            text[3] = 0;
            results[0] = text;
        }
        else if (type == TimecodeType::fps24)
        {
            results[0] = twoCharString (((int) (t * 24)) % 24);
        }
        else if (type == TimecodeType::fps25)
        {
            results[0] = twoCharString (((int) (t * 25)) % 25);
        }
        else
        {
            jassert (type == TimecodeType::fps30);
            results[0] = twoCharString (((int) (t * 30)) % 30);
        }

        if (*duration.seconds < 0s)
            results[3] = "-";

        auto hours = (int) (t * (1.0 / 3600.0));
        results[3] << twoCharString (hours);

        auto mins = (((int) t) / 60) % 60;
        results[2] = twoCharString (mins);

        auto secs = (((int) t) % 60);
        results[1] = twoCharString (secs);
    }
    else
    {
        jassertfalse;
    }
}

TimecodeDuration TimecodeDisplayFormat::getNewTimeWithPartValue (TimecodeDuration time, const TempoSequence& tempo,
                                                                 int part, int newValue, bool isRelative) const
{
    if (type == TimecodeType::barsBeats)
    {
        if (time.beats.has_value())
        {
            auto t = (*time.beats).inBeats();

            auto bars  = int (t / time.beatsPerBar);
            auto beats = t - (bars * time.beatsPerBar);
            auto ticks = std::fmod (beats, 1.0);
            beats = int (beats);

            if (part == 2)      bars  = newValue;
            else if (part == 1) beats = newValue;
            else if (part == 0) ticks = newValue / double (Edit::ticksPerQuarterNote);

            return TimecodeDuration::fromBeatsOnly (BeatDuration::fromBeats (bars * time.beatsPerBar + beats + ticks), time.beatsPerBar);
        }
        else
        {
            auto t = toPosition (tracktion::abs (*time.seconds));

            auto pos = createPosition (tempo);
            pos.set (t);

            auto barsBeats = tempo.toBarsAndBeats (t);

            if (part == 0)       pos.add (BeatDuration::fromBeats (newValue / (double) Edit::ticksPerQuarterNote) - barsBeats.getFractionalBeats());
            else if (part == 1)  pos.add (BeatDuration::fromBeats ((isRelative ? newValue : (newValue - 1)) - barsBeats.getWholeBeats()));
            else if (part == 2)  pos.addBars  ((isRelative ? newValue : (newValue - 1)) - barsBeats.bars);

            return TimecodeDuration::fromSecondsOnly (toDuration (*time.seconds < 0s ? -pos.getTime() : pos.getTime()));
        }
    }

    auto t = tracktion::abs (*time.seconds).inSeconds();
    auto intT = (int) t;
    auto hours = (int) (t / 3600.0);
    auto mins  = (intT / 60) % 60;
    auto secs  = (intT % 60);
    auto frac  = t - intT;

    if (part == 0)
    {
        auto subSecDivs = getSubSecondDivisions();
        frac = ((newValue + subSecDivs * 1000) % subSecDivs) / (double) subSecDivs;
    }
    else if (part == 1)
    {
        secs = (newValue + 3600) % 60;
    }
    else if (part == 2)
    {
        mins = (newValue + 3600) % 60;
    }
    else if (part == 3)
    {
        hours = std::max (0, newValue);
    }

    t = hours * 3600.0 + mins * 60.0 + secs + frac;

    return TimecodeDuration::fromSecondsOnly (TimeDuration::fromSeconds (*time.seconds < 0s ? -t : t));
}

//==============================================================================
juce::String TimecodeDisplayFormat::toFullTimecode (TimePosition seconds, int subSecondDivisions, bool showHours)
{
    juce::String result = (seconds < 0s) ? "-" : "";
    auto absSecs = tracktion::abs (seconds).inSeconds();

    if (showHours || (absSecs >= 60.0 * 60.0))
        result += juce::String::formatted ("%02d:%02d:%02d",
                                           (int) (absSecs / (60.0 * 60.0)),
                                           ((int) (absSecs / 60.0)) % 60,
                                           ((int) absSecs) % 60);
    else
        result += juce::String::formatted ("%d:%02d",
                                           ((int) (absSecs / 60.0)) % 60,
                                           ((int) absSecs) % 60);

    if (subSecondDivisions > 0)
        result += juce::String::formatted (":%0*d",
                                           juce::String (subSecondDivisions - 1).length(),
                                           juce::roundToInt (absSecs * subSecondDivisions) % subSecondDivisions);

    return result;
}

//==============================================================================
TimecodeSnapType TimecodeDisplayFormat::getBestSnapType (const TempoSetting& tempo,
                                                         TimeDuration onScreenTimePerPixel,
                                                         bool isTripletOverride) const
{
    if (type >= TimecodeType::fps24 && (1.0 / getSubSecondDivisions()) / onScreenTimePerPixel.inSeconds() > 2)
        return TimecodeSnapType (type, 1);

    auto numSnapTypes = getNumSnapTypes();

    for (int i = 0; i < numSnapTypes; ++i)
    {
        TimecodeSnapType snap (type, i);
        auto res = snap.getApproxIntervalTime (tempo, isTripletOverride);
        auto t = res / onScreenTimePerPixel.inSeconds();

        if (t > 12s)
            return snap;
    }

    return getSnapType (numSnapTypes);
}

int TimecodeDisplayFormat::getNumSnapTypes() const
{
    return type == TimecodeType::millisecs ? 13
                                           : (type == TimecodeType::barsBeats ? 19 : 15);
}

TimecodeSnapType TimecodeDisplayFormat::getSnapType (int index) const
{
    return TimecodeSnapType (type, juce::jlimit (0, getNumSnapTypes() - 1, index));
}

//==============================================================================
TimecodeDisplayIterator::TimecodeDisplayIterator (const Edit& edit, TimePosition startTime,
                                                  TimecodeSnapType minSnapTypeToUse, bool to)
    : sequence (edit.tempoSequence),
      minSnapType (minSnapTypeToUse),
      time (minSnapType.roundTimeDown (startTime, sequence)),
      isTripletOverride (to)
{
}

TimePosition TimecodeDisplayIterator::next()
{
    bool triplets = isTripletOverride || sequence.isTripletsAtTime (time);
    auto nextTime = std::max (0_tp, minSnapType.roundTimeUp (time + 1.0e-5s, sequence, triplets));

    if (nextTime <= time)
    {
        jassertfalse;
        nextTime = time + minSnapType.getApproxIntervalTime (*sequence.getTempo (0), triplets);
    }

    time = nextTime;
    currentSnapType = minSnapType.getSnapTypeForMaximumSnapLevelOf (time, sequence, triplets);
    return time;
}

juce::String TimecodeDisplayIterator::getTimecodeAsString() const
{
    return currentSnapType.getTimecodeString (time, sequence, true);
}

bool TimecodeDisplayIterator::isOneBarOrGreater() const noexcept
{
    return currentSnapType.getLevel() >= currentSnapType.getOneBarLevel();
}

}} // namespace tracktion { inline namespace engine
