/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class AutomationCurve
{
public:
    AutomationCurve();
    AutomationCurve (const juce::ValueTree& parent, const juce::ValueTree& state);
    AutomationCurve (const AutomationCurve&);
    AutomationCurve& operator= (const AutomationCurve&);
    ~AutomationCurve();

    void setState (const juce::ValueTree&);
    void setParentState (const juce::ValueTree&);
    void setOwnerParameter (AutomatableParameter*);

    AutomatableParameter* getOwnerParameter() const noexcept        { return ownerParam; }

    //==============================================================================
    struct AutomationPoint
    {
        AutomationPoint() noexcept  {}

        AutomationPoint (double t, float v, float c) noexcept
            : time (t), value (v), curve (c)
        {
            jassert (c >= -1.0 && c <= 1.0);
            jassert (t >= 0);
        }

        juce::ValueTree toValueTree() const;

        double time = 0;
        float value = 0, curve = 0;

        bool operator< (const AutomationPoint& other) const     { return time < other.time; }
    };

    //==============================================================================
    int getNumPoints() const noexcept;
    double getLength() const;
    juce::Range<float> getValueLimits() const;

    AutomationPoint getPoint (int index) const noexcept;
    double getPointTime (int index) const noexcept;
    float getPointValue (int index) const noexcept;
    float getPointCurve (int index) const noexcept;

    CurvePoint getBezierHandle (int index) const noexcept;
    CurvePoint getBezierPoint (int index) const noexcept;
    void getBezierEnds (int index, double& x1, float& y1, double& x2, float& y2) const noexcept;

    float getValueAt (double time) const;

    int indexBefore (double time) const;
    int nextIndexAfter (double time) const;

    // returns the index of the next index after this point, xToYRatio is 1 scren unit in value / 1 screen unit in time
    int getNearestPoint (double& t, float& v, double xToYRatio) const;

    int countPointsInRegion (EditTimeRange) const;

    //==============================================================================
    void clear();

    // returns index of new point
    int addPoint (double time, float value, float curve);
    void removePoint (int index);
    void removePointsInRegion (EditTimeRange);
    void removeRegionAndCloseGap (EditTimeRange);
    void removeRedundantPoints (EditTimeRange);

    juce::Array<AutomationPoint> getPointsInRegion (EditTimeRange range) const;

    // returns the new index of the point, which may have changed
    int movePoint (int index, double newTime, float newValue, bool removeInterveningPoints);

    void setPointTime (int index, double newTime);
    void setPointValue (int index, float newValue);
    void setCurveValue (int index, float newCurve);

    //==============================================================================
    void mergeOtherCurve (const AutomationCurve& source,
                          EditTimeRange destRange,
                          double sourceStartTime,
                          double fadeLength,
                          bool leaveOpenAtStart,
                          bool leaveOpenEnded);

    void simplify (EditTimeRange range, double minTimeDifference, float minValueDifference);
    void rescaleAllTimes (double factor);
    void addToAllTimes (double delta);
    void rescaleValues (float factor, EditTimeRange range);
    void addToValues (float valueDelta, EditTimeRange range);

    static double getBezierXfromT (double t, double x1, double xb, double x2);
    static float getBezierYFromX (double t, double x1, float y1, double xb, float yb, double x2, float y2);

    static void removeAllAutomationCurvesRecursively (const juce::ValueTree&);

    juce::ValueTree parentState, state;

private:
    AutomatableParameter* ownerParam = nullptr;

    juce::UndoManager* getUndoManager() const;
    void addPointAtIndex (int index, double t, float v, float c);
    void checkParenthoodStatus();

    JUCE_LEAK_DETECTOR (AutomationCurve)
};

//==============================================================================
/** Removes points from the curve to simplfy it and returns the number of points removed. */
int simplify (AutomationCurve&, int strength, EditTimeRange range);

} // namespace tracktion_engine
