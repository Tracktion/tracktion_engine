/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


CurveEditorPoint::~CurveEditorPoint()
{
    notifyListenersOfDeletion();
}

static CurveEditor* getPointIndexes (Array<int>& set, const SelectableList& items)
{
    set.ensureStorageAllocated (items.size());
    CurveEditor* ed = nullptr;

    for (auto point : items.getItemsOfType<CurveEditorPoint>())
    {
        ed = point->editor;
        set.add (point->index);
    }

    std::sort (set.begin(), set.end());
    return ed;
}

bool CurveEditorPoint::arePointsOnSameCurve (const SelectableList& items)
{
    CurveEditor* ed = nullptr;

    for (auto point : items.getItemsOfType<CurveEditorPoint>())
    {
        if (ed == nullptr)
            ed = point->editor;
        else if (ed != point->editor)
            return false;
    }

    return true;
}

bool CurveEditorPoint::arePointsConsecutive (const SelectableList& items)
{
    Array<int> set;
    getPointIndexes (set, items);

    for (int i = 0; i < set.size() - 1; ++i)
        if (set[i + 1] > set[i] + 1)
            return false;

    return arePointsOnSameCurve (items);
}

EditTimeRange CurveEditorPoint::getPointTimeRange (const SelectableList& items)
{
    Array<int> set;

    if (auto ed = getPointIndexes (set, items))
        return { ed->getPointTime (set.getFirst()),
                 ed->getPointTime (set.getLast()) };

    return {};
}

void CurveEditorPoint::selectionStatusChanged (bool)
{
    if (editor != nullptr)
    {
        editor->updateLineThickness();
        editor->repaint();
    }
}

//==============================================================================
CurveEditor::CurveEditor (Edit& e, SelectionManager& sm)
    : MouseHoverDetector (200),
      edit (e),
      undoManager (edit.getUndoManager()),
      selectionManager (sm)
{
    setAlwaysOnTop (true);
    setHoverComponent (this, false);
    selectionManager.addChangeListener (this);
}

CurveEditor::~CurveEditor()
{
    selectionManager.removeChangeListener (this);

    for (auto p : selectionManager.getItemsOfType<CurveEditorPoint>())
        if (p->editor == this)
            selectionManager.deselect (p);

    deleteAllChildren();
}

void CurveEditor::updateLineThickness()
{
    auto newthickness = (isMouseOverOrDragging() || isCurveSelected || areAnyPointsSelected())
                          ? 2.0f : 1.0f;

    if (lineThickness != newthickness)
    {
        lineThickness = newthickness;
        repaint();
    }
}

void CurveEditor::paint (juce::Graphics& g)
{
    CRASH_TRACER

    if ((rightTime - leftTime) == 0.0)
        return;

    const bool isOver = isMouseOverOrDragging();
    auto curveColour = getCurrentLineColour();
    auto backgroundColour = getBackgroundColour();

    // draw the name of the curve
    if (isOver || isCurveSelected)
    {
        g.setColour (getCurveNameTextBackgroundColour());
        auto text = getCurveName();
        g.setFont (13.0f);
        auto tw = g.getCurrentFont().getStringWidth (text);
        auto tx = getCurveNameOffset() - (tw + 8);
        g.fillRect (tx, 0, tw + 6, 16);

        g.setColour (getDefaultLineColour());
        g.drawText (text, tx, 0, tw + 6, 16,
                    Justification::centred, true);
    }

    // draw the line to the first point, or all the way across if there are no points
    const int start = jmax (0, nextIndexAfter (leftTime) - 1);
    const int numPoints = getNumPoints();

    auto clipBounds = g.getClipBounds();

    {
        auto lastY = valueToY (getValueAt (leftTime));

        Path curvePath;
        curvePath.startNewSubPath (jmax (0.0f, timeToX (0)), lastY);
        curvePath.preallocateSpace (numPoints * 5 + 1);

        if (numPoints > 0)
        {
            for (int index = start; index < numPoints - 1; ++index)
            {
                if (index == start)
                    curvePath.lineTo (getPosition (index));

                auto p2 = getPosition (index + 1);
                auto c = getPointCurve (index);

                if (c == 0)
                {
                    curvePath.lineTo (p2);
                }
                else
                {
                    auto bp = getPosition (getBezierPoint (index));

                    if (c >= -0.5 && c <= 0.5)
                    {
                        curvePath.quadraticTo (bp, p2);
                    }
                    else
                    {
                        double lineX1, lineX2;
                        float lineY1, lineY2;
                        getBezierEnds (index, lineX1, lineY1, lineX2, lineY2);

                        curvePath.lineTo (getPosition ({ lineX1, lineY1 }));
                        curvePath.quadraticTo (bp, getPosition ({ lineX2, lineY2 }));
                        curvePath.lineTo (p2);
                    }
                }

                lastY = p2.y;

                if (p2.x > clipBounds.getRight())
                    break;
            }
        }

        curvePath.lineTo ((float) getWidth(), lastY);

        g.setColour (getCurrentLineColour());
        g.strokePath (curvePath, PathStrokeType (lineThickness));
    }

    // draw the points along the line - the points, the add point and the curve point
    const bool anySelected = areAnyPointsSelected();

    if (isOver || isCurveSelected || anySelected)
    {
        RectangleList<float> rects, selectedRects, fills;
        rects.ensureStorageAllocated (numPoints);

        if (anySelected)
            fills.ensureStorageAllocated (numPoints);

        // draw the white points
        for (int i = start; i < numPoints; ++i)
        {
            auto pos = getPosition (i);

            juce::Rectangle<float> r (pos.x - pointRadius,
                                      pos.y - pointRadius,
                                      pointRadius * 2,
                                      pointRadius * 2);

            if (r.getX() > clipBounds.getRight())
                break;

            const bool isSelected = isPointSelected (i);

            if (isSelected)
                selectedRects.addWithoutMerging (r);
            else
                rects.addWithoutMerging (r);

            if (! isSelected && i != pointUnderMouse)
                fills.addWithoutMerging (r.reduced (1.0f));
        }

        g.setColour (getPointOutlineColour());
        g.fillRectList (rects);

        g.setColour (backgroundColour);
        g.fillRectList (fills);

        g.setColour (getSelectedLineColour());
        g.fillRectList (selectedRects);

        // draw the curve points
        for (int i = start; i < numPoints - 1; ++i)
        {
            auto pos = getPosition (getBezierHandle (i));

            juce::Rectangle<float> r (pos.x - pointRadius,
                                      pos.y - pointRadius,
                                      pointRadius * 2,
                                      pointRadius * 2);

            if (r.getX() > clipBounds.getRight())
                break;

            g.setColour (curveColour);
            g.fillEllipse (r);

            if (i != curveUnderMouse && ! isPointSelected (i))
            {
                g.setColour (backgroundColour);
                g.fillEllipse (r.reduced (1.0f));
            }
        }
    }
}

bool CurveEditor::hitTest (int x, int y)
{
    auto py1 = valueToY (getValueAt (xToTime (x - 3.0f)));
    auto py2 = valueToY (getValueAt (xToTime (x + 3.0f)));

    if (y > jmin (py1, py2) - 4.0f && y < jmax (py1, py2) + 4.0f)
        return true;

    for (int i = firstIndexOnScreen; i < getNumPoints(); ++i)
    {
        auto t = getPointTime (i);

        if (t >= rightTime)
            break;

        auto px = timeToX (t);
        auto py = valueToY (getPointValue (i));

        if (std::abs (x - px) < pointRadius
             && std::abs (y - py) < pointRadius + 2)
            return true;
    }

    return false;
}

void CurveEditor::mouseDown (const MouseEvent& e)
{
    CRASH_TRACER

    if (getItem() == nullptr)
        return;

    if (ModifierKeys::getCurrentModifiers().isAltDown())
    {
        mouseDoubleClick(e);
        return;
    }

    dragged = false;
    pointBeingMoved = pointUnderMouse;

    if (pointBeingMoved >= 0)
        movingAllPoints = e.mods.isShiftDown() && e.mods.isCommandDown();
    else
        movingAllPoints = false;

    if (pointUnderMouse != -1)
    {
        if (e.mods.isShiftDown() && areAnyPointsSelected())
        {
            int selPoint = 0;

            if (auto firstPoint = selectionManager.getFirstItemOfType<CurveEditorPoint>())
                selPoint = firstPoint->index;

            int start = jmin (selPoint, pointUnderMouse);
            int end   = jmax (selPoint, pointUnderMouse);

            selectionManager.deselectAll();

            for (int i = start; i <= end; ++i)
                selectPoint (i, true);
        }
        else if (e.mods.isCommandDown())
        {
            if (isPointSelected (pointUnderMouse))
                selectionManager.deselect (getSelectedPoint (pointUnderMouse));
            else
                selectionManager.addToSelection (createPoint (pointUnderMouse));
        }
        else
        {
            selectionManager.deselectAll();
            selectPoint (pointUnderMouse, false);
        }
    }
    else if (curveUnderMouse != -1)
    {
        selectionManager.deselectAll();
        selectPoint (curveUnderMouse, false);
    }
    else
    {
        selectionManager.select (getItem(), e.mods.isShiftDown());
    }
}

void CurveEditor::selectPoint (int pointIdx, bool addToSelection)
{
    if (pointIdx >= 0 && pointIdx < getNumPoints() && ! isPointSelected (pointIdx))
        selectionManager.select (createPoint (pointIdx), addToSelection);
}

void CurveEditor::mouseDrag (const MouseEvent& e)
{
    yieldGUIThread();

    if (getItem() == nullptr)
        return;

    // test if we are just starting to drag and the mouse has moved
    if (! dragged)
    {
        if (e.getDistanceFromDragStart() <= 1)
            return;

        CRASH_TRACER
        dragged = true;

        if (realTimeDrag)
            undoManager.beginNewTransaction();
        else
            nonRealTimeDragStart();

        mouseDownTime  = xToTime (e.mouseDownPosition.x);
        mouseDownValue = yToValue (e.mouseDownPosition.y);
        mouseDownCurve = curveUnderMouse >= 0 ? getPointCurve (curveUnderMouse) : 0.0f;

        if (lineUnderMouse >= 0)
        {
            // add new endpoints to the line
            if (e.mods.isCommandDown())
            {
                auto t1 = getPointTime (lineUnderMouse);
                auto t2 = getPointTime (lineUnderMouse + 1);
                auto v1 = getPointValue (lineUnderMouse);
                auto v2 = getPointValue (lineUnderMouse + 1);
                auto c1 = getPointCurve (lineUnderMouse);
                auto c2 = getPointCurve (lineUnderMouse + 1);

                addPoint (t1, v1, c1);
                addPoint (t2, v2, c2);

                lineUnderMouse++;

                selectionManager.deselectAll();
                selectPoint (lineUnderMouse, true);
                selectPoint (lineUnderMouse + 1, true);
            }

            point1 = getPointValue (lineUnderMouse);
            point2 = getPointValue (lineUnderMouse + 1);
        }
        else if (curveUnderMouse == -1 && pointUnderMouse == -1 && getNumPoints() <= 1)
        {
            if (getNumPoints() == 1)
                point1 = getPointValue (0);
            else
                point1 = getValueAt (0);
        }
        else if (getNumPoints())
        {
            point1 = getPointValue (0);
            point2 = getPointValue (getNumPoints() - 1);
        }
    }

    if (pointBeingMoved >= 0 && realTimeDrag)
        undoManager.undoCurrentTransactionOnly();

    // bend a line or move a line
    if (curveUnderMouse >= 0)
    {
        CRASH_TRACER

        // bend a curve
        auto mult = getPointValue (curveUnderMouse) < getPointValue (curveUnderMouse + 1) ? 1 : -1;
        auto c = jlimit (-1.0f, 1.0f, mult * e.getDistanceFromDragStartY() / 75.0f + mouseDownCurve);
        curvePoint (curveUnderMouse, c);
        showBubbleForPointUnderMouse();
    }
    // move a point around
    else if (pointBeingMoved >= 0)
    {
        CRASH_TRACER
        auto t = snapTime (xToTime (e.position.x), e.mods);

        if (movingAllPoints)
        {
            auto delta = t - mouseDownTime;

            if (delta > 0)
            {
                for (int i = getNumPoints(); --i >= pointBeingMoved;)
                    movePoint (i, getPointTime (i) + delta, getPointValue (i), false);
            }
            else
            {
                bool firstPoint = true;

                for (int i = pointBeingMoved; i < getNumPoints(); ++i)
                {
                    auto newIndex = movePoint (i,
                                               getPointTime (i) + delta,
                                               getPointValue (i),
                                               i == pointBeingMoved
                                                 && e.mods.testFlags (ModifierKeys::commandModifier | ModifierKeys::ctrlModifier));

                    if (firstPoint)
                    {
                        firstPoint = false;

                        i -= pointBeingMoved - newIndex;
                        pointUnderMouse = newIndex;
                    }
                }
            }
        }
        else
        {
            auto newTime = t;
            auto newValue = yToValue (e.position.y);

            if (ModifierKeys::getCurrentModifiers().isShiftDown())
            {
                if (std::abs (e.getDistanceFromDragStartX()) > std::abs (e.getDistanceFromDragStartY()))
                    newValue = getPointValue(pointBeingMoved);
                else
                    newTime  = getPointTime(pointBeingMoved);
            }

            pointUnderMouse = movePoint (pointBeingMoved, newTime, newValue,
                                         e.mods.testFlags (ModifierKeys::commandModifier | ModifierKeys::ctrlModifier));
        }

        showBubbleForPointUnderMouse();
    }
    else if (lineUnderMouse >= 0)
    {
        // move a line up and down
        CRASH_TRACER
        auto offset = mouseDownValue - yToValue (e.position.y);

        movePoint (lineUnderMouse, getPointTime(lineUnderMouse), point1 - offset, false);
        movePoint (lineUnderMouse + 1, getPointTime(lineUnderMouse + 1), point2 - offset, false);
    }
    else
    {
        CRASH_TRACER
        auto offset = mouseDownValue - yToValue (e.position.y);

        if (getNumPoints() <= 0)
            setValueWhenNoPoints (point1 - offset);
        else if (getNumPoints() == 1 || mouseDownTime < getPointTime(0))
            movePoint (0, getPointTime(0), point1 - offset, false);
        else if (mouseDownTime > getPointTime (getNumPoints() - 1))
            movePoint (getNumPoints() - 1, getPointTime(getNumPoints() - 1), point2 - offset, false);
    }

    CRASH_TRACER
    updateLineThickness();
    repaint();

    if (areAnyPointsSelected())
        for (auto c : selectionManager.getItemsOfType<CurveEditorPoint>())
            c->changed();
}

void CurveEditor::mouseUp (const MouseEvent& e)
{
    if (getItem() == nullptr)
        return;

    if (dragged)
    {
        if (pointBeingMoved >= 0)
        {
            auto newPoint = getPosition (pointUnderMouse);

            for (int i = jmax (0, pointBeingMoved - 1); i < pointBeingMoved + 2; ++i)
            {
                auto pos = getPosition (i);

                if (i != pointBeingMoved && pos.getDistanceFrom (newPoint) < 7.0f)
                {
                    // too near to an existing point - so remove it..
                    removePoint (pointBeingMoved);
                    break;
                }
            }
        }

        if (realTimeDrag)
            undoManager.beginNewTransaction();
        else
            nonRealTimeDragEnd();
    }

    dragged = false;
    pointBeingMoved = -1;

    updatePointUnderMouse (e.position);
    updateLineThickness();
    repaint();

    hideBubble();
}

void CurveEditor::mouseDoubleClick (const MouseEvent& e)
{
    if (getItem() == nullptr)
        return;

    if (pointUnderMouse >= 0 && ! dragged)
    {
        if (auto p = getSelectedPoint (pointUnderMouse))
            p->deselect();

        removePoint (pointUnderMouse);
    }
    else if (pointUnderMouse < 0)
    {
        auto t = snapTime (xToTime (e.position.x), e.mods);
        auto value = getValueAt (t);

        if (juce::Point<float> (e.position.x, valueToY (value)).getDistanceFrom (e.position) < 8.0f)
        {
            // add a new point on the line if we've clicked near enough
            undoManager.beginNewTransaction();

            auto pnt = addPoint (t, value, defaultCurve);

            if (pnt >= 0)
                selectPoint (pnt, false);

            undoManager.beginNewTransaction();
        }
    }
}

void CurveEditor::mouseMove (const MouseEvent& e)
{
    if (getItem() == nullptr)
        return;

    updatePointUnderMouse (e.position);
    updateLineThickness();
}

void CurveEditor::mouseEnter (const MouseEvent& e)
{
    mouseMove (e);
    updateLineThickness();
}

void CurveEditor::mouseExit (const MouseEvent& e)
{
    mouseMove (e);
    updateLineThickness();
}

void CurveEditor::mouseHovered (int, int)
{
    showBubbleForPointUnderMouse();
}

void CurveEditor::mouseMovedAfterHover()
{
    hideBubble();
}

MouseCursor CurveEditor::getMouseCursor()
{
    if (pointUnderMouse != -1 || curveUnderMouse != -1 || ModifierKeys::getCurrentModifiers().isAltDown())
        return MouseCursor::NormalCursor;

    return MouseCursor::UpDownResizeCursor;
}

void CurveEditor::updatePointUnderMouse (juce::Point<float> pos)
{
    if (getItem() == nullptr || pointBeingMoved >= 0)
        return;

    pos.x = jmax (timeToX (0), pos.x);

    const auto captureRadius = pointRadius * pointRadius;
    int point = -1;
    int curvePoint = -1;

    for (int i = firstIndexOnScreen; i < getNumPoints(); ++i)
    {
        auto t = getPointTime (i);

        if (t >= rightTime)
            break;

        // check if there is a point under the mouse
        if (getPosition ({ t, getPointValue (i) }).getDistanceSquaredFrom (pos) < captureRadius)
        {
            point = i;
            break;
        }

        // check if there is a curve dragger under the mouse
        if (i < getNumPoints() - 1)
            if (getPosition (getBezierHandle (i)).getDistanceSquaredFrom (pos) < captureRadius)
                curvePoint = i;
   }

    if (pointUnderMouse != point)
    {
        pointUnderMouse = point;
        repaint();
    }

    if (! dragged)
    {
        if (pointUnderMouse != -1)
            curvePoint = -1;

        if (curveUnderMouse != curvePoint)
        {
            curveUnderMouse = curvePoint;
            repaint();
        }

        int newLineUnderMouse = -1;

        if (pointUnderMouse == -1 && curveUnderMouse == -1)
        {
            auto numPoints = getNumPoints();

            for (int i = 0; i < numPoints - 1; ++i)
            {
                auto t = xToTime (pos.x);

                if (t > getPointTime(i) && t < getPointTime (i + 1))
                    newLineUnderMouse = i;
            }
        }

        if (newLineUnderMouse != lineUnderMouse)
        {
            lineUnderMouse = newLineUnderMouse;
            repaint();
        }
    }
}

void CurveEditor::setTimes (double l, double r)
{
    leftTime = l;
    rightTime = r;
}

float CurveEditor::timeToX (double t) const
{
    return (float) (getWidth() * (t - leftTime) / (rightTime - leftTime));
}

double CurveEditor::xToTime (double x) const
{
    return jmax (0.0, leftTime + (x * (rightTime - leftTime)) / getWidth());
}

float CurveEditor::valueToY (float val) const
{
    return 1.0f + (1.0f - (val - parameterMinValue) / parameterRange) * (getHeight() - 2);
}

float CurveEditor::yToValue (double y) const
{
    return (float) ((1.0 - (y - 1) / (getHeight() - 2)) * parameterRange + parameterMinValue);
}

juce::Point<float> CurveEditor::getPosition (CurvePoint p) const
{
    return { timeToX (p.time), valueToY (p.value) };
}

juce::Point<float> CurveEditor::getPosition (int index)
{
    return getPosition ({ getPointTime (index), getPointValue (index) });
}

void CurveEditor::selectableObjectChanged (Selectable*)
{
    updateLineThickness();
}

void CurveEditor::selectableObjectAboutToBeDeleted (Selectable*)
{
    updateLineThickness();
}

void CurveEditor::changeListenerCallback (ChangeBroadcaster* cb)
{
    if (cb == &selectionManager)
    {
        const bool selectedNow = selectionManager.isSelected (getItem());

        if (isCurveSelected != selectedNow)
        {
            isCurveSelected = selectedNow;
            repaint();
        }
    }
    else
    {
        updatePointUnderMouse (getMouseXYRelative().toFloat());
        repaint();
    }

    updateLineThickness();
}

Edit& CurveEditor::getEdit() const
{
    return edit;
}

double CurveEditor::snapTime (double time, juce::ModifierKeys)
{
    return time;
}

bool CurveEditor::isPointSelected (int idx)
{
    for (auto p : selectionManager.getItemsOfType<CurveEditorPoint>())
        if (p->editor == this && p->index == idx)
            return true;

    return false;
}

bool CurveEditor::areAnyPointsSelected()
{
    for (auto p : selectionManager.getItemsOfType<CurveEditorPoint>())
        if (p->editor == this)
            return true;

    return false;
}

CurveEditorPoint* CurveEditor::getSelectedPoint (int idx)
{
    for (auto p : selectionManager.getItemsOfType<CurveEditorPoint>())
        if (p->editor == this && p->index == idx)
            return p;

    return {};
}
