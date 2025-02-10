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

AutomationCurve::AutomationCurve()
    : state (IDs::AUTOMATIONCURVE)
{
    bypass.referTo (state, IDs::bypass, nullptr);
}

AutomationCurve::AutomationCurve (const juce::ValueTree& p, const juce::ValueTree& v)
    : parentState (p), state (v)
{
    if (! state.isValid())
        state = juce::ValueTree (IDs::AUTOMATIONCURVE);

    bypass.referTo (state, IDs::bypass, nullptr);
}

AutomationCurve::AutomationCurve (const AutomationCurve& o)
    : parentState (o.parentState), state (o.state)
{
    bypass.referTo (state, IDs::bypass, nullptr);
}

void AutomationCurve::setState (const juce::ValueTree& v)
{
    state = v;
    jassert (state.hasType (IDs::AUTOMATIONCURVE));
    jassert (state.getParent() == parentState);
    bypass.referTo (state, IDs::bypass, nullptr);
}

void AutomationCurve::setParentState (const juce::ValueTree& v)
{
    parentState = v;
}

void AutomationCurve::setParameterID (juce::String paramID)
{
    state.setProperty (IDs::paramID, std::move (paramID), nullptr);
}

juce::String AutomationCurve::getParameterID() const
{
    return state[IDs::paramID];
}

//==============================================================================
int AutomationCurve::getNumPoints() const noexcept
{
    return state.getNumChildren();
}

AutomationCurve::AutomationPoint AutomationCurve::getPoint (int index) const noexcept
{
    assert (index >= 0);
    assert (index < getNumPoints());
    auto child = state.getChild (index);

    //ddd if (! child.isValid() && ownerParam != nullptr)
    //     return AutomationPoint ({}, ownerParam->getCurrentValue(), 0);

    return AutomationPoint (TimePosition::fromSeconds (static_cast<double> (child.getProperty (IDs::t))),
                            child.getProperty (IDs::v),
                            child.getProperty (IDs::c));
}

TimePosition AutomationCurve::getPointTime (int index) const noexcept
{
    return TimePosition::fromSeconds (static_cast<double> (state.getChild (index).getProperty (IDs::t)));
}

float AutomationCurve::getPointValue (int index) const noexcept
{
    assert (index >= 0);
    assert (index < getNumPoints());
    //ddd if (index >= getNumPoints() && ownerParam != nullptr)
    //     return ownerParam->getCurrentBaseValue();

    return state.getChild (index).getProperty (IDs::v);
}

float AutomationCurve::getPointCurve (int index) const noexcept
{
    return state.getChild (index).getProperty (IDs::c);
}

int AutomationCurve::indexBefore (TimePosition t) const
{
    for (int i = getNumPoints(); --i >= 0;)
        if (getPointTime (i) <= t)
            return i;

    return -1;
}

int AutomationCurve::nextIndexAfter (TimePosition t) const
{
    auto num = getNumPoints();

    for (int i = 0; i < num; ++i)
        if (getPointTime (i) >= t)
            return i;

    return num;
}

TimeDuration AutomationCurve::getLength() const
{
    return toDuration (getPointTime (getNumPoints() - 1));
}

//==============================================================================
float AutomationCurve::getValueAt (const AutomatableParameter& param, TimePosition timePos) const
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    const auto index = nextIndexAfter (timePos);
    const auto time = timePos.inSeconds();

    auto numPoints = getNumPoints();

    if (index <= 0)
        return numPoints > 0 ? getPointValue (0)
                             : param.getCurrentBaseValue();

    if (index >= numPoints)
        return getPointValue (numPoints - 1);

    auto p1 = state.getChild (index - 1);
    auto p2 = state.getChild (index);

    const double time1 = p1.getProperty (IDs::t);
    const float curve1 = p1.getProperty (IDs::c);
    const float value1 = p1.getProperty (IDs::v);

    const double time2 = p2.getProperty (IDs::t);
    const float value2 = p2.getProperty (IDs::v);

    auto& ts = getTempoSequence (param);

    if (curve1 == 0.0f)
    {
        auto alpha = (float) ((time - time1) / (time2 - time1));
        return value1 + alpha * (value2 - value1);
    }

    if (curve1 >= -0.5f && curve1 <= 0.5f)
    {
        auto bezierPoint = getBezierPoint (index - 1);
        return getBezierYFromX (time, time1, value1, toTime (bezierPoint.time, ts).inSeconds(), bezierPoint.value, time2, value2);
    }

    double x1, x2;
    float y1, y2;
    getBezierEnds (index - 1, x1, y1, x2, y2);

    if (time >= time1 && time <= x1)
        return value1;

    if (time >= x2 && time <= time2)
        return value2;

    auto bezierPoint = getBezierPoint (index - 1);
    return getBezierYFromX (time, x1, y1, toTime (bezierPoint.time, ts).inSeconds(), bezierPoint.value, x2, y2);
}

static double getDistanceFromLine (double& x, double& y,
                                   double x1, double y1,
                                   double x2, double y2)
{
    auto dx = x2 - x1;
    auto dy = y2 - y1;
    auto length = hypot (dx, dy);

    auto alongLine = (length > 0) ? juce::jlimit (0.0, 1.0, ((x - x1) * dx + (y - y1) * dy) / (length * length))
                                  : ((x < x1) ? 0.0 : 1.0);

    auto nearX = x1 + (x2 - x1) * alongLine;
    auto nearY = y1 + (y2 - y1) * alongLine;
    x -= nearX;
    x *= x;
    y -= nearY;
    y *= y;

    auto dist = std::sqrt (x + y);
    x = nearX;
    y = nearY;

    return dist;
}

//ddd int AutomationCurve::getNearestPoint (TimePosition& t, float& v, double xToYRatio) const
// {
//     auto numPoints = getNumPoints();
//
//     if (numPoints > 1)
//     {
//         if (t <= getPointTime (0))
//         {
//             v = getPointValue (0);
//             return 0;
//         }
//
//         if (t >= getPointTime (numPoints - 1))
//         {
//             v = getPointValue (numPoints - 1);
//             return numPoints;
//         }
//
//         double bestDist = 1e10;
//         auto bestTime = t;
//         double bestValue = v;
//         int nextIndex = 0;
//
//         for (int i = 0; i < numPoints - 1; ++i)
//         {
//             auto p1 = state.getChild (i);
//             auto p2 = state.getChild (i + 1);
//
//             const auto time1 = TimePosition::fromSeconds (static_cast<double> (p1.getProperty (IDs::t)));
//             const float value1 = p1.getProperty (IDs::v);
//
//             const auto time2 = TimePosition::fromSeconds (static_cast<double> (p2.getProperty (IDs::t)));
//             const float value2 = p2.getProperty (IDs::v);
//
//             auto x = t.inSeconds();
//             auto y = xToYRatio * v;
//
//             auto dist = getDistanceFromLine (x, y,
//                                              time1.inSeconds(), xToYRatio * value1,
//                                              time2.inSeconds(), xToYRatio * value2);
//             y /= xToYRatio;
//
//             if (dist < bestDist)
//             {
//                 bestDist = dist;
//                 bestTime = TimePosition::fromSeconds (x);
//                 bestValue = y;
//                 nextIndex = i + 1;
//             }
//         }
//
//         t = bestTime;
//         v = (float) bestValue;
//
//         return nextIndex;
//     }
//
//     if (numPoints > 0)
//     {
//         v = getPointValue (0);
//         return t > getPointTime (0) ? 1 : 0;
//     }
//
//     if (ownerParam != nullptr)
//         v = ownerParam->getCurrentValue();
//     else
//         v = 0.0f;
//
//     return 0;
// }

//==============================================================================
juce::ValueTree AutomationCurve::AutomationPoint::toValueTree() const
{
    return createValueTree (IDs::POINT,
                            IDs::t, time.inSeconds(),
                            IDs::v, value,
                            IDs::c, curve);
}

int AutomationCurve::addPoint (TimePosition time, float value, float curve, juce::UndoManager* um)
{
    int i;
    for (i = getNumPoints(); --i >= 0;)
        if (getPointTime (i) <= time)
            break;

    addPointAtIndex (++i, time, value, curve, um);
    return i;
}

void AutomationCurve::addPointAtIndex (int index, TimePosition time, float value, float curve, juce::UndoManager* um)
{
    state.addChild (AutomationPoint (time, value, curve).toValueTree(), index, um);
    checkParenthoodStatus (um);
}

void AutomationCurve::removePoint (int index, juce::UndoManager* um)
{
    state.removeChild (index, um);
    checkParenthoodStatus (um);
}

void AutomationCurve::checkParenthoodStatus (juce::UndoManager* um)
{
    bool hasParent = state.getParent() == parentState;
    bool needsParent = getNumPoints() > 0;

    jassert (hasParent || ! state.getParent().isValid());

    if (needsParent != hasParent)
    {
        if (needsParent)
            parentState.addChild (state, -1, um);
        else
            parentState.removeChild (state, um);
    }
}

void AutomationCurve::setPointTime  (int index, TimePosition newTime, juce::UndoManager* um)    { state.getChild (index).setProperty (IDs::t, newTime.inSeconds(),  um); }
void AutomationCurve::setPointValue (int index, float newValue, juce::UndoManager* um)          { state.getChild (index).setProperty (IDs::v, newValue, um); }
void AutomationCurve::setCurveValue (int index, float newCurve, juce::UndoManager* um)          { state.getChild (index).setProperty (IDs::c, newCurve, um); }

int AutomationCurve::movePoint (AutomatableParameter& param, int index, TimePosition newTime, float newValue, bool removeInterveningPoints, juce::UndoManager* um)
{
    if (juce::isPositiveAndBelow (index, getNumPoints()))
    {
        if (removeInterveningPoints)
        {
            auto oldTime = getPointTime (index);
            const bool movingPointBack = newTime < oldTime;

            auto t1 = std::min (newTime, oldTime) - TimeDuration::fromSeconds (0.00001);
            auto t2 = std::max (newTime, oldTime) + TimeDuration::fromSeconds (0.00001);

            for (int i = getNumPoints(); --i >= 0;)
            {
                auto t = getPointTime (i);

                if (t < t1)
                    break;

                // If points lay at the same time, don't remove them
                if (t == oldTime)
                {
                    if (movingPointBack)
                    {
                        if (index < i)
                           break;
                    }
                    else
                    {
                        if (index > i)
                           break;
                    }
                }

                if (t < t2 && i != index)
                {
                    if (i < index)
                        --index;

                    removePoint (i, um);
                }
            }
        }

        if (index > 0)
            newTime = std::max (getPointTime (index - 1), newTime);
        else
            newTime = std::max (0_tp, newTime);

        if (index < getNumPoints() - 1)
            newTime = std::min (newTime, getPointTime (index + 1));

        newValue = param.getValueRange().clipValue (param.snapToState (newValue));

        auto v = state.getChild (index);

        v.setProperty (IDs::t, newTime.inSeconds(), um);
        v.setProperty (IDs::v, newValue, um);
    }
    else
    {
        jassertfalse;
    }

    return index;
}

//==============================================================================
void AutomationCurve::clear (juce::UndoManager* um)
{
    state.removeAllChildren (um);
}

juce::Array<AutomationCurve::AutomationPoint> AutomationCurve::getPointsInRegion (TimeRange range) const
{
    juce::Array<AutomationPoint> results;
    auto numPoints = getNumPoints();

    for (int i = 0; i < numPoints; ++i)
    {
        auto v = state.getChild (i);
        auto t = TimePosition::fromSeconds (static_cast<double> (v.getProperty (IDs::t)));

        if (range.contains (t))
            results.add (AutomationPoint (t, v.getProperty (IDs::v), v.getProperty (IDs::c)));
    }

    return results;
}

void AutomationCurve::removePointsInRegion (TimeRange range, juce::UndoManager* um)
{
    for (int i = getNumPoints(); --i >= 0;)
    {
        auto t = getPointTime (i);

        if (t < range.getStart())
            break;

        if (t < range.getEnd())
            removePoint (i, um);
    }
}

void AutomationCurve::removeRedundantPoints (TimeRange range, juce::UndoManager* um)
{
    constexpr auto threshold = 0.0001;

    for (int i = getNumPoints(); --i >= 0;)
    {
        auto t = getPointTime (i);

        if (! range.contains (t))
            continue;

        auto v = getPointValue (i);

        bool sameLeft  = i <= 0                  || std::abs (getPointValue (i - 1) - v) < threshold;
        bool sameRight = i >= getNumPoints() - 1 || std::abs (getPointValue (i + 1) - v) < threshold;

        // if points to left and right have same value
        if (sameLeft && sameRight)
        {
            removePoint (i, um);
            continue;
        }

        // if point to right is exact same
        if (i < getNumPoints() - 1
              && std::abs (getPointValue (i + 1) - v) < threshold
              && std::abs ((getPointTime (i + 1) - t).inSeconds()) < threshold)
        {
            removePoint (i, um);
            continue;
        }

        // if points to left and right are at same time
        if (i > 0
             && i < getNumPoints() - 1
             && std::abs ((getPointTime (i - 1) - t).inSeconds()) < threshold
             && std::abs ((getPointTime (i + 1) - t).inSeconds()) < threshold)
        {
            removePoint (i, um);
            continue;
        }

        if (getNumPoints() <= 1)
            break;
    }
}

void AutomationCurve::removeRegionAndCloseGap (AutomatableParameter& param, TimeRange range, juce::UndoManager* um)
{
    auto valAtStart = getValueAt (param, range.getStart());
    auto valAtEnd   = getValueAt (param, range.getEnd());

    if (getNumPoints() == 0)
        return;

    for (int i = getNumPoints(); --i >= 0;)
    {
        auto t = getPointTime (i);

        if (t >= range.getStart() && t <= range.getEnd())
            removePoint (i, um);
    }

    for (int i = 0; i < getNumPoints(); ++i)
    {
        auto t = getPointTime (i);

        if (t >= range.getEnd())
            movePoint (param, i, t - range.getLength(), getPointValue(i), false, um);
    }

    if (getValueAt (param, range.getStart()) != valAtStart)
        addPoint (range.getStart(), valAtStart, 0.0f, um);

    if (valAtStart != valAtEnd)
        addPoint (range.getStart(), valAtEnd, 0.0f, um);
}

int AutomationCurve::countPointsInRegion (TimeRange range) const
{
    int num = 0;

    for (int i = getNumPoints(); --i >= 0;)
    {
        auto t = getPointTime(i);

        if (t < range.getStart())
            break;

        if (t < range.getEnd())
            ++num;
    }

    return num;
}

//ddd void AutomationCurve::mergeOtherCurve (const AutomationCurve& source,
//                                        AutomatableParameter& destParam,
//                                        TimeRange destRange,
//                                        TimePosition sourceStartTime,
//                                        TimeDuration fadeLength,
//                                        bool leaveOpenAtStart,
//                                        bool leaveOpenEnded)
// {
//     auto um = getUndoManager_p (destParam);
//     auto sourceEndTime = sourceStartTime + destRange.getLength();
//     auto dstValueAtStart = getValueAt (destParam, destRange.getStart());
//     auto dstValueAtEnd   = getValueAt (destParam, destRange.getEnd());
//
//     auto srcValueAtStart = source.getValueAt (destParam, sourceStartTime);
//     auto srcValueAtEnd = source.getValueAt (destParam, sourceEndTime);
//
//     removePointsInRegion (destRange, um);
//
//     if (fadeLength == TimeDuration() && dstValueAtStart != srcValueAtStart)
//         addPoint (destRange.getStart(), dstValueAtStart, 0.0f, um);
//
//     if (! leaveOpenAtStart)
//         addPoint (destRange.getStart(), srcValueAtStart, 0.0f, um);
//
//     bool pointsInFadeZoneStart = false, pointsInFadeZoneEnd = false;
//
//     for (int i = 0; i < source.getNumPoints(); ++i)
//     {
//         auto t = source.getPointTime (i) + (destRange.getStart() - sourceStartTime);
//
//         if (t >= destRange.getStart() && t <= destRange.getEnd())
//         {
//             auto v = source.getPointValue(i);
//             auto c = source.getPointCurve(i);
//
//             // see if this point is in a fade zone..
//             if (t <= destRange.getStart() + fadeLength)
//             {
//                 pointsInFadeZoneStart = true;
//
//                 if (fadeLength > TimeDuration())
//                     v = (float) (dstValueAtStart + (v - dstValueAtStart) * ((t - destRange.getStart()) / fadeLength));
//             }
//             else if (t >= destRange.getEnd() - fadeLength)
//             {
//                 pointsInFadeZoneEnd = true;
//
//                 if (fadeLength > TimeDuration())
//                     v = (float) (v + (dstValueAtEnd - v) * 1.0 - ((destRange.getEnd() - t) / fadeLength));
//             }
//
//             addPoint (t, v, c, um);
//         }
//     }
//
//     if (fadeLength > 0_td && ! pointsInFadeZoneStart)
//         addPoint (destRange.getStart() + fadeLength - TimeDuration::fromSeconds (0.0001), dstValueAtStart, 0.0f, um);
//
//     if (! leaveOpenEnded)
//     {
//         if (! pointsInFadeZoneEnd)
//             addPoint (destRange.getEnd() - fadeLength, srcValueAtEnd, 0.0f, um);
//
//         addPoint (destRange.getEnd(), dstValueAtEnd, 0.0f, um);
//     }
// }

void AutomationCurve::simplify (TimeRange range, double minTimeDifference, float minValueDifference, juce::UndoManager* um)
{
    auto minDist = std::sqrt (minTimeDifference * minTimeDifference
                                + minValueDifference * minValueDifference);

    for (int i = 1; i < getNumPoints(); ++i)
    {
        auto time2 = getPointTime (i);

        if (range.contains (time2))
        {
            auto time1 = getPointTime (i - 1);

            // look for points too close together
            if (std::abs ((time1 - time2).inSeconds()) < minTimeDifference
                 && std::abs (getPointValue (i - 1) - getPointValue (i)) < minValueDifference)
            {
                removePoint (i, um);
                i = std::max (i - 3, 1);
            }
            else
            {
                // see if three points are in-line
                if (i < getNumPoints() - 1)
                {
                    double x = time2.inSeconds();
                    double y = getPointValue (i);

                    auto dist = getDistanceFromLine (x, y, time1.inSeconds(), getPointValue (i - 1),
                                                     getPointTime (i + 1).inSeconds(), getPointValue (i + 1));

                    if (dist < minDist)
                    {
                        removePoint (i, um);
                        i = std::max (i - 3, 1);
                    }
                }
            }
        }
    }
}

void AutomationCurve::addToAllTimes (TimeDuration delta, juce::UndoManager* um)
{
    if (delta != TimeDuration())
        for (int i = getNumPoints(); --i >= 0;)
            setPointTime (i, getPointTime (i) + delta, um);
}

void AutomationCurve::rescaleAllTimes (double factor, juce::UndoManager* um)
{
    if (factor != 1.0f)
        for (int i = getNumPoints(); --i >= 0;)
            setPointTime (i, TimePosition::fromSeconds (getPointTime (i).inSeconds() * factor), um);
}

void AutomationCurve::rescaleValues (float factor, TimeRange range, juce::Range<float> limits, juce::UndoManager* um)
{
    if (factor != 1.0f)
        for (int i = getNumPoints(); --i >= 0;)
            if (range.contains (getPointTime (i)))
                setPointValue (i, juce::jlimit (limits.getStart(), limits.getEnd(), getPointValue (i) * factor), um);
}

void AutomationCurve::addToValues (float valueDelta, TimeRange range, juce::Range<float> limits, juce::UndoManager* um)
{
    if (valueDelta != 0)
        for (int i = getNumPoints(); --i >= 0;)
            if (range.contains (getPointTime (i)))
                setPointValue (i, juce::jlimit (limits.getStart(), limits.getEnd(), getPointValue (i) + valueDelta), um);
}

CurvePoint AutomationCurve::getBezierPoint (int index) const noexcept
{
    auto x1 = getPointTime  (index);
    auto y1 = getPointValue (index);
    auto x2 = getPointTime  (index + 1);
    auto y2 = getPointValue (index + 1);
    auto c  = juce::jlimit (-1.0f, 1.0f, getPointCurve (index) * 2.0f);

    if (y2 > y1)
    {
        auto run  = x2 - x1;
        auto rise = y2 - y1;

        auto xc = x1 + run / 2;
        auto yc = y1 + rise / 2;

        auto x = xc - run / 2 * -c;
        auto y = yc + rise / 2 * -c;

        return { x, y };
    }

    auto run  = x2 - x1;
    auto rise = y1 - y2;

    auto xc = x1 + run / 2;
    auto yc = y2 + rise / 2;

    auto x = xc - run / 2 * -c;
    auto y = yc - rise / 2 * -c;

    return { x, y };
}

double AutomationCurve::getBezierXfromT (double t, double x1, double xb, double x2)
{
    // test for straight lines and bail out
    if (x1 == x2)
        return (x1 + x2) / 2.0 * t + x1;

    return (std::pow (1.0 - t, 2.0) * x1) + 2 * t * (1 - t) * xb + std::pow (t, 2.0) * x2;
}

float AutomationCurve::getBezierYFromX (double x, double x1, float y1, double xb, float yb, double x2, float y2)
{
    // test for straight lines and bail out
    if (x1 == x2 || y1 == y2)
        return y1;

    // ok, we have a bezier curve with one control point,
    // we know x, we need to find y

    // flip the bezier equation around so its an quadratic equation
    auto a = x1 - 2 * xb + x2;
    auto b = -2 * x1 + 2 * xb;
    auto c = x1 - x;

    // solve for t, [0..1]
    double t;

    if (a == 0.0f)
    {
        t = -c / b;
    }
    else
    {
        t = (-b + std::sqrt (b * b - 4 * a * c)) / (2 * a);

        if (t < 0.0f || t > 1.0f)
            t = (-b - std::sqrt (b * b - 4 * a * c)) / (2 * a);
    }

    // find y using the t we just found
    return (float) ((std::pow (1 - t, 2) * y1) + 2 * t * (1 - t) * yb + std::pow (t, 2) * y2);
}

CurvePoint AutomationCurve::getBezierHandle (int index, TempoSequence& tempoSequence) const noexcept
{
    auto x1 = getPointTime (index).inSeconds();
    auto y1 = getPointValue (index);
    auto c  = getPointCurve (index);

    auto x2 = getPointTime (index + 1).inSeconds();
    auto y2 = getPointValue (index + 1);

    if (x1 == x2 || y1 == y2)
        return { TimePosition::fromSeconds ((x1 + x2) / 2), (y1 + y2) / 2 };

    if (c == 0.0f)
        return { TimePosition::fromSeconds ((x1 + x2) / 2), (y1 + y2) / 2 };

    if (c >= -0.5 && c <= 0.5)
    {
        auto bp = getBezierPoint (index);
        auto x = getBezierXfromT (0.5, x1, toTime (bp.time, tempoSequence).inSeconds(), x2);
        auto y = getBezierYFromX (x, x1, y1, toTime (bp.time, tempoSequence).inSeconds(), bp.value, x2, y2);

        return { TimePosition::fromSeconds (x), y };
    }

    if (c > -1.0 && c < 1.0)
    {
        double x1end, x2end;
        float y1end, y2end;
        getBezierEnds (index, x1end, y1end, x2end, y2end);

        auto bp = getBezierPoint (index);

        auto x = getBezierXfromT (0.5, x1end, toTime (bp.time, tempoSequence).inSeconds(), x2end);
        auto y = getBezierYFromX (x, x1end, y1end, toTime (bp.time, tempoSequence).inSeconds(), bp.value, x2end, y2end);
        return { TimePosition::fromSeconds (x), y };
    }

    double x1end, x2end;
    float y1end, y2end;
    getBezierEnds (index, x1end, y1end, x2end, y2end);
    return { TimePosition::fromSeconds (x1end), y1end };
}

void AutomationCurve::getBezierEnds (int index, double& x1out, float& y1out, double& x2out, float& y2out) const noexcept
{
    auto x1 = getPointTime (index).inSeconds();
    auto y1 = getPointValue (index);
    auto c  = getPointCurve (index);

    auto x2 = getPointTime (index + 1).inSeconds();
    auto y2 = getPointValue (index + 1);

    auto minic = (std::abs (c) - 0.5f) * 2.0f;
    auto run   = (minic) * (x2 - x1);
    auto rise  = (minic) * ((y2 > y1) ? (y2 - y1) : (y1 - y2));

    if (c > 0.0f)
    {
        x1out = x1 + run;
        y1out = y1;

        x2out = x2;
        y2out = (y1 < y2) ? (y2 - rise) : (y2 + rise);
    }
    else
    {
        x1out = x1;
        y1out = (y1 < y2) ? (y1 + rise) : (y1 - rise);

        x2out = x2 - run;
        y2out = y2;
    }
}

void AutomationCurve::removeAllAutomationCurvesRecursively (const juce::ValueTree& v)
{
    for (int i = v.getNumChildren(); --i >= 0;)
    {
        if (v.getChild (i).hasType (IDs::AUTOMATIONCURVE))
            juce::ValueTree (v).removeChild (i, nullptr);
        else
            removeAllAutomationCurvesRecursively (v.getChild (i));
    }
}


//==============================================================================
int simplify (AutomationCurve& curve, int strength,
              TimeRange time, juce::Range<float> valueRange,
              juce::UndoManager* um)
{
    jassert (juce::isPositiveAndNotGreaterThan (strength, 2));

    double td = 0.08;
    float vd = 0.06f;

    if (strength == 0)
    {
        td = 0.03;
        vd = 0.004f;
    }
    else if (strength == 1)
    {
        td = 0.05;
        vd = 0.03f;
    }

    auto range = valueRange.getLength();
    vd *= range;

    auto numPointsBefore = curve.getNumPoints();
    curve.simplify (time, td, vd, um);
    auto numPointsAfter = curve.getNumPoints();

    return numPointsBefore - numPointsAfter;
}

void mergeCurve (const ParameterAndCurve source,
                 TimePosition sourceStartTime,
                 ParameterAndCurve dest,
                 TimeRange destRange,
                 TimeDuration fadeLength,
                 bool leaveOpenAtStart,
                 bool leaveOpenEnded)
{
    auto um = getUndoManager_p (dest.param);
    auto sourceEndTime = sourceStartTime + destRange.getLength();
    auto dstValueAtStart = dest.curve.getValueAt (dest.param, destRange.getStart());
    auto dstValueAtEnd   = dest.curve.getValueAt (dest.param, destRange.getEnd());

    auto srcValueAtStart = source.curve.getValueAt (source.param, sourceStartTime);
    auto srcValueAtEnd = source.curve.getValueAt (source.param, sourceEndTime);

    dest.curve.removePointsInRegion (destRange, um);

    if (fadeLength == TimeDuration() && dstValueAtStart != srcValueAtStart)
        dest.curve.addPoint (destRange.getStart(), dstValueAtStart, 0.0f, um);

    if (! leaveOpenAtStart)
        dest.curve.addPoint (destRange.getStart(), srcValueAtStart, 0.0f, um);

    bool pointsInFadeZoneStart = false, pointsInFadeZoneEnd = false;

    for (int i = 0; i < source.curve.getNumPoints(); ++i)
    {
        auto t = source.curve.getPointTime (i) + (destRange.getStart() - sourceStartTime);

        if (t >= destRange.getStart() && t <= destRange.getEnd())
        {
            auto v = source.curve.getPointValue (i);
            auto c = source.curve.getPointCurve (i);

            // see if this point is in a fade zone..
            if (t <= destRange.getStart() + fadeLength)
            {
                pointsInFadeZoneStart = true;

                if (fadeLength > TimeDuration())
                    v = (float) (dstValueAtStart + (v - dstValueAtStart) * ((t - destRange.getStart()) / fadeLength));
            }
            else if (t >= destRange.getEnd() - fadeLength)
            {
                pointsInFadeZoneEnd = true;

                if (fadeLength > TimeDuration())
                    v = (float) (v + (dstValueAtEnd - v) * 1.0 - ((destRange.getEnd() - t) / fadeLength));
            }

            dest.curve.addPoint (t, v, c, um);
        }
    }

    if (fadeLength > 0_td && ! pointsInFadeZoneStart)
        dest.curve.addPoint (destRange.getStart() + fadeLength - TimeDuration::fromSeconds (0.0001), dstValueAtStart, 0.0f, um);

    if (! leaveOpenEnded)
    {
        if (! pointsInFadeZoneEnd)
            dest.curve.addPoint (destRange.getEnd() - fadeLength, srcValueAtEnd, 0.0f, um);

        dest.curve.addPoint (destRange.getEnd(), dstValueAtEnd, 0.0f, um);
    }
}

} // namespace tracktion::inline engine
