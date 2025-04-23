/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine
{

class AutomationCurve
{
public:
    enum class TimeBase
    {
        time,
        beats
    };

    AutomationCurve (Edit&, TimeBase);
    AutomationCurve (Edit&, TimeBase,
                     const juce::ValueTree& parent, const juce::ValueTree& state);
    AutomationCurve (const AutomationCurve&);

    void setState (const juce::ValueTree&);
    void setParentState (const juce::ValueTree&);

    /** Sets the paramID to store in the state. */
    void setParameterID (juce::String);

    /** Returns the parameter ID set or an empty string. */
    juce::String getParameterID() const;

    //==============================================================================
    /** If set to true, this curve is disabled, having no effect on the AutomatableParameter. */
    juce::CachedValue<AtomicWrapper<bool>> bypass;

    //==============================================================================
    struct AutomationPoint
    {
        AutomationPoint() noexcept = default;

        AutomationPoint (EditPosition t, float v, float c) noexcept
            : time (t), value (v), curve (c)
        {
            jassert (c >= -1.0 && c <= 1.0);
            jassert (toUnderlying (t) >= 0.0);
        }

        juce::ValueTree toValueTree() const;

        EditPosition time;
        float value = 0, curve = 0;

        bool operator< (const AutomationPoint& other) const
        {
            assert (time.isBeats() == other.time.isBeats());
            return toUnderlying (time) < toUnderlying (other.time);
        }
    };

    //==============================================================================
    int getNumPoints() const noexcept;
    EditDuration getDuration() const noexcept;

    AutomationPoint getPoint (int index) const noexcept;
    EditPosition getPointPosition (int index) const noexcept;
    float getPointValue (int index) const noexcept;
    float getPointCurve (int index) const noexcept;

    CurvePoint getBezierHandle (int index) const noexcept;
    CurvePoint getBezierPoint (int index) const noexcept;
    std::pair<CurvePoint,CurvePoint> getBezierEnds (int index) const;

    void getBezierEnds (int index, double& x1, float& y1, double& x2, float& y2) const noexcept;

    float getValueAt (EditPosition, float defaultValue) const;

    int indexBefore (EditPosition) const;
    int nextIndexAfter (EditPosition) const;

    //==============================================================================
    void clear (juce::UndoManager*);

    // returns index of new point
    int addPoint (EditPosition, float value, float curve, juce::UndoManager*);
    void removePoint (int index, juce::UndoManager*);
    void removePoints (EditTimeRange, juce::UndoManager*);
    void removePointsAndCloseGap (AutomatableParameter&, EditTimeRange, juce::UndoManager*);
    void removeRedundantPoints (EditTimeRange, juce::UndoManager*);

    juce::Array<AutomationPoint> getPointsInRegion (EditTimeRange) const;

    // returns the new index of the point, which may have changed
    int movePoint (int index, EditPosition, float newValue, std::optional<juce::Range<float>> valueLimits,
                   bool removeInterveningPoints, juce::UndoManager*);

    void setPointValue (int index, float newValue, juce::UndoManager*);
    void setCurveValue (int index, float newCurve, juce::UndoManager*);

    //==============================================================================
    void rescaleAllPositions (double factor, juce::UndoManager*);
    void rescaleValues (float factor, EditTimeRange, juce::Range<float> valueRange, juce::UndoManager*);
    void addToValues (float valueDelta, EditTimeRange, juce::Range<float> valueRange, juce::UndoManager*);

    void simplify (EditTimeRange, EditDuration minTimeDifference, float minValueDifference, juce::UndoManager*);

    //==============================================================================
    static void removeAllAutomationCurvesRecursively (const juce::ValueTree&);

    //==============================================================================
    Edit& edit;
    const TimeBase timeBase;
    juce::ValueTree parentState, state;

    //==============================================================================
    // Deprecated Time based functions, prefer the generic ones above
    TimePosition getPointTime (int index) const noexcept;
    float getValueAt (TimePosition, float defaultValue) const;
    int indexBefore (TimePosition) const;
    TimeDuration getLength() const;
    int nextIndexAfter (TimePosition) const;
    int countPointsInRegion (EditTimeRange) const;
    int addPoint (TimePosition, float value, float curve, juce::UndoManager*);
    void removePointsInRegion (TimeRange, juce::UndoManager*);
    void removeRegionAndCloseGap (AutomatableParameter&, TimeRange, juce::UndoManager*);
    void removeRedundantPoints (TimeRange, juce::UndoManager*);
    juce::Array<AutomationPoint> getPointsInRegion (TimeRange) const;
    void rescaleValues (float factor, TimeRange, juce::Range<float> valueRange, juce::UndoManager*);
    void addToValues (float valueDelta, TimeRange, juce::Range<float> valueRange, juce::UndoManager*);
    int movePoint (AutomatableParameter&, int index, TimePosition, float newValue,
                   bool removeInterveningPoints, juce::UndoManager*);
    void simplify (TimeRange, double minTimeDifference, float minValueDifference, juce::UndoManager*);

    /** @internal */
    void setPointPosition (int index, EditPosition, juce::UndoManager*);

private:
    EditPosition createPosition (double) const;
    EditPosition getPosition (const juce::ValueTree&) const;
    EditPosition convertPositionToBase (EditPosition) const;
    void setPointTime (int index, TimePosition, juce::UndoManager*);
    void addPointAtIndex (int index, EditPosition, float v, float c, juce::UndoManager*);
    void addPointAtIndex (int index, TimePosition, float v, float c, juce::UndoManager*);
    void checkParenthoodStatus (juce::UndoManager*);

    JUCE_LEAK_DETECTOR (AutomationCurve)
};

//==============================================================================
/** Removes points from the curve to simplify it and returns the number of points removed. */
int simplify (AutomationCurve&, int strength,
              EditTimeRange, juce::Range<float> valueRange,
              juce::UndoManager*);

void mergeCurve (AutomationCurve& dest,
                 EditTimeRange destRange,
                 const AutomationCurve& source,
                 EditPosition sourceStartTime,
                 float defaultValue,
                 EditDuration fadeLength,
                 bool leaveOpenAtStart,
                 bool leaveOpenEnded);

//==============================================================================
/** Returns the value of a parameter's curve at the given position. */
float getValueAt (AutomatableParameter&, EditPosition);

/** Returns the value of a parameter's curve at the given position. */
float getValueAt (AutomatableParameter&, TimePosition);

/** Returns the range the curve occupies. */
EditTimeRange getFullRange (const AutomationCurve&);

} // namespace tracktion::inline engine
