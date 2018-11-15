/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


PluginWindowState::PluginWindowState (Engine& e)
   : engine (e),
     windowLocked (e.getPluginManager().areGUIsLockedByDefault())
{
}

void PluginWindowState::showWindow()
{
    if (masterConnection == nullptr)
    {
        // Ensure the top middle 8th stays on screen
        bool windowBoundsIsOnScreen = Desktop::getInstance().getDisplays().getRectangleList (true)
                                        .containsRectangle (lastWindowBounds.withTrimmedTop (40)
                                                                            .reduced (lastWindowBounds.getWidth() / 4, 0)
                                                                            .withTrimmedBottom (3 * (lastWindowBounds.getHeight() / 4)));

        if (lastWindowBounds.isEmpty() || ! windowBoundsIsOnScreen)
            pickDefaultWindowBounds();

        WeakReference<Component> oldFocus (Component::getCurrentlyFocusedComponent());
        masterConnection = engine.getUIBehaviour().createPluginWindowConnection (*this);

        if (oldFocus != nullptr)
            oldFocus->grabKeyboardFocus();
    }

    if (masterConnection != nullptr)
    {
        windowOpenTime = Time::getCurrentTime();
        masterConnection->setVisible (true);
        masterConnection->toFront (false);
    }
}

void PluginWindowState::pluginClicked (const MouseEvent& e)
{
    bool isWindowShowing = masterConnection != nullptr && masterConnection->isShowing();

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
        if ((masterConnection == nullptr || ! masterConnection->isVisible())
             && ! (engine.getPluginManager().doubleClickToOpenWindows() || wasExplicitlyClosed))
            showWindow();
    }
    else if (! windowLocked)
    {
        deleteWindow();
    }
}

//==============================================================================
PluginWindowConnection::Master::Master (PluginWindowState& s, Edit& e)
    : state (s), edit (e)
{
}

PluginWindowConnection::Master::Master (PluginWindowState& s, Edit& e,
                                        Component* w, Slave* windowConnectionSlave)
    : state (s), edit (e), pluginWindow (w)
{
    jassert (w != nullptr);

    if (w != nullptr)
    {
        slave = windowConnectionSlave;
        slave->master = this;
    }
}

PluginWindowConnection::Master::~Master()
{
}

bool PluginWindowConnection::Master::isVisible() const
{
    CRASH_TRACER
    if (slave != nullptr)
        return slave->isVisible();

    return true;
}

void PluginWindowConnection::Master::setVisible (bool visible)
{
    CRASH_TRACER
    if (slave != nullptr)
        slave->setVisible (visible);
}

void PluginWindowConnection::Master::toFront (bool setAsForeground)
{
    CRASH_TRACER
    if (slave != nullptr)
        slave->toFront (setAsForeground);
}

void PluginWindowConnection::Master::hide()
{
    CRASH_TRACER
    if (slave != nullptr)
        slave->hide();
}

bool PluginWindowConnection::Master::isShowing() const
{
    CRASH_TRACER
    if (slave != nullptr)
        return slave->isShowing();

    return false;
}

bool PluginWindowConnection::Master::shouldBeShowing() const
{
    return Engine::getInstance().getUIBehaviour().isEditVisibleOnScreen (edit);
}

void PluginWindowConnection::Master::setWindowLocked (bool isLocked)
{
    state.windowLocked = isLocked;
}

bool PluginWindowConnection::Master::isWindowLocked() const
{
    return state.windowLocked;
}

PluginInstanceWrapper* PluginWindowConnection::Master::getWrapper() const
{
    return {};
}

//==============================================================================
PluginWindowConnection::Slave::Slave (Component& b) : pluginWindow (b)
{
}

bool PluginWindowConnection::Slave::isVisible() const                   { return pluginWindow.isVisible(); }
void PluginWindowConnection::Slave::setVisible (bool visible)           { pluginWindow.setVisible (visible); }
void PluginWindowConnection::Slave::toFront (bool setAsForeground)      { pluginWindow.toFront (setAsForeground); }
void PluginWindowConnection::Slave::hide()                              { pluginWindow.setVisible (false); }
bool PluginWindowConnection::Slave::isShowing() const                   { return pluginWindow.isShowing(); }

bool PluginWindowConnection::Slave::shouldBeShowing() const
{
    CRASH_TRACER
    if (master != nullptr)
        return master->shouldBeShowing();

    return true;
}

void PluginWindowConnection::Slave::setWindowLocked (bool isLocked)
{
    CRASH_TRACER
    if (master != nullptr)
        master->setWindowLocked (isLocked);
}

bool PluginWindowConnection::Slave::isWindowLocked() const
{
    CRASH_TRACER
    if (master != nullptr)
        return master->isWindowLocked();

    return false;
}
