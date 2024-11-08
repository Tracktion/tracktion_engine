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

ParameterChangeHandler::ParameterChangeHandler (Edit& e) : edit (e)
{
}

void ParameterChangeHandler::addListener (Listener& l)
{
    std::unique_lock sl (listenerListLock);

    if (std::find (listeners.begin(), listeners.end(), std::addressof (l)) == listeners.end())
        listeners.push_back (std::addressof (l));
}

void ParameterChangeHandler::removeListener (Listener& l)
{
    std::unique_lock sl (listenerListLock);

    if (auto i = std::find (listeners.begin(), listeners.end(), std::addressof (l)); i != listeners.end())
        listeners.erase (i);
}

bool ParameterChangeHandler::isEventPending() const noexcept
{
    return isParameterPending() || isActionFunctionPending();
}

//==============================================================================
void ParameterChangeHandler::parameterChanged (AutomatableParameter& parameter, bool fromAutomation)
{
    {
        std::shared_lock sl (listenerListLock);

        for (auto& l : listeners)
            l->pluginParameterChanged (parameter, fromAutomation);
    }

    if (enabled
         && ! fromAutomation
         && (edit.engine.getMidiLearnState().isActive() || parameterLearnActive))
    {
        std::scoped_lock sl (eventLock);
        pendingActionId = -1;
        pendingParam = &parameter;
        sendChangeMessage();
    }
}

bool ParameterChangeHandler::isParameterPending() const noexcept
{
    std::scoped_lock sl (eventLock);
    return pendingParam != nullptr;
}

AutomatableParameter::Ptr ParameterChangeHandler::getPendingParam (bool consumeEvent) noexcept
{
    if (consumeEvent)
    {
        std::scoped_lock sl (eventLock);
        return std::move (pendingParam);
    }

    std::scoped_lock sl (eventLock);
    return pendingParam;
}

//==============================================================================
void ParameterChangeHandler::actionFunctionTriggered (int externalControllerID)
{
    std::scoped_lock sl (eventLock);
    pendingParam = nullptr;
    pendingActionId = externalControllerID;
    sendChangeMessage();
}

bool ParameterChangeHandler::isActionFunctionPending() const noexcept
{
    std::scoped_lock sl (eventLock);
    return pendingActionId != -1;
}

int ParameterChangeHandler::getPendingActionFunctionId (bool consumeEvent) noexcept
{
    std::scoped_lock sl (eventLock);
    const juce::ScopedValueSetter<int> actionResetter (pendingActionId, pendingActionId,
                                                       consumeEvent ? -1 : pendingActionId);

    if (consumeEvent)
        sendChangeMessage();

    return pendingActionId;
}

}} // namespace tracktion { inline namespace engine
