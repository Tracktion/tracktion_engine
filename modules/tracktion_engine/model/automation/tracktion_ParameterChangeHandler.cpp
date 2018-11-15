/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


ParameterChangeHandler::ParameterChangeHandler (Edit& e) : edit (e)
{
    juce::ignoreUnused (edit);
}

bool ParameterChangeHandler::isEventPending() const noexcept
{
    return isParameterPending() || isActionFunctionPending();
}

//==============================================================================
void ParameterChangeHandler::parameterChanged (AutomatableParameter& parameter, bool fromAutomation)
{
   #if TRACKTION_ENABLE_AUTOMAP && TRACKTION_ENABLE_CONTROL_SURFACES
    if (edit.shouldPlay())
        if (auto na = edit.engine.getExternalControllerManager().getAutomap())
            na->paramChanged (&parameter);
   #endif

    if (enabled
         && ! fromAutomation
         && (edit.engine.getMidiLearnState().isActive() || parameterLearnActive))
    {
        const ScopedLock sl (eventLock);
        pendingActionId = -1;
        pendingParam = &parameter;
        sendChangeMessage();
    }
}

bool ParameterChangeHandler::isParameterPending() const noexcept
{
    const ScopedLock sl (eventLock);
    return pendingParam != nullptr;
}

AutomatableParameter::Ptr ParameterChangeHandler::getPendingParam (bool consumeEvent) noexcept
{
    const ScopedLock sl (eventLock);
    AutomatableParameter::Ptr paramToReturn (pendingParam);

    if (consumeEvent)
        pendingParam = nullptr;

    return paramToReturn;
}

//==============================================================================
void ParameterChangeHandler::actionFunctionTriggered (int externalControllerID)
{
    const ScopedLock sl (eventLock);
    pendingParam = nullptr;
    pendingActionId = externalControllerID;
    sendChangeMessage();
}

bool ParameterChangeHandler::isActionFunctionPending() const noexcept
{
    const ScopedLock sl (eventLock);
    return pendingActionId != -1;
}

int ParameterChangeHandler::getPendingActionFunctionId (bool consumeEvent) noexcept
{
    const ScopedLock sl (eventLock);
    const ScopedValueSetter<int> actionResetter (pendingActionId, pendingActionId,
                                                 consumeEvent ? -1 : pendingActionId);

    if (consumeEvent)
        sendChangeMessage();

    return pendingActionId;
}
