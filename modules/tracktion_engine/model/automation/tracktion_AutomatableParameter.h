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

class AutomatableParameter   : public juce::ReferenceCountedObject,
                               public Selectable,
                               private juce::ValueTree::Listener
{
public:
    AutomatableParameter (const juce::String& paramID,
                          const juce::String& name,
                          AutomatableEditItem&,
                          juce::NormalisableRange<float> valueRange);

    ~AutomatableParameter() override;

    using Ptr   = juce::ReferenceCountedObjectPtr<AutomatableParameter>;
    using Array = juce::ReferenceCountedArray<AutomatableParameter>;

    const juce::String paramID;
    const juce::NormalisableRange<float> valueRange;
    AutomatableEditItem& automatableEditElement;

    juce::Range<float> getValueRange() const               { return valueRange.getRange(); }
    Plugin* getPlugin() const                              { return plugin; }

    Engine& getEngine() const noexcept;
    Edit& getEdit() const noexcept;
    Track* getTrack() const noexcept;

    AutomationCurve& getCurve() const noexcept;

    void attachToCurrentValue (juce::CachedValue<float>&);
    void attachToCurrentValue (juce::CachedValue<int>&);
    void attachToCurrentValue (juce::CachedValue<bool>&);
    void updateFromAttachedValue();
    void detachFromCurrentValue();

    //==============================================================================
    virtual juce::String getParameterName() const               { return paramName; }
    virtual juce::String getParameterShortName (int) const      { return paramName; }
    virtual juce::String getLabel()                             { return {}; }

    // returns a string "pluginname / paramname"
    virtual juce::String getPluginAndParamName() const;

    // returns "track / pluginname / paramname"
    virtual juce::String getFullName() const;

    /** Returns the thing that you'd select if you wanted to show this param */
    Selectable* getOwnerSelectable() const;

    /** Returns the thing that you'd select if you wanted to show this param */
    EditItemID getOwnerID() const;

    //==============================================================================
    float getCurrentValue() const noexcept                      { return currentValue; }
    float getCurrentNormalisedValue() const noexcept            { return valueRange.convertTo0to1 (currentValue); }

    virtual juce::String valueToString (float value)            { return valueToStringFunction (value); }
    virtual float stringToValue (const juce::String& s)         { return stringToValueFunction (s); }

    virtual juce::String getCurrentValueAsString()              { return valueToString (getCurrentValue()); }
    juce::String getCurrentValueAsStringWithLabel();

    std::function<juce::String(float)> valueToStringFunction;
    std::function<float(const juce::String&)> stringToValueFunction;

    // should be called to change a parameter when a user is actively moving it
    void setParameter (float value, juce::NotificationType);
    void setNormalisedParameter (float value, juce::NotificationType);
    void updateToFollowCurve (TimePosition);

    /** Call to indicate this parameter is about to be changed. */
    void parameterChangeGestureBegin();

    /** Call to indicate this parameter has stopped being to be changed. */
    void parameterChangeGestureEnd();

    bool hasAutomationPoints() const noexcept                   { return getCurve().getNumPoints() > 0; }

    //==============================================================================
    /** Base class for things that can be used to modify parameters. */
    struct ModifierSource
    {
        ModifierSource() = default;
        virtual ~ModifierSource();

    private:
        juce::WeakReference<ModifierSource>::Master masterReference;
        friend class juce::WeakReference<ModifierSource>;
    };

    /** Connects a modifier source to an AutomatableParameter. */
    struct ModifierAssignment : public juce::ReferenceCountedObject
    {
        using Ptr = juce::ReferenceCountedObjectPtr<ModifierAssignment>;

        ModifierAssignment (Edit&, const juce::ValueTree&);

        /** Must return true if this assigment is for the given source. */
        virtual bool isForModifierSource (const ModifierSource&) const = 0;

        Edit& edit;
        juce::ValueTree state;
        juce::CachedValue<float> value, offset, curve;
        juce::CachedValue<float> inputStart, inputEnd;
    };

    /** Creates an assignment for a given source.
        @param value    the value of the assignment 0 - 1
        @param offset   the offset of the assignment 0 - 1
        @param curve    the curve of the assignment 0 - 1 where 0.5 is a linear mapping
    */
    ModifierAssignment::Ptr addModifier (ModifierSource&, float value = 1.0f, float offset = 0.0f, float curve = 0.5f);

    /** Removes an assignment. N.B. the passed in assignment is likely to be deleted after this call.
        @returns true if the ModifierAssignment was removed
    */
    bool removeModifier (ModifierAssignment&);

    /** Removes assignments for a ModifierSource.
        @returns true if the ModifierSource was removed
    */
    bool removeModifier (ModifierSource&);

    /** Returns true if any ModifierSources are currently in use by assignments. */
    bool hasActiveModifierAssignments() const;

    /** Returns all the current ModifierAssignments. */
    juce::ReferenceCountedArray<ModifierAssignment> getAssignments() const;

    /** Returns all the current ModifierSources currently in use by assignments. */
    juce::Array<ModifierSource*> getModifiers() const;

    //==============================================================================
    /** This is the value that has been set explicity, either by calling setParameter
        or the plugin telling us one if its parameters has changed.
    */
    float getCurrentExplicitValue() const                       { return currentParameterValue; }

    /** This is the current base value of the parameter i.e. either the explicit value or
        the automation curve value.
    */
    float getCurrentBaseValue() const                           { return currentBaseValue; }

    /** This is the ammount of the modifier that has been applied to the base value to
        give the current parameter value.
    */
    float getCurrentModifierValue() const                       { return currentModifierValue; }

    //==============================================================================
    /** Returns true if the parameter is being dynamically changed somehow, either
        through automation or a ModifierAssignment.
    */
    bool isAutomationActive() const;

    /**  Forces the parameter to update its automation stream for reading automation. */
    void updateStream();

    /** Updates the parameter and modifier values from its current automation sources. */
    void updateFromAutomationSources (TimePosition);

    //==============================================================================
    virtual bool isParameterActive() const                          { return true; }
    virtual bool isDiscrete() const                                 { return false; }
    virtual int getNumberOfStates() const                           { return 0; }
    virtual float getValueForState (int) const                      { return 0; }
    virtual int getStateForValue (float) const                      { return 0; }
    virtual std::optional<float> getDefaultValue() const;

    virtual bool hasLabels()  const                                 { return false; }
    virtual juce::String getLabelForValue (float) const             { return {}; }
    virtual float snapToState (float val) const                     { return val; }
    virtual juce::StringArray getAllLabels() const                  { return {}; }

    //==============================================================================
    /** true if the parameter been moved while in an automation record mode. */
    bool isCurrentlyRecording() const                               { return isRecording; }

    //==============================================================================
    // called by ParameterControlMappings
    void midiControllerMoved (float newPosition);
    void midiControllerPressed();

    //==============================================================================
    void curveHasChanged();

    //==============================================================================
    juce::String getSelectableDescription() override    { return TRANS("Automation Parameter"); }

    const juce::String paramName;
    juce::ValueTree parentState;

    //==============================================================================
    struct Listener
    {
        virtual ~Listener() {}

        /** Called when the automation curve has changed, point time, value or curve. */
        virtual void curveHasChanged (AutomatableParameter&) = 0;

        /** Called when the current value of the parameter changed, either from setting the parameter,
            automation, a macro or modifier.
            This is async so you won't get a callback for every parameter change.
        */
        virtual void currentValueChanged (AutomatableParameter&) {}

        /** Called when the parameter is changed by the plugin or host, not from automation. */
        virtual void parameterChanged (AutomatableParameter&, float /*newValue*/) {}
        virtual void parameterChangeGestureBegin (AutomatableParameter&) {}
        virtual void parameterChangeGestureEnd (AutomatableParameter&) {}

        /** Called when this parameter starts or stops recording. */
        virtual void recordingStatusChanged (AutomatableParameter&) {}
    };

    void addListener (Listener* l)              { listeners.add (l); }
    void removeListener (Listener* l)           { listeners.remove (l); }

    /** @internal */
    struct ScopedActiveParameter
    {
        ScopedActiveParameter (const AutomatableParameter&);
        ~ScopedActiveParameter();

        const AutomatableParameter& parameter;
    };

    /** @internal */
    void resetRecordingStatus();
    /** @internal */
    void startRecordingStatus();

protected:
    struct AttachedValue;
    struct AttachedFloatValue;
    struct AttachedIntValue;
    struct AttachedBoolValue;
    std::unique_ptr<AttachedValue> attachedValue;
    juce::ListenerList<Listener> listeners;

    SafeSelectable<Edit> editRef;
    Plugin* plugin = nullptr;
    Modifier* modifierOwner = nullptr;
    MacroParameterList* macroOwner = nullptr;
    std::unique_ptr<AutomationCurveSource> curveSource;
    std::atomic<float> currentValue { 0.0f }, currentParameterValue { 0.0f },  currentBaseValue { 0.0f }, currentModifierValue { 0.0f };
    mutable std::atomic<int> numActiveAutomationSources { 0 };
    std::atomic<bool> isRecording { false };
    bool updateParametersRecursionCheck = false;
    AsyncCaller parameterChangedCaller { [this] { listeners.call (&Listener::currentValueChanged, *this); } };
    int gestureCount = 0;

    juce::ValueTree modifiersState;
    struct AutomationSourceList;
    mutable std::unique_ptr<AutomationSourceList> automationSourceList;

    AutomationSourceList& getAutomationSourceList() const;

    void setParameterValue (float value, bool isFollowingCurve);

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;
    void valueTreeParentChanged (juce::ValueTree&) override;
    void valueTreeRedirected (juce::ValueTree&) override;

    virtual void parameterChanged (float, bool /*byAutomation*/) {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomatableParameter)
};


//==============================================================================
/** Looks for the Track this parameter is currently on and returns the AutomationMode for it. */
AutomationMode getAutomationMode (const AutomatableParameter&);

/** Returns all the Assignments of a specific type. */
template<typename AssignmentType>
juce::ReferenceCountedArray<AssignmentType> getAssignmentsOfType (const AutomatableParameter& ap)
{
    juce::ReferenceCountedArray<AssignmentType> assignments;

    for (auto ass : ap.getAssignments())
        if (auto type = dynamic_cast<AssignmentType*> (ass))
            assignments.add (type);

    return assignments;
}

/** Returns all the modifers in use of a specific type. */
template<typename ModifierType>
juce::Array<ModifierType*> getModifiersOfType (const AutomatableParameter& ap)
{
    juce::Array<ModifierType*> mods;

    for (auto mod : ap.getModifiers())
        if (auto type = dynamic_cast<ModifierType*> (mod))
            mods.add (type);

    return mods;
}

/** Iterates an Edit looking for all parameters that assigned to a given parameter. */
template<typename AssignmentType, typename ModifierSourceType, typename EditType>
juce::ReferenceCountedArray<AssignmentType> getAssignmentsForSource (EditType& edit, const ModifierSourceType& source)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    juce::ReferenceCountedArray<AssignmentType> assignments;

    edit.visitAllAutomatableParams (true, [&] (AutomatableParameter& param)
    {
        for (auto* ass : param.getAssignments())
            if (auto* m = dynamic_cast<AssignmentType*> (ass))
                if (m->isForModifierSource (source))
                    assignments.add (m);
    });

    return assignments;
}

/** Iterates an Edit looking for the source of this assignment. */
AutomatableParameter::ModifierSource* getSourceForAssignment (const AutomatableParameter::ModifierAssignment&);

/** Iterates an Edit looking for all parameters that are being modified by the given ModifierSource. */
juce::ReferenceCountedArray<AutomatableParameter> getAllParametersBeingModifiedBy (Edit&, AutomatableParameter::ModifierSource&);

/** Iterates an Edit looking for the parameter that this ModifierAssignment has been made from. */
AutomatableParameter* getParameter (AutomatableParameter::ModifierAssignment&);

//==============================================================================
/** Returns an int version of an AutomatableParameter. */
inline int getIntParamValue (const AutomatableParameter& ap)
{
    return juce::roundToInt (ap.getCurrentValue());
}

/** Returns a bool version of an AutomatableParameter. */
inline bool getBoolParamValue (const AutomatableParameter& ap)
{
    return getIntParamValue (ap) == 1;
}

/** Returns an int version of an AutomatableParameter cast to a enum type. */
template<typename Type>
inline Type getTypedParamValue (const AutomatableParameter& ap)
{
    return static_cast<Type> (getIntParamValue (ap));
}

//==============================================================================
/**
    Components can implement this to let things know which automatable parameter
    they control.
*/
class AutomationDragDropTarget   : public juce::DragAndDropTarget
{
public:
    AutomationDragDropTarget();
    ~AutomationDragDropTarget() override;

    //==============================================================================
    virtual bool hasAnAutomatableParameter() = 0;

    // this will only be called when actually dropped, so if there are multiple params,
    // it should show UI to choose one and then invoke the callback asynchronously
    virtual void chooseAutomatableParameter (std::function<void(AutomatableParameter::Ptr)> handleChosenParam,
                                             std::function<void()> startLearnMode) = 0;

    // start the process for learning a parameter and call the callback when one is chosen
    virtual void startParameterLearn (std::function<void(AutomatableParameter::Ptr)>) {}

    // the subclass can call this so it knows to draw itself differently when dragging over
    bool isAutomatableParameterBeingDraggedOver() const;

    bool isInterestedInDragSource (const SourceDetails& details) override;
    void itemDragEnter (const SourceDetails& dragSourceDetails) override;
    void itemDragExit (const SourceDetails& dragSourceDetails) override;
    void itemDropped (const SourceDetails& dragSourceDetails) override;

    static const char* automatableDragString;

private:
    bool isAutoParamCurrentlyOver = false;
};


//==============================================================================
class ParameterisableDragDropSource
{
public:
    ParameterisableDragDropSource() {}
    virtual ~ParameterisableDragDropSource() {}

    // just to say that the user has used the drag function
    virtual void draggedOntoAutomatableParameterTargetBeforeParamSelection() = 0;

    // this is called if they actually choose a param
    virtual void draggedOntoAutomatableParameterTarget (const AutomatableParameter::Ptr& param) = 0;
};


//==============================================================================
/**
    A cache of automation points, with a cursor which moves through it.
*/
struct AutomationIterator
{
    AutomationIterator (Edit&, const AutomationCurve&);
    AutomationIterator (const AutomatableParameter&);

    bool isEmpty() const noexcept               { return points.size() <= 1; }

    void setPosition (EditPosition) noexcept;
    float getCurrentValue() noexcept            { return currentValue; }

private:
    int updateIndex (double position);

    struct AutoPoint
    {
        double time = 0.0; // Could be time or beats depending on the curve's timeBase
        float value = 0.0f;
        float curve = 0.0f;
    };

    const tempo::Sequence& tempoSequence;
    juce::Array<AutoPoint> points;
    int currentIndex = -1;
    float currentValue = 0.0f;
    const AutomationCurve::TimeBase timeBase;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomationIterator)
};

}} // namespace tracktion { inline namespace engine
