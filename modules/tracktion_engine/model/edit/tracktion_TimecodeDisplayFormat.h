/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

//==============================================================================
/**
*/
enum class TimecodeType
{
    millisecs,
    barsBeats,
    fps24,
    fps25,
    fps30
};


//==============================================================================
/**
    A snap mode, e.g. "nearest beat".
    A list of available types is returned from methods in TimecodeDisplayFormat
*/
struct TimecodeSnapType
{
    //==============================================================================
    // default to minutes/secs, snap to 1ms
    TimecodeSnapType() noexcept = default;
    TimecodeSnapType (TimecodeType t, int lev) noexcept : type (t), level (lev) {}

    //==============================================================================
    juce::String getDescription (const TempoSetting&) const;
    double getApproxIntervalTime (const TempoSetting&) const; // may not be accurate for stuff like ramped tempos

    /** Similar to above expect that the isTripletsOverride argument is used instead of the tempo owner sequence. */
    double getApproxIntervalTime (const TempoSetting&, bool isTripletsOverride) const; // may not be accurate for stuff like ramped tempos

    juce::String getTimecodeString (double time,
                                    const TempoSequence&,
                                    bool useStartLabelIfZero) const;

    double roundTimeDown (double t, const TempoSequence&) const;
    double roundTimeNearest (double t, const TempoSequence&) const;
    double roundTimeUp (double t, const TempoSequence&) const;
    double roundTimeUp (double t, const TempoSequence&, bool tripletsOverride) const;

    //==============================================================================
    int getLevel() const noexcept                   { return level; }
    int getOneBarLevel() const noexcept;

    // find the maximum snap level for which this time can be considered 'on an interval'
    TimecodeSnapType getSnapTypeForMaximumSnapLevelOf (double t, const TempoSequence&) const;
    TimecodeSnapType getSnapTypeForMaximumSnapLevelOf (double t, const TempoSequence&, bool isTripletsOverride) const;

    // get a snap type that rounds things to 1 beat
    static TimecodeSnapType get1BeatSnapType();

    TimecodeType type = TimecodeType::millisecs;
    int level = 0;

private:
    double getIntervalNonBarsBeats() const;
    double roundTime (double t, const TempoSequence&, double adjustment) const;
    double roundTime (double t, const TempoSequence&, double adjustment, bool isTripletsOverride) const;
    double roundTimeNearest (double t, const TempoSequence&, bool isTripletsOverride) const;
};


//==============================================================================
/**
    A timecode display mode, e.g. bars/beats, seconds/frames, etc.
*/
struct TimecodeDisplayFormat
{
    //==============================================================================
    TimecodeType type;

    //==============================================================================
    // (defaults to barsBeats, 24fps
    TimecodeDisplayFormat() noexcept    : type (TimecodeType::barsBeats) {}
    TimecodeDisplayFormat (TimecodeType t) noexcept  : type (t) {}

    bool operator== (const TimecodeDisplayFormat& other) const      { return type == other.type; }
    bool operator!= (const TimecodeDisplayFormat& other) const      { return type != other.type; }

    //==============================================================================
    bool isBarsBeats() const;
    bool isMilliseconds() const;
    bool isSMPTE() const;
    int getFPS() const;

    //==============================================================================
    // general description of how the rounding will be done - e.g. "Snap to nearest beat"
    juce::String getRoundingDescription() const;

    juce::String getString (const TempoSequence&, double time, bool isRelative) const;
    juce::String getStartLabel() const;

    /** number of sections in the timecode string */
    int getNumParts() const;
    juce::String getSeparator (int part) const;
    int getMaxCharsInPart (int part, bool canBeNegative) const;
    int getMaxValueOfPart (const TempoSequence&, double currentTime, int part, bool isRelative) const;
    int getMinValueOfPart (int part, bool isRelative) const;

    void getPartStrings (double time, const TempoSequence&, bool isRelative, juce::String results[4]) const;
    double getNewTimeWithPartValue (double oldTime, const TempoSequence&,
                                    int part, int newValue, bool isRelative) const;

    //==============================================================================
    TimecodeSnapType getBestSnapType (const TempoSetting&, double onScreenTimePerPixel) const;

    int getNumSnapTypes() const;
    TimecodeSnapType getSnapType (int index) const;

    int getSubSecondDivisions() const; // ticks, fps or 1000 for minutes/secs

    /** this will format the time as "hh:mm:ss.xxx".

        The xxx depends on the subSecondDivisor value - e.g. 1000 will show xxx as milliseconds,
        if subSecondDivisor is 25 xxx will be 0 to 24 (for frames). If subSecondDivisor is 0, no
        sub-second portion will be produced.
    */
    static juce::String toFullTimecode (double seconds,
                                        const int subSecondDivisions = 1000,
                                        bool showHours = false);
};


//==============================================================================
/**
    Iterates along a timeline for drawing things like the ticks on the timebar.
*/
struct TimecodeDisplayIterator
{
    TimecodeDisplayIterator (const Edit&, double startTime, TimecodeSnapType minSnapTypeToUse);

    /** returns the next time. */
    double next();

    juce::String getTimecodeAsString() const;

    /** the resolution level of the timecode that the current interval is at */
    int getCurrentResolutionLevel() const noexcept  { return currentSnapType.getLevel(); }
    int getMinimumResolutionLevel() const noexcept  { return minSnapType.getLevel(); }

    // true if the current resolution is at least bar level
    bool isOneBarOrGreater() const noexcept;

private:
    const TempoSequence& sequence;
    TimecodeSnapType minSnapType, currentSnapType;
    double time;

    JUCE_DECLARE_NON_COPYABLE (TimecodeDisplayIterator)
};


} // namespace tracktion_engine

//==============================================================================

namespace juce
{

template<>
struct VariantConverter<tracktion_engine::TimecodeDisplayFormat>
{
    static tracktion_engine::TimecodeDisplayFormat fromVar (const var& v)
    {
        if (v == "beats") return tracktion_engine::TimecodeType::barsBeats;
        if (v == "fps24") return tracktion_engine::TimecodeType::fps24;
        if (v == "fps25") return tracktion_engine::TimecodeType::fps25;
        if (v == "fps30") return tracktion_engine::TimecodeType::fps30;

        return tracktion_engine::TimecodeType::millisecs;
    }

    static var toVar (tracktion_engine::TimecodeDisplayFormat t)
    {
        if (t.type == tracktion_engine::TimecodeType::barsBeats)  return "beats";
        if (t.type == tracktion_engine::TimecodeType::fps24)  return "fps24";
        if (t.type == tracktion_engine::TimecodeType::fps25)  return "fps25";
        if (t.type == tracktion_engine::TimecodeType::fps30)  return "fps30";

        return "seconds";
    }
};

}
