/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/**
    Base class for elements that have some kind of automatable parameters.
*/
class AutomatableEditItem  : public EditItem
{
public:
    AutomatableEditItem (Edit&, const juce::ValueTree&);
    virtual ~AutomatableEditItem();

    //==============================================================================
    virtual void flushPluginStateToValueTree();
    virtual void restorePluginStateFromValueTree (const juce::ValueTree&) { jassertfalse; }

    //==============================================================================
    juce::Array<AutomatableParameter*> getAutomatableParameters() const;
    int getNumAutomatableParameters() const;
    AutomatableParameter::Ptr getAutomatableParameter (int index) const     { return automatableParams[index]; }
    AutomatableParameter::Ptr getAutomatableParameterByID (const juce::String& paramID) const;

    void deleteParameter (AutomatableParameter*);
    void deleteAutomatableParameters();

    int indexOfAutomatableParameter (const AutomatableParameter::Ptr&) const;

    AutomatableParameterTree& getParameterTree() const;
    juce::ReferenceCountedArray<AutomatableParameter> getFlattenedParameterTree() const;

    //==============================================================================
    // true if it's got any points on any params
    bool isAutomationNeeded() const noexcept                    { return automationActive.load (std::memory_order_relaxed); }

    // updates any automatables to their state at this time
    void setAutomatableParamPosition (double t);

    // true if it's not been more than a few hundred ms since a block was processed
    bool isBeingActivelyPlayed() const;

    /** Updates all the auto params to their positions at this time. */
    virtual void updateAutomatableParamPosition (double time);

    /** Updates all the parameter streams to their positions at this time.
        This should be used during real time processing as it's a lot quicker than the above method.
    */
    void updateParameterStreams (double time);

    /** Iterates all the parameters to find out which ones need to be automated. */
    void updateActiveParameters();

    /** Marks the end of an automation recording stream. Call this when play stops or starts. */
    void resetRecordingStatus();

    //==============================================================================
    juce::ValueTree elementState;
    juce::CachedValue<bool> remapOnTempoChange;

protected:
    virtual void buildParameterTree() const;

    void updateLastPlaybackTime();
    void addAutomatableParameter (const AutomatableParameter::Ptr&);
    void rebuildParameterTree();

    /** Saves the explicit value of any parameters that have deviated to the state. */
    void saveChangedParametersToState();

    /** Restores the value of any explicitly set parameters. */
    void restoreChangedParametersFromState();

private:
    juce::CriticalSection activeParameterLock;
    juce::ReferenceCountedArray<AutomatableParameter> automatableParams, activeParameters;
    mutable AutomatableParameterTree parameterTree;

    mutable bool parameterTreeBuilt = false;
    std::atomic<bool> automationActive { false };
    juce::uint32 systemTimeOfLastPlayedBlock = 0;
    double lastTime = 0.0;

    //==============================================================================
    juce::ReferenceCountedArray<AutomatableParameter> getFlattenedParameterTree (AutomatableParameterTree::TreeNode&) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomatableEditItem)
};

} // namespace tracktion_engine
