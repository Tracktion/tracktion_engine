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

AutomationCurve::AutomationCurve (Edit& e, TimeBase tb)
    : edit (e), timeBase (tb), state (IDs::AUTOMATIONCURVE)
{
    bypass.referTo (state, IDs::bypass, nullptr);
}

AutomationCurve::AutomationCurve (Edit& e, TimeBase tb,
                                  const juce::ValueTree& p, const juce::ValueTree& v)
    : edit (e), timeBase (tb), parentState (p), state (v)
{
    if (! state.isValid())
        state = juce::ValueTree (IDs::AUTOMATIONCURVE);

    bypass.referTo (state, IDs::bypass, nullptr);
}

AutomationCurve::AutomationCurve (const AutomationCurve& o)
    : edit (o.edit), timeBase (o.timeBase),
      parentState (o.parentState), state (o.state)
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

EditDuration AutomationCurve::getDuration() const noexcept
{
    if (timeBase == TimeBase::time)
        return TimeDuration::fromSeconds (toUnderlying (getPointPosition (getNumPoints() - 1)));

    return BeatDuration::fromBeats (toUnderlying (getPointPosition (getNumPoints() - 1)));
}

EditPosition AutomationCurve::getPointPosition (int index) const noexcept
{
    return getPosition (state.getChild (index));
}

AutomationCurve::AutomationPoint AutomationCurve::getPoint (int index) const noexcept
{
    assert (index >= 0);
    assert (index < getNumPoints());
    auto child = state.getChild (index);

    return AutomationPoint (getPosition (child),
                            child.getProperty (IDs::v),
                            child.getProperty (IDs::c));
}

TimePosition AutomationCurve::getPointTime (int index) const noexcept
{
    auto pointState = state.getChild (index);
    auto pos = createPosition (static_cast<double> (pointState[IDs::t]));
    return toTime (pos, edit.tempoSequence);
}

float AutomationCurve::getPointValue (int index) const noexcept
{
    assert (index >= 0);
    assert (index < getNumPoints());

    return state.getChild (index).getProperty (IDs::v);
}

float AutomationCurve::getPointCurve (int index) const noexcept
{
    return state.getChild (index).getProperty (IDs::c);
}

int AutomationCurve::indexBefore (EditPosition p) const
{
    for (int i = getNumPoints(); --i >= 0;)
        if (lessThanOrEqualTo (getPointPosition (i), p, edit.tempoSequence))
            return i;

    return -1;
}

int AutomationCurve::indexBefore (TimePosition t) const
{
    for (int i = getNumPoints(); --i >= 0;)
        if (getPointTime (i) <= t)
            return i;

    return -1;
}

int AutomationCurve::nextIndexAfter (EditPosition p) const
{
    auto num = getNumPoints();

    for (int i = 0; i < num; ++i)
        if (greater (getPointPosition (i), p, edit.tempoSequence))
            return i;

    return num;
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
float AutomationCurve::getValueAt (EditPosition editPos, float defaultValue) const
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    const auto index = nextIndexAfter (editPos);
    const auto time = toUnderlying (editPos);

    auto numPoints = getNumPoints();

    if (index <= 0)
        return numPoints > 0 ? getPointValue (0)
                             : defaultValue;

    if (index >= numPoints)
        return getPointValue (numPoints - 1);

    auto p1 = state.getChild (index - 1);
    auto p2 = state.getChild (index);

    const double time1 = p1.getProperty (IDs::t);
    const float curve1 = p1.getProperty (IDs::c);
    const float value1 = p1.getProperty (IDs::v);

    const double time2 = p2.getProperty (IDs::t);
    const float value2 = p2.getProperty (IDs::v);

    if (curve1 == 0.0f)
    {
        auto alpha = (float) ((time - time1) / (time2 - time1));
        return value1 + alpha * (value2 - value1);
    }

    if (curve1 >= -0.5f && curve1 <= 0.5f)
    {
        auto bezierPoint = getBezierPoint (index - 1);
        return static_cast<float> (getBezierYFromX (time, time1, value1, toUnderlying (bezierPoint.time), bezierPoint.value, time2, value2));
    }

    double x1, x2;
    float y1, y2;
    getBezierEnds (index - 1, x1, y1, x2, y2);

    if (time >= time1 && time <= x1)
        return value1;

    if (time >= x2 && time <= time2)
        return value2;

    auto bezierPoint = getBezierPoint (index - 1);
    return static_cast<float> (getBezierYFromX (time, x1, y1, toUnderlying (bezierPoint.time), bezierPoint.value, x2, y2));
}

float AutomationCurve::getValueAt (TimePosition timePos, float defaultValue) const
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    const auto index = nextIndexAfter (timePos);
    const auto time = timePos.inSeconds();

    auto numPoints = getNumPoints();

    if (index <= 0)
        return numPoints > 0 ? getPointValue (0)
                             : defaultValue;

    if (index >= numPoints)
        return getPointValue (numPoints - 1);

    auto p1 = state.getChild (index - 1);
    auto p2 = state.getChild (index);

    const double time1 = p1.getProperty (IDs::t);
    const float curve1 = p1.getProperty (IDs::c);
    const float value1 = p1.getProperty (IDs::v);

    const double time2 = p2.getProperty (IDs::t);
    const float value2 = p2.getProperty (IDs::v);

    auto& ts = getTempoSequence (edit);

    if (curve1 == 0.0f)
    {
        auto alpha = (float) ((time - time1) / (time2 - time1));
        return value1 + alpha * (value2 - value1);
    }

    if (curve1 >= -0.5f && curve1 <= 0.5f)
    {
        auto bezierPoint = getBezierPoint (index - 1);
        return static_cast<float> (static_cast<float> (getBezierYFromX (time, time1, value1, toTime (bezierPoint.time, ts).inSeconds(), bezierPoint.value, time2, value2)));
    }

    double x1, x2;
    float y1, y2;
    getBezierEnds (index - 1, x1, y1, x2, y2);

    if (time >= time1 && time <= x1)
        return value1;

    if (time >= x2 && time <= time2)
        return value2;

    auto bezierPoint = getBezierPoint (index - 1);
    return static_cast<float> (getBezierYFromX (time, x1, y1, toTime (bezierPoint.time, ts).inSeconds(), bezierPoint.value, x2, y2));
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


//==============================================================================
juce::ValueTree AutomationCurve::AutomationPoint::toValueTree() const
{
    return createValueTree (IDs::POINT,
                            IDs::t, toUnderlying (time),
                            IDs::v, value,
                            IDs::c, curve);
}

int AutomationCurve::addPoint (EditPosition pos, float value, float curve, juce::UndoManager* um)
{
    auto& ts = edit.tempoSequence;
    int i = 0;

    if (timeBase == TimeBase::beats)
    {
        auto beats = toBeats (pos, ts);

        for (i = getNumPoints(); --i >= 0;)
            if (toBeats (getPointPosition (i), ts) <= beats)
                break;
    }
    else
    {
        auto time = toTime (pos, ts);

        for (i = getNumPoints(); --i >= 0;)
            if (toTime (getPointPosition (i), ts) <= time)
                break;
    }

    addPointAtIndex (++i, pos, value, curve, um);
    return i;
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

EditPosition AutomationCurve::createPosition (double rawPos) const
{
    if (timeBase == TimeBase::time)
        return TimePosition::fromSeconds (rawPos);

    return BeatPosition::fromBeats (rawPos);
}

EditPosition AutomationCurve::getPosition (const juce::ValueTree& pointState) const
{
    assert (pointState.hasType (IDs::POINT));
    return createPosition (static_cast<double> (pointState[IDs::t]));
}

void AutomationCurve::addPointAtIndex (int index, EditPosition pos, float value, float curve, juce::UndoManager* um)
{
    state.addChild (AutomationPoint (pos, value, curve).toValueTree(), index, um);
    checkParenthoodStatus (um);
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

void AutomationCurve::setPointPosition (int index, EditPosition pos, juce::UndoManager* um)
{
    auto& ts = edit.tempoSequence;
    state.getChild (index).setProperty (IDs::t,
                                        timeBase == TimeBase::beats ? toBeats (pos, ts).inBeats()
                                                                    : toTime (pos, ts).inSeconds(),
                                        um);
}

void AutomationCurve::setPointTime  (int index, TimePosition newTime, juce::UndoManager* um)
{
    assert (timeBase == TimeBase::time);
    state.getChild (index).setProperty (IDs::t, newTime.inSeconds(),  um);
}

void AutomationCurve::setPointValue (int index, float newValue, juce::UndoManager* um)          { state.getChild (index).setProperty (IDs::v, newValue, um); }
void AutomationCurve::setCurveValue (int index, float newCurve, juce::UndoManager* um)          { state.getChild (index).setProperty (IDs::c, newCurve, um); }

int AutomationCurve::movePoint (int index, EditPosition newPos, float newValue, std::optional<juce::Range<float>> valueLimits, bool removeInterveningPoints, juce::UndoManager* um)
{
    auto& ts = edit.tempoSequence;

    if (juce::isPositiveAndBelow (index, getNumPoints()))
    {
        if (removeInterveningPoints)
        {
            auto oldPos = getPointPosition (index);
            const bool movingPointBack = less (newPos, oldPos, ts);

            auto t1 = minus (min (newPos, oldPos, ts), 0.00001_td, ts);
            auto t2 = plus (max (newPos, oldPos, ts), 0.00001_td, ts);

            for (int i = getNumPoints(); --i >= 0;)
            {
                auto t = getPointPosition (i);

                if (less (t, t1, ts))
                    break;

                // If points lay at the same time, don't remove them
                if (equals (t, oldPos, ts))
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

                if (less (t, t2, ts) && i != index)
                {
                    if (i < index)
                        --index;

                    removePoint (i, um);
                }
            }
        }

        if (index > 0)
            newPos = max (getPointPosition (index - 1), newPos, ts);
        else
            newPos = max (0_tp, newPos, ts);

        if (index < getNumPoints() - 1)
            newPos = min (newPos, getPointPosition (index + 1), ts);

        if (valueLimits.has_value())
            newValue = valueLimits->clipValue (newValue);

        auto v = state.getChild (index);

        v.setProperty (IDs::t, toUnderlying (newPos), um);
        v.setProperty (IDs::v, newValue, um);
    }
    else
    {
        jassertfalse;
    }

    return index;
}

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

        assert (timeBase == TimeBase::time);
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
        assert (timeBase == TimeBase::time);
        auto t = TimePosition::fromSeconds (static_cast<double> (v.getProperty (IDs::t)));

        if (range.contains (t))
            results.add (AutomationPoint (t, v.getProperty (IDs::v), v.getProperty (IDs::c)));
    }

    return results;
}

juce::Array<AutomationCurve::AutomationPoint> AutomationCurve::getPointsInRegion (EditTimeRange range) const
{
    juce::Array<AutomationPoint> results;
    auto numPoints = getNumPoints();
    auto& ts = edit.tempoSequence;

    for (int i = 0; i < numPoints; ++i)
    {
        auto v = state.getChild (i);
        auto t = getPosition (v);

        if (contains (range, t, ts))
            results.add (AutomationPoint (t, v.getProperty (IDs::v), v.getProperty (IDs::c)));
    }

    return results;
}

void AutomationCurve::removePoints (EditTimeRange range, juce::UndoManager* um)
{
    auto& ts = edit.tempoSequence;

    for (int i = getNumPoints(); --i >= 0;)
    {
        auto t = getPointPosition (i);

        if (less (t, range.getStart(), ts))
            break;

        if (less (t, range.getEnd(), ts))
            removePoint (i, um);
    }
}

void AutomationCurve::removePointsAndCloseGap (AutomatableParameter& param, EditTimeRange range, juce::UndoManager* um)
{
    auto defaultValue = param.getCurrentBaseValue();
    auto valAtStart = getValueAt (range.getStart(), defaultValue);
    auto valAtEnd   = getValueAt (range.getEnd(), defaultValue);
    auto& ts = edit.tempoSequence;

    if (getNumPoints() == 0)
        return;

    for (int i = getNumPoints(); --i >= 0;)
    {
        auto t = getPointPosition (i);

        if (greaterThanOrEqualTo (t, range.getStart(), ts)
            && lessThanOrEqualTo (t, range.getEnd(), ts))
           removePoint (i, um);
    }

    for (int i = 0; i < getNumPoints(); ++i)
    {
        auto t = getPointPosition (i);

        if (greaterThanOrEqualTo (t, range.getEnd(), ts))
            movePoint (i, minus (t, range.getLength(), ts), getPointValue (i), {}, false, um);
    }

    if (getValueAt (range.getStart(), defaultValue) != valAtStart)
        addPoint (range.getStart(), valAtStart, 0.0f, um);

    if (valAtStart != valAtEnd)
        addPoint (range.getStart(), valAtEnd, 0.0f, um);
}

void AutomationCurve::removeRedundantPoints (EditTimeRange range, juce::UndoManager* um)
{
    constexpr auto threshold = 0.0001;
    auto& ts = edit.tempoSequence;

    for (int i = getNumPoints(); --i >= 0;)
    {
        auto t = getPointPosition (i);

        if (! contains (range, t, ts))
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
              && std::abs (toUnderlying (minus (getPointPosition (i + 1), t, ts))) < threshold)
        {
            removePoint (i, um);
            continue;
        }

        // if points to left and right are at same time
        if (i > 0
             && i < getNumPoints() - 1
             && std::abs (toUnderlying (minus (getPointPosition (i - 1), t, ts))) < threshold
             && std::abs (toUnderlying (minus (getPointPosition (i + 1), t, ts))) < threshold)
        {
            removePoint (i, um);
            continue;
        }

        if (getNumPoints() <= 1)
            break;
    }
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
    auto& ts = edit.tempoSequence;

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
              && std::abs (toUnderlying (minus (getPointPosition (i + 1), t, ts))) < threshold)
        {
            removePoint (i, um);
            continue;
        }

        // if points to left and right are at same time
        if (i > 0
             && i < getNumPoints() - 1
             && std::abs (toUnderlying (minus (getPointPosition (i - 1), t, ts))) < threshold
             && std::abs (toUnderlying (minus (getPointPosition (i + 1), t, ts))) < threshold)
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
    auto defaultValue = param.getCurrentBaseValue();
    auto valAtStart = getValueAt (range.getStart(), defaultValue);
    auto valAtEnd   = getValueAt (range.getEnd(), defaultValue);

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

    if (getValueAt (range.getStart(), defaultValue) != valAtStart)
        addPoint (range.getStart(), valAtStart, 0.0f, um);

    if (valAtStart != valAtEnd)
        addPoint (range.getStart(), valAtEnd, 0.0f, um);
}

int AutomationCurve::countPointsInRegion (EditTimeRange range) const
{
    int num = 0;
    auto& ts = edit.tempoSequence;

    for (int i = getNumPoints(); --i >= 0;)
    {
        auto t = getPointPosition (i);

        if (less (t, range.getStart(), ts))
            break;

        if (less (t, range.getEnd(), ts))
            ++num;
    }

    return num;
}

void AutomationCurve::simplify (EditTimeRange editRange, EditDuration minDifference, float minValueDifference, juce::UndoManager* um)
{
    assert (editRange.isBeats() == std::holds_alternative<BeatDuration> (minDifference));

    auto range = toUnderlying (editRange);
    auto minTimeDifference = toUnderlying (minDifference);

    auto minDist = std::sqrt (minTimeDifference * minTimeDifference
                                + minValueDifference * minValueDifference);

    for (int i = 1; i < getNumPoints(); ++i)
    {
        auto time2 = toUnderlying (getPointPosition (i));

        if (range.contains (time2))
        {
            auto time1 = toUnderlying (getPointPosition (i - 1));

            // look for points too close together
            if (std::abs ((time1 - time2)) < minTimeDifference
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
                    double x = time2;
                    double y = getPointValue (i);

                    auto dist = getDistanceFromLine (x, y, time1, getPointValue (i - 1),
                                                     toUnderlying (getPointPosition (i + 1)), getPointValue (i + 1));

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

void AutomationCurve::simplify (TimeRange range, double minTimeDifference, float minValueDifference, juce::UndoManager* um)
{
    simplify (range, TimeDuration::fromSeconds (minTimeDifference), minValueDifference, um);
}

void AutomationCurve::rescaleValues (float factor, EditTimeRange range, juce::Range<float> limits, juce::UndoManager* um)
{
    auto& ts = edit.tempoSequence;

    if (factor != 1.0f)
        for (int i = getNumPoints(); --i >= 0;)
            if (contains (range, getPointPosition (i), ts))
                setPointValue (i, juce::jlimit (limits.getStart(), limits.getEnd(), getPointValue (i) * factor), um);
}

void AutomationCurve::addToValues (float valueDelta, EditTimeRange range, juce::Range<float> limits, juce::UndoManager* um)
{
    auto& ts = edit.tempoSequence;

    if (valueDelta != 0)
        for (int i = getNumPoints(); --i >= 0;)
            if (contains (range, getPointPosition (i), ts))
                setPointValue (i, juce::jlimit (limits.getStart(), limits.getEnd(), getPointValue (i) + valueDelta), um);
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
    auto x1 = toUnderlying (getPointPosition (index));
    auto y1 = getPointValue (index);
    auto x2 = toUnderlying (getPointPosition  (index + 1));
    auto y2 = getPointValue (index + 1);
    auto c  = juce::jlimit (-1.0f, 1.0f, getPointCurve (index) * 2.0f);

    auto [x, y] = core::getBezierPoint (x1, y1, x2, y2, c);

    return { createPosition (x), static_cast<float> (y) };
}

CurvePoint AutomationCurve::getBezierHandle (int index) const noexcept
{
    auto x1 = toUnderlying (getPointPosition (index));
    auto y1 = getPointValue (index);
    auto c  = getPointCurve (index);

    auto x2 = toUnderlying (getPointPosition (index + 1));
    auto y2 = getPointValue (index + 1);

    if (x1 == x2 || y1 == y2)
        return { createPosition ((x1 + x2) / 2), (y1 + y2) / 2 };

    if (c == 0.0f)
        return { createPosition ((x1 + x2) / 2), (y1 + y2) / 2 };

    if (c >= -0.5 && c <= 0.5)
    {
        auto bp = getBezierPoint (index);
        auto x = getBezierXfromT (0.5, x1, toUnderlying (bp.time), x2);
        auto y = getBezierYFromX (x, x1, y1, toUnderlying (bp.time), bp.value, x2, y2);

        return { createPosition (x), static_cast<float> (y) };
    }

    if (c > -1.0 && c < 1.0)
    {
        double x1end, x2end;
        float y1end, y2end;
        getBezierEnds (index, x1end, y1end, x2end, y2end);

        auto bp = getBezierPoint (index);

        auto x = getBezierXfromT (0.5, x1end, toUnderlying (bp.time), x2end);
        auto y = getBezierYFromX (x, x1end, y1end, toUnderlying (bp.time), bp.value, x2end, y2end);
        return { createPosition (x), static_cast<float> (y) };
    }

    double x1end, x2end;
    float y1end, y2end;
    getBezierEnds (index, x1end, y1end, x2end, y2end);
    return { createPosition (x1end), y1end };
}

std::pair<CurvePoint,CurvePoint> AutomationCurve::getBezierEnds (int index) const
{
    auto x1 = 0.0;
    auto x2 = 0.0;
    auto y1 = 0.0f;
    auto y2 = 0.0f;

    getBezierEnds (index, x1, y1, x2, y2);

    return
    {
        { createPosition (x1), y1 },
        { createPosition (x2), y2 }
    };
}

void AutomationCurve::getBezierEnds (int index, double& x1out, float& y1out, double& x2out, float& y2out) const noexcept
{
    auto x1 = toUnderlying (getPointPosition (index));
    auto y1 = getPointValue (index);
    auto c  = getPointCurve (index);

    auto x2 = toUnderlying (getPointPosition (index + 1));
    auto y2 = getPointValue (index + 1);

    auto ends = core::getBezierEnds (x1, static_cast<double> (y1), x2, static_cast<double> (y2), static_cast<double> (c));
    x1out = ends.x1;
    y1out = static_cast<float> (ends.y1);
    x2out = ends.x2;
    y2out = static_cast<float> (ends.y2);
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
              EditTimeRange time, juce::Range<float> valueRange,
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
    curve.simplify (time, TimeDuration::fromSeconds (td), vd, um);
    auto numPointsAfter = curve.getNumPoints();

    return numPointsBefore - numPointsAfter;
}

void mergeCurve (AutomationCurve& dest,
                 TimeRange destRange,
                 const AutomationCurve& source,
                 TimePosition sourceStartTime,
                 AutomatableParameter& param,
                 TimeDuration fadeLength,
                 bool leaveOpenAtStart,
                 bool leaveOpenEnded)
{
    auto um = getUndoManager_p (param);
    auto sourceEndTime = sourceStartTime + destRange.getLength();
    auto defaultValue = param.getCurrentBaseValue();

    auto dstValueAtStart = dest.getValueAt (destRange.getStart(), defaultValue);
    auto dstValueAtEnd   = dest.getValueAt (destRange.getEnd(), defaultValue);

    auto srcValueAtStart = source.getValueAt (sourceStartTime, defaultValue);
    auto srcValueAtEnd = source.getValueAt (sourceEndTime, defaultValue);

    dest.removePointsInRegion (destRange, um);

    if (fadeLength == TimeDuration() && dstValueAtStart != srcValueAtStart)
        dest.addPoint (destRange.getStart(), dstValueAtStart, 0.0f, um);

    if (! leaveOpenAtStart)
        dest.addPoint (destRange.getStart(), srcValueAtStart, 0.0f, um);

    bool pointsInFadeZoneStart = false, pointsInFadeZoneEnd = false;

    for (int i = 0; i < source.getNumPoints(); ++i)
    {
        auto t = source.getPointTime (i) + (destRange.getStart() - sourceStartTime);

        if (t >= destRange.getStart() && t <= destRange.getEnd())
        {
            auto v = source.getPointValue (i);
            auto c = source.getPointCurve (i);

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

            dest.addPoint (t, v, c, um);
        }
    }

    if (fadeLength > 0_td && ! pointsInFadeZoneStart)
        dest.addPoint (destRange.getStart() + fadeLength - 0.0001_td, dstValueAtStart, 0.0f, um);

    if (! leaveOpenEnded)
    {
        if (! pointsInFadeZoneEnd)
            dest.addPoint (destRange.getEnd() - fadeLength, srcValueAtEnd, 0.0f, um);

        dest.addPoint (destRange.getEnd(), dstValueAtEnd, 0.0f, um);
    }
}

float getValueAt (AutomatableParameter& param, EditPosition p)
{
    return param.getCurve().getValueAt (p, param.getCurrentBaseValue());
}

float getValueAt (AutomatableParameter& param, TimePosition p)
{
    return param.getCurve().getValueAt (p, param.getCurrentBaseValue());
}

} // namespace tracktion::inline engine
