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

PluginWindowState::PluginWindowState (Edit& e)
   : edit (e),
     engine (e.engine),
     windowLocked (engine.getPluginManager().areGUIsLockedByDefault())
{
}

void PluginWindowState::showWindow()
{
    if (! pluginWindow)
    {
        // Ensure the top middle 8th stays on screen
        bool windowBoundsIsOnScreen = Desktop::getInstance().getDisplays().getRectangleList (true)
                                        .containsRectangle (lastWindowBounds.withTrimmedTop (40)
                                                                            .reduced (lastWindowBounds.getWidth() / 4, 0)
                                                                            .withTrimmedBottom (3 * (lastWindowBounds.getHeight() / 4)));

        if (lastWindowBounds.isEmpty() || ! windowBoundsIsOnScreen)
            pickDefaultWindowBounds();

        WeakReference<Component> oldFocus (Component::getCurrentlyFocusedComponent());
        pluginWindow = engine.getUIBehaviour().createPluginWindow (*this);

        if (oldFocus != nullptr)
            oldFocus->grabKeyboardFocus();
    }

    if (pluginWindow)
    {
        windowOpenTime = Time::getCurrentTime();
        pluginWindow->setVisible (true);
        pluginWindow->toFront (false);
    }
}

void PluginWindowState::pluginClicked (const MouseEvent& e)
{
    bool isWindowShowing = pluginWindow && pluginWindow->isShowing();

    if (e.getNumberOfClicks() >= 2)
    {
        if ((Time::getCurrentTime() - windowOpenTime).inMilliseconds() < 300)
            return;

        if (isWindowShowing)
            closeWindowExplicitly();
        else
            showWindowExplicitly();
    }
    else if (! (isWindowShowing || engine.getPluginManager().doubleClickToOpenWindows()))
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

}
