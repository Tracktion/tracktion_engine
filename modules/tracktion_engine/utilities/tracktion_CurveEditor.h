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

class CurveEditor;

// Vertical components will ignore mouse events if they occur over a comp which
// inherits from this class..
struct TransparentToMouseInTrackItemComp {};


struct CurvePoint
{
    CurvePoint() = default;
    CurvePoint (EditTime t, float val) : time (t), value (val) {}

    EditTime time;
    float value = 0;
};

//==============================================================================
class CurveEditorPoint  : public Selectable
{
public:
    CurveEditorPoint() noexcept {}
    CurveEditorPoint (int i, CurveEditor* ed)  : index (i), editor (ed) {}
    ~CurveEditorPoint() override;

    void selectionStatusChanged (bool isNowSelected) override;

    static bool arePointsConsecutive (const SelectableList&);
    static bool arePointsOnSameCurve (const SelectableList&);

    static TimeRange getPointTimeRange (const SelectableList&);

    int index = 0;
    juce::Component::SafePointer<CurveEditor> editor;
};


//==============================================================================
class CurveEditor  : public juce::Component,
                     public SelectableListener,
                     public juce::ChangeListener,
                     public juce::TooltipClient,
                     public MouseHoverDetector,
                     public TransparentToMouseInTrackItemComp
{
public:
    CurveEditor (Edit&, SelectionManager&);
    ~CurveEditor() override;

    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseHovered (int mouseX, int mouseY) override;
    void mouseMovedAfterHover() override;

    juce::MouseCursor getMouseCursor() override;

    void setTimes (TimePosition leftTime, TimePosition rightTime);

    float timeToX (TimePosition) const;
    TimePosition xToTime (double x) const;
    float valueToY (float val) const;
    float yToValue (double y) const;
    juce::Point<float> getPosition (CurvePoint) const;
    juce::Point<float> getPosition (int index);

    void selectableObjectChanged (Selectable*) override;
    void selectableObjectAboutToBeDeleted (Selectable*) override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void updateLineThickness();

    Edit& getEdit() const;
    virtual Track* getTrack()      { return {}; }

    virtual TimePosition snapTime (TimePosition, juce::ModifierKeys);

    virtual float getValueAt (TimePosition) = 0;
    virtual TimePosition getPointTime (int idx) = 0;
    virtual float getPointValue (int idx) = 0;
    virtual float getPointCurve (int idx) = 0;
    virtual void removePoint (int index) = 0;
    virtual int addPoint (TimePosition time, float value, float curve) = 0;
    virtual int getNumPoints() = 0;
    virtual CurvePoint getBezierHandle (int idx) = 0;
    virtual CurvePoint getBezierPoint (int idx) = 0;
    virtual int nextIndexAfter (TimePosition) = 0;
    virtual void getBezierEnds (int index, double& x1out, float& y1out, double& x2out, float& y2out) = 0;
    virtual int movePoint (int index, TimePosition newTime, float newValue, bool removeInterveningPoints) = 0;
    virtual void setValueWhenNoPoints (float value) = 0;
    virtual CurveEditorPoint* createPoint (int idx) = 0;
    virtual int curvePoint (int index, float newCurve) = 0;
    virtual juce::String getCurveName() = 0;
    virtual int getCurveNameOffset() = 0;
    virtual Selectable* getItem() = 0;
    virtual bool isShowingCurve() const = 0;
    virtual void updateFromTrack() = 0;

    virtual juce::Colour getCurrentLineColour() const = 0;
    virtual juce::Colour getDefaultLineColour() const = 0;
    virtual juce::Colour getSelectedLineColour() const = 0;
    virtual juce::Colour getBackgroundColour() const = 0;
    virtual juce::Colour getCurveNameTextBackgroundColour() const = 0;
    virtual juce::Colour getPointOutlineColour() const = 0;

    void selectPoint (int pointIdx, bool addToSelection);

protected:
    void updatePointUnderMouse (juce::Point<float>);
    virtual void showBubbleForPointUnderMouse() = 0;
    virtual void hideBubble() = 0;

    virtual void nonRealTimeDragStart() {}
    virtual void nonRealTimeDragEnd()   {}

    bool isPointSelected (int idx);
    bool areAnyPointsSelected();
    CurveEditorPoint* getSelectedPoint (int);

    static constexpr const float pointRadius = 5.0f;

    Edit& edit;
    juce::UndoManager& undoManager;
    SelectionManager& selectionManager;
    float parameterMinValue = 0, parameterRange = 1.0f;
    TimePosition leftTime, rightTime;
    int firstIndexOnScreen = 0;
    int pointUnderMouse = -1, pointBeingMoved = -1;
    int curveUnderMouse = -1, lineUnderMouse = -1;
    bool dragged = false, movingAllPoints = false;
    TimePosition mouseDownTime;
    float mouseDownValue = 0;
    bool isCurveSelected = false;
    float mouseDownCurve = 0;
    float point1 = 0, point2 = 0;
    bool realTimeDrag = true;
    float defaultCurve = 0;
    float lineThickness = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveEditor)
};

}} // namespace tracktion { inline namespace engine
