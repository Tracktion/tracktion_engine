/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class MouseHoverDetector
{
public:
    MouseHoverDetector (int hoverTimeMillisecs_ = 400)
        : hoverTimeMillisecs (hoverTimeMillisecs_)
    {
        internalTimer.owner = this;
    }

    virtual ~MouseHoverDetector()
    {
        setHoverComponent (nullptr, false);
    }

    void setHoverTimeMillisecs (int newTimeInMillisecs)
    {
        hoverTimeMillisecs = newTimeInMillisecs;
    }

    void setHoverComponent (juce::Component* newSourceComponent,
                            bool wantsEventsForAllNestedChildComponents)
    {
        if (source != newSourceComponent)
        {
            internalTimer.stopTimer();
            hasJustHovered = false;
            wantsHoversForAllNestedChildComponents = wantsEventsForAllNestedChildComponents;

            if (source != nullptr)
                source->removeMouseListener (&internalTimer);

            source = newSourceComponent;

            if (newSourceComponent != nullptr)
                newSourceComponent->addMouseListener (&internalTimer, wantsHoversForAllNestedChildComponents);
        }
    }

    void hoverTimerCallback()
    {
        internalTimer.stopTimer();

        if (source != nullptr)
        {
            auto pos = source->getMouseXYRelative();

            if (source->reallyContains (pos, wantsHoversForAllNestedChildComponents))
            {
                hasJustHovered = true;
                mouseHovered (pos.getX(), pos.getY());
            }
        }
    }

    void checkJustHoveredCallback()
    {
        if (hasJustHovered)
        {
            hasJustHovered = false;
            mouseMovedAfterHover();
        }
    }

protected:
    virtual void mouseHovered (int mouseX, int mouseY) = 0;
    virtual void mouseMovedAfterHover() = 0;

private:
    //==============================================================================
    class HoverDetectorInternal  : public juce::MouseListener,
                                   public juce::Timer
    {
    public:
        MouseHoverDetector* owner;
        juce::Point<int> lastPos;

        void timerCallback() override
        {
            owner->hoverTimerCallback();
        }

        void mouseEnter (const juce::MouseEvent&) override
        {
            stopTimer();
            owner->checkJustHoveredCallback();
        }

        void mouseExit (const juce::MouseEvent&) override
        {
            stopTimer();
            owner->checkJustHoveredCallback();
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            stopTimer();
            owner->checkJustHoveredCallback();
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            stopTimer();
            owner->checkJustHoveredCallback();
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            if (lastPos != e.getPosition()) // to avoid fake mouse-moves setting it off
            {
                lastPos = e.getPosition();

                if (owner->source != nullptr)
                    startTimer (owner->hoverTimeMillisecs);

                owner->checkJustHoveredCallback();
            }
        }

        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override
        {
            stopTimer();
            owner->checkJustHoveredCallback();
        }
    };

    HoverDetectorInternal internalTimer;
    juce::Component* source = nullptr;
    int hoverTimeMillisecs;
    bool hasJustHovered = false;
    bool wantsHoversForAllNestedChildComponents = false;

    JUCE_DECLARE_NON_COPYABLE (MouseHoverDetector)
};

} // namespace tracktion_engine
