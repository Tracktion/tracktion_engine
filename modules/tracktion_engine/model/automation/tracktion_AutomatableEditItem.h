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

/**
    Base class for elements that have some kind of automatable parameters.
*/
class AutomatableEditItem  : public EditItem
{
public:
    AutomatableEditItem (Edit&, const juce::ValueTree&);
    ~AutomatableEditItem() override;

    //==============================================================================
    virtual void flushPluginStateToValueTree();
    virtual void restorePluginStateFromValueTree (const juce::ValueTree&) { jassertfalse; }

    //==============================================================================
    juce::Array<AutomatableParameter*> getAutomatableParameters() const;
    int getNumAutomatableParameters() const;
    AutomatableParameter::Ptr getAutomatableParameter (int index) const     { return automatableParams[index]; }
    AutomatableParameter::Ptr getAutomatableParameterByID (const juce::String& paramID) const;
    void visitAllAutomatableParams (const std::function<void(AutomatableParameter&)>& visit) const;

    void deleteParameter (AutomatableParameter*);
    void deleteAutomatableParameters();

    int indexOfAutomatableParameter (const AutomatableParameter::Ptr&) const;

    AutomatableParameterTree& getParameterTree() const;
    juce::ReferenceCountedArray<AutomatableParameter> getFlattenedParameterTree() const;

    struct ParameterListChangeListener
    {
        virtual ~ParameterListChangeListener() {}
        virtual void parameterListChanged (AutomatableEditItem&) = 0;
    };

    void addParameterListChangeListener (ParameterListChangeListener*);
    void removeParameterListChangeListener (ParameterListChangeListener*);

    //==============================================================================
    // true if it's got any points on any params
    bool isAutomationNeeded() const noexcept;

    // updates any automatables to their state at this time
    void setAutomatableParamPosition (TimePosition);

    // true if it's not been more than a few hundred ms since a block was processed
    bool isBeingActivelyPlayed() const;

    /** Updates all the auto params to their positions at this time.
        [[ message_thread ]]
    */
    virtual void updateAutomatableParamPosition (TimePosition);

    /** Updates all the parameter streams to their positions at this time.
        This should be used during real time processing as it's a lot quicker than
        the above method but is called automatically by the audio graph so
        shouldn't really be called manually.
    */
    void updateParameterStreams (TimePosition);

    /** Marks the end of an automation recording stream. Call this when play stops or starts. */
    void resetRecordingStatus();

    //==============================================================================
    juce::ValueTree elementState;
    juce::CachedValue<bool> remapOnTempoChange;

    /** @internal. */
    void updateStreamIterators();
    /** @internal. */
    void addActiveParameter (const AutomatableParameter&);
    /** @internal. */
    void removeActiveParameter (const AutomatableParameter&);
    /** @internal. Testing only. */
    bool isActiveParameter (AutomatableParameter&);

protected:
    virtual void buildParameterTree() const;

    void updateLastPlaybackTime();
    void clearParameterList();
    void addAutomatableParameter (const AutomatableParameter::Ptr&);
    void rebuildParameterTree();

    /** Saves the explicit value of any parameters that have deviated to the state. */
    void saveChangedParametersToState();

    /** Restores the value of any explicitly set parameters. */
    void restoreChangedParametersFromState();

private:
    RealTimeSpinLock activeParameterLock;
    juce::ReferenceCountedArray<AutomatableParameter> automatableParams, activeParameters;
    mutable AutomatableParameterTree parameterTree;

    mutable bool parameterTreeBuilt = false;
    friend struct AutomatableParameter::ScopedActiveParameter;
    mutable std::atomic<int> numActiveParameters { 0 };
    uint32_t systemTimeOfLastPlayedBlock = 0;
    TimePosition lastTime;

    struct ParameterChangeListeners  : public juce::AsyncUpdater
    {
        ParameterChangeListeners (AutomatableEditItem&);
        AutomatableEditItem& owner;
        juce::ListenerList<ParameterListChangeListener> listeners;
        void handleAsyncUpdate() override;
    };

    std::unique_ptr<ParameterChangeListeners> parameterChangeListeners;
    void sendListChangeMessage() const;

    //==============================================================================
    juce::ReferenceCountedArray<AutomatableParameter> getFlattenedParameterTree (AutomatableParameterTree::TreeNode&) const;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AutomatableEditItem)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomatableEditItem)
};

}} // namespace tracktion { inline namespace engine
