/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

static bool isDialogOpen()
{
    auto& mm = *juce::ModalComponentManager::getInstance();
    if (mm.getNumModalComponents() > 0)
        return true;

    for (int i = juce::TopLevelWindow::getNumTopLevelWindows(); --i >= 0;)
        if (auto w = juce::TopLevelWindow::getTopLevelWindow (i))
            if (dynamic_cast<juce::AlertWindow*> (w))
                return true;

    return false;
}

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

void PluginWindowState::showWindowIfTemporarilyHidden()
{
    if (temporarilyHidden && ! isWindowShowing())
        showWindowExplicitly();
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

void PluginWindowState::hideWindowTemporarily()
{
    if (isWindowShowing())
    {
        closeWindowExplicitly();
        temporarilyHidden = true;
    }
    else
    {
        temporarilyHidden = false;
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

void PluginWindowState::showWindow()
{
    if (isDialogOpen())
        return;

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

std::vector<PluginWindowState*> PluginWindowState::getAllWindows (Edit& ed)
{
    std::vector<PluginWindowState*> windows;

    for (auto p : getAllPlugins (ed, true))
    {
        if (auto w = p->windowState.get())
            windows.push_back (w);

        if (auto rf = dynamic_cast<RackInstance*> (p))
            if (auto rft = rf->type)
                for (auto ws : rft->getWindowStates())
                    windows.push_back (ws);
    }

    return windows;
}

uint32_t PluginWindowState::getNumOpenWindows (Edit& ed)
{
    uint32_t openWindows = 0;

    for (auto w : getAllWindows (ed))
        if (w->isWindowShowing())
            ++openWindows;

    return openWindows;
}

void PluginWindowState::showAllTemporarilyHiddenWindows (Edit& ed)
{
    for (auto w : getAllWindows (ed))
        w->showWindowIfTemporarilyHidden();
}

void PluginWindowState::hideAllWindowsTemporarily (Edit& ed)
{
    for (auto w : getAllWindows (ed))
        w->hideWindowTemporarily();
}

void PluginWindowState::showOrHideAllWindows (Edit& ed)
{
    if (getNumOpenWindows (ed) == 0)
        showAllTemporarilyHiddenWindows (ed);
    else
        hideAllWindowsTemporarily (ed);
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

juce::Point<int> PluginWindowState::choosePositionForPluginWindow()
{
    if (lastWindowBounds)
        return lastWindowBounds->getPosition();

    if (auto focused = juce::Component::getCurrentlyFocusedComponent())
        return focused->getTopLevelComponent()->getPosition() + juce::Point<int> (80, 80);

    for (int i = juce::ComponentPeer::getNumPeers(); --i >= 0;)
        if (auto p = juce::ComponentPeer::getPeer(i))
            if (p->isFocused())
                return p->getBounds().getPosition() + juce::Point<int> (80, 80);

    return juce::Desktop::getInstance().getDisplays()
            .getPrimaryDisplay()->userArea.getRelativePoint (0.2f, 0.2f);
}

}} // namespace tracktion { inline namespace engine
