/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

struct PluginWindowState  : private juce::Timer
{
    PluginWindowState (Edit&);

    void incRefCount()
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        ++windowShowerCount;
        startTimer (100);
    }

    void decRefCount()
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        --windowShowerCount;
        startTimer (100);
    }

    void showWindowExplicitly()
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        wasExplicitlyClosed = false;
        stopTimer();
        showWindow();
    }

    void closeWindowExplicitly()
    {
        TRACKTION_ASSERT_MESSAGE_THREAD

        if (pluginWindow && pluginWindow->isVisible())
        {
            wasExplicitlyClosed = true;
            deleteWindow();
            stopTimer();
        }
    }

    void hideWindowForShutdown()
    {
        pluginWindow.reset();
        stopTimer();
    }

    void pickDefaultWindowBounds()
    {
        lastWindowBounds = { 100, 100, 600, 500 };

        if (auto focused = juce::Component::getCurrentlyFocusedComponent())
            lastWindowBounds.setPosition (focused->getTopLevelComponent()->getPosition()
                                            + juce::Point<int> (80, 80));
    }

    void pluginClicked (const juce::MouseEvent&);

    Edit& edit;
    Engine& engine;
    std::unique_ptr<juce::Component> pluginWindow;
    int windowShowerCount = 0;
    bool windowLocked;
    bool wasExplicitlyClosed = false;
    juce::Rectangle<int> lastWindowBounds;
    juce::Time windowOpenTime;

private:
    void timerCallback() override;

    void showWindow();

    void deleteWindow()
    {
        pluginWindow.reset();
    }

    JUCE_DECLARE_WEAK_REFERENCEABLE (PluginWindowState)
    JUCE_DECLARE_NON_COPYABLE (PluginWindowState)
};

} // namespace tracktion_engine
