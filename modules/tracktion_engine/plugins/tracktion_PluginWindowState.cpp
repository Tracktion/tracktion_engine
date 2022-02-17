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

PluginWindowState::PluginWindowState (Edit& e)
   : edit (e),
     engine (e.engine),
     windowLocked (engine.getPluginManager().areGUIsLockedByDefault())
{
}

void PluginWindowState::deleteWindow()
{
    pluginWindow.reset();
}

void PluginWindowState::incRefCount()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    ++windowShowerCount;
    startTimer (100);
}

void PluginWindowState::decRefCount()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    --windowShowerCount;
    startTimer (100);
}

void PluginWindowState::showWindowExplicitly()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    wasExplicitlyClosed = false;
    stopTimer();
    showWindow();
}

void PluginWindowState::closeWindowExplicitly()
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (pluginWindow && pluginWindow->isVisible())
    {
        wasExplicitlyClosed = true;
        deleteWindow();
        stopTimer();
    }
}

bool PluginWindowState::isWindowShowing() const
{
    return pluginWindow != nullptr && pluginWindow->isVisible();
}

void PluginWindowState::recreateWindowIfShowing()
{
    deleteWindow();
    startTimer (100);
}

void PluginWindowState::hideWindowForShutdown()
{
    deleteWindow();
    stopTimer();
}

void PluginWindowState::pickDefaultWindowBounds()
{
    lastWindowBounds = { 100, 100, 600, 500 };

    if (auto focused = juce::Component::getCurrentlyFocusedComponent())
        lastWindowBounds.setPosition (focused->getTopLevelComponent()->getPosition()
                                        + juce::Point<int> (80, 80));
}

void PluginWindowState::showWindow()
{
    if (! pluginWindow)
    {
        // Ensure at least 40px of the window is on screen
        const auto displayRects = []
        {
            juce::RectangleList<int> trimmedDisplays;
            
            for (auto rect : juce::Desktop::getInstance().getDisplays().getRectangleList (true))
                trimmedDisplays.addWithoutMerging (rect.withTrimmedLeft (100).withTrimmedRight (100).withTrimmedBottom (100));
            
            return trimmedDisplays;
        }();
        
        const bool windowBoundsIsOnScreen = displayRects.intersectsRectangle (lastWindowBounds);

        if (lastWindowBounds.isEmpty() || ! windowBoundsIsOnScreen)
            pickDefaultWindowBounds();

        juce::WeakReference<juce::Component> oldFocus (juce::Component::getCurrentlyFocusedComponent());
        pluginWindow = engine.getUIBehaviour().createPluginWindow (*this);

        if (oldFocus != nullptr)
            oldFocus->grabKeyboardFocus();
    }

    if (pluginWindow)
    {
        windowOpenTime = juce::Time::getCurrentTime();
        pluginWindow->setVisible (true);
        pluginWindow->toFront (false);
    }
}

void PluginWindowState::pluginClicked (const juce::MouseEvent& e)
{
    bool isShowing = isWindowShowing();

    if (e.getNumberOfClicks() >= 2)
    {
        if ((juce::Time::getCurrentTime() - windowOpenTime).inMilliseconds() < 300)
            return;

        if (isShowing)
            closeWindowExplicitly();
        else
            showWindowExplicitly();
    }
    else if (! (isShowing || engine.getPluginManager().doubleClickToOpenWindows()))
    {
        showWindowExplicitly();
    }
}

void PluginWindowState::timerCallback()
{
    stopTimer();

    if (windowShowerCount > 0)
    {
        if ((pluginWindow == nullptr || ! pluginWindow->isVisible())
             && ! (engine.getPluginManager().doubleClickToOpenWindows() || wasExplicitlyClosed))
            showWindow();
    }
    else if (! windowLocked)
    {
        deleteWindow();
    }
}

}} // namespace tracktion { inline namespace engine
