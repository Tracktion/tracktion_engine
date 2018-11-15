/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** Manages a set of AutomatableParameters for an Edit and notifies controllers and
    registered listeners when they change.
*/
class ParameterChangeHandler   : public juce::ChangeBroadcaster
{
public:
    //==============================================================================
    /** Creates a ParameterChangeHandler for a given Edit. */
    ParameterChangeHandler (Edit&);

    /** Returns true if there is either a parameter or action waiting to be assigned. */
    bool isEventPending() const noexcept;

    /** Temporarily disables change events e.g. during a tempo change. */
    struct Disabler
    {
        Disabler (ParameterChangeHandler& p) : owner (p)    { p.enabled = false; }
        ~Disabler()                                         { owner.enabled = true; }

        ParameterChangeHandler& owner;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Disabler)
    };

    //==============================================================================
    /** Called by parameters when they are changed.
        This is internally called by each parameter and will forward the call on to appropriate
        controllers and mapping sets if required.
    */
    void parameterChanged (AutomatableParameter& parameter, bool fromAutomation);

    /** Returns true if there is an AutomatableParameter waiting to be assigned.
    */
    bool isParameterPending() const noexcept;

    /** Returns the parameter waiting to be assigned if there is one, otherwise a nullptr.
        Note that if consumeEvent is true this will consume the pending event so can't be
        reused for other controllers.
    */
    AutomatableParameter::Ptr getPendingParam (bool consumeEvent) noexcept;

    //==============================================================================
    /** Called by the accelerators when an assignable action is called.
        This gets added to the pending list which CustomControllerSurfaces can check when controllers
        are moved and they are in MIDI learn mode.
    */
    void actionFunctionTriggered (int externalControllerID);

    /** Returns true if there is an action function waiting to be assigned.
    */
    bool isActionFunctionPending() const noexcept;

    /** Returns the pending action function waiting to be assigned or a nullptr if none has been.
        The object returned should be owned by the caller.
        Note that if consumeEvent is true this will consume the pending event so can't be reused for other controllers.
    */
    int getPendingActionFunctionId (bool consumeEvent) noexcept;

    //==============================================================================
    /** Set true when UI up for learning parameters */
    void setParameterLearnActive (bool a)     { parameterLearnActive = a; }

private:
    //==============================================================================
    Edit& edit;
    int pendingActionId = -1;
    bool enabled = true;
    bool parameterLearnActive = false;
    AutomatableParameter::Ptr pendingParam;
    juce::CriticalSection eventLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterChangeHandler)
};

} // namespace tracktion_engine
