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
    AutomationCurve();
    AutomationCurve (const juce::ValueTree& parent, const juce::ValueTree& state);
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

        AutomationPoint (TimePosition t, float v, float c) noexcept
            : time (t), value (v), curve (c)
        {
            jassert (c >= -1.0 && c <= 1.0);
            jassert (t.inSeconds() >= 0);
        }

        juce::ValueTree toValueTree() const;

        TimePosition time;
        float value = 0, curve = 0;

        bool operator< (const AutomationPoint& other) const     { return time < other.time; }
    };

    //==============================================================================
    int getNumPoints() const noexcept;
    TimeDuration getLength() const;

    AutomationPoint getPoint (int index) const noexcept;
    TimePosition getPointTime (int index) const noexcept;
    float getPointValue (int index) const noexcept;
    float getPointCurve (int index) const noexcept;

    CurvePoint getBezierHandle (int index, TempoSequence&) const noexcept;
    CurvePoint getBezierPoint (int index) const noexcept;
    void getBezierEnds (int index, double& x1, float& y1, double& x2, float& y2) const noexcept;

    float getValueAt (const AutomatableParameter&, TimePosition) const;

    int indexBefore (TimePosition) const;
    int nextIndexAfter (TimePosition) const;

    // returns the index of the next index after this point, xToYRatio is 1 scren unit in value / 1 screen unit in time
    //ddd int getNearestPoint (TimePosition&, float& v, double xToYRatio) const;

    int countPointsInRegion (TimeRange) const;

    //==============================================================================
    void clear (juce::UndoManager*);

    // returns index of new point
    int addPoint (TimePosition, float value, float curve, juce::UndoManager*);
    void removePoint (int index, juce::UndoManager*);
    void removePointsInRegion (TimeRange, juce::UndoManager*);
    void removeRegionAndCloseGap (AutomatableParameter&, TimeRange, juce::UndoManager*);
    void removeRedundantPoints (TimeRange, juce::UndoManager*);

    juce::Array<AutomationPoint> getPointsInRegion (TimeRange) const;

    // returns the new index of the point, which may have changed
    int movePoint (AutomatableParameter&, int index, TimePosition, float newValue,
                   bool removeInterveningPoints, juce::UndoManager*);

    void setPointTime (int index, TimePosition, juce::UndoManager*);
    void setPointValue (int index, float newValue, juce::UndoManager*);
    void setCurveValue (int index, float newCurve, juce::UndoManager*);

    //==============================================================================
    void simplify (TimeRange, double minTimeDifference, float minValueDifference, juce::UndoManager*);
    void rescaleAllTimes (double factor, juce::UndoManager*);
    void addToAllTimes (TimeDuration delta, juce::UndoManager*);
    void rescaleValues (float factor, TimeRange, juce::Range<float> valueRange, juce::UndoManager*);
    void addToValues (float valueDelta, TimeRange, juce::Range<float> valueRange, juce::UndoManager*);

    static double getBezierXfromT (double t, double x1, double xb, double x2);
    static float getBezierYFromX (double t, double x1, float y1, double xb, float yb, double x2, float y2);

    static void removeAllAutomationCurvesRecursively (const juce::ValueTree&);

    juce::ValueTree parentState, state;

private:
    void addPointAtIndex (int index, TimePosition, float v, float c, juce::UndoManager*);
    void checkParenthoodStatus (juce::UndoManager*);

    JUCE_LEAK_DETECTOR (AutomationCurve)
};

//==============================================================================
/** Removes points from the curve to simplfy it and returns the number of points removed. */
int simplify (AutomationCurve&, int strength,
              TimeRange, juce::Range<float> valueRange,
              juce::UndoManager*);

struct ParameterAndCurve
{
    AutomatableParameter& param;
    AutomationCurve& curve;
};

void mergeCurve (const ParameterAndCurve source,
                 TimePosition sourceStartTime,
                 ParameterAndCurve dest,
                 TimeRange destRange,
                 TimeDuration fadeLength,
                 bool leaveOpenAtStart,
                 bool leaveOpenEnded);



} // namespace tracktion::inline engine
