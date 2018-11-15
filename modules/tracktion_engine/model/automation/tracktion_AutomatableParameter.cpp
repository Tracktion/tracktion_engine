/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace AutomationScaleHelpers
{
    float getQuadraticBezierControlPoint (float y1, float y2, float curve) noexcept
    {
        jassert (curve >= -0.5f && curve <= 0.5f);

        auto c = jlimit (-1.0f, 1.0f, curve * 2.0f);

        if (y2 > y1)
        {
            auto rise = y2 - y1;
            auto yc = y1 + rise / 2;
            auto y = yc + rise / 2 * -c;

            return y;
        }

        auto rise = y1 - y2;
        auto yc = y2 + rise / 2;
        auto y = yc - rise / 2 * -c;

        return y;
    }

    float getCurvedValue (float value, float start, float end, float curve) noexcept
    {
        if (curve == 0.0f)
            return ((end - start) * value) + start;

        auto control = getQuadraticBezierControlPoint (start, end, curve);
        return (float) AutomationCurve::getBezierXfromT (value, start, control, end);
    }

    float mapValue (float inputVal, float offset, float value, float curve) noexcept
    {
        return inputVal < 0.0 ? offset - getCurvedValue (-inputVal, 0.0f, value, curve)
                              : offset + getCurvedValue (inputVal, 0.0f, value, curve);
    }
}

//==============================================================================
AutomatableParameter::ModifierSource::~ModifierSource()
{
    masterReference.clear();
}

//==============================================================================
struct AutomationSource  : public ReferenceCountedObject
{
    AutomationSource (const ValueTree& v) : state (v) {}
    virtual ~AutomationSource() = default;

    /** Must return the value of automation at the given time.
        This is called from the message thread and can be used for drawing etc. so
        shouldn't reposition the streams.
    */
    virtual float getValueAt (double time) = 0;

    /** Must return if the source is enabled at the given time.
        This is called from the message thread and can be used for drawing etc. so
        shouldn't reposition the streams.
    */
    virtual bool isEnabledAt (double time) = 0;

    /** Should set the position of the source to a specific time in the Edit.
        This must be thread safe as it could be called from multiple threads.
    */
    virtual void setPosition (double time) = 0;

    /** Should return true if the source is enabled at the current position. */
    virtual bool isEnabled() = 0;

    /** Should return the current value of the source. */
    virtual float getCurrentValue() = 0;

    ValueTree state;
};

//==============================================================================
struct AutomationModifierSource : public AutomationSource
{
    AutomationModifierSource (AutomatableParameter::ModifierAssignment::Ptr ass)
        : AutomationSource (ass->state),
          assignment (std::move (ass))
    {
    }

    virtual AutomatableParameter::ModifierSource* getModifierSource() = 0;

    AutomatableParameter::ModifierAssignment::Ptr assignment;
};

//==============================================================================
struct ModifierAutomationSource : public AutomationModifierSource
{
    ModifierAutomationSource (Modifier::Ptr mod, const ValueTree& assignmentState)
        : AutomationModifierSource (mod->createAssignment (assignmentState)),
          modifier (std::move (mod))
    {
        jassert (state.hasProperty (IDs::source));
    }

    AutomatableParameter::ModifierSource* getModifierSource() override
    {
        return modifier.get();
    }

    // Modifiers will be updated at the start of each block so can't be repositioned
    float getValueAt (double) override          { return getCurrentValue(); }
    bool isEnabledAt (double) override          { return true; }
    void setPosition (double) override          {}

    bool isEnabled() override
    {
        return getBoolParamValue (*modifier->enabledParam);
    }

    float getCurrentValue() override
    {
        const float baseValue = modifier->getCurrentValue();
        return AutomationScaleHelpers::mapValue (baseValue, assignment->offset, assignment->value, assignment->curve);
    }

    const Modifier::Ptr modifier;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModifierAutomationSource)
};

//==============================================================================
class AutomationCurveSource : public AutomationSource
{
public:
    AutomationCurveSource (AutomatableParameter& ap)
        : AutomationSource (getState (ap)),
          parameter (ap),
          curve (ap.parentState, state)
    {
        deferredUpdateTimer.setCallback ([this]
                                         {
                                             deferredUpdateTimer.stopTimer();
                                             updateInterpolatedPoints();
                                         });

        curve.setOwnerParameter (&ap);
    }

    void triggerAsyncCurveUpdate()
    {
        if (! parameter.getEdit().isLoading())
            deferredUpdateTimer.startTimer (10);
    }

    void updateInterpolatedPoints()
    {
        jassert (! parameter.getEdit().isLoading());
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        std::unique_ptr<AutomationIterator> newStream;

        if (curve.getNumPoints() > 0)
        {
            auto s = std::make_unique<AutomationIterator> (parameter);

            if (! s->isEmpty())
                newStream = std::move (s);
        }

        {
            const ScopedLock sl (parameterStreamLock);
            automationActive.store (newStream != nullptr, std::memory_order_relaxed);
            parameterStream = std::move (newStream);

            if (! parameterStream)
                parameter.updateToFollowCurve (lastTime);

            lastTime = -1.0;
        }

        parameter.automatableEditElement.updateActiveParameters();
    }

    bool isActive() const noexcept
    {
        return automationActive.load (std::memory_order_relaxed);
    }

    float getValueAt (double time) override
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        return curve.getValueAt (time);
    }

    bool isEnabledAt (double) override
    {
        return true;
    }

    void setPosition (double time) override
    {
        if (! parameter.getEdit().getAutomationRecordManager().isReadingAutomation())
            return;

        const ScopedLock sl (parameterStreamLock);

        if (lastTime.exchange (time) != time)
            parameterStream->setPosition (time);
    }

    bool isEnabled() override
    {
        return true;
    }

    float getCurrentValue() override
    {
        const ScopedLock sl (parameterStreamLock);
        return parameterStream->getCurrentValue();
    }

    AutomatableParameter& parameter;
    AutomationCurve curve;

private:
    LambdaTimer deferredUpdateTimer;
    juce::CriticalSection parameterStreamLock;
    std::unique_ptr<AutomationIterator> parameterStream;
    std::atomic<bool> automationActive { false };
    std::atomic<double> lastTime { -1.0 };

    static ValueTree getState (AutomatableParameter& ap)
    {
        auto v = ap.parentState.getChildWithProperty (IDs::paramID, ap.paramID);

        if (! v.isValid())
        {
            // ExternalAutomatableParameter used to use the parameter name as the property value
            auto oldAutomation = ap.parentState.getChildWithProperty (IDs::name, ap.paramName);

            if (oldAutomation.isValid())
                return oldAutomation;

            // Internal plugins have always used the paramID as the value
            return ap.parentState.getChildWithProperty (IDs::name, ap.paramID);
        }

        return v;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomationCurveSource)
};

//==============================================================================
struct MacroSource : public AutomationModifierSource
{
    MacroSource (MacroParameter::Assignment::Ptr macroAssignment, MacroParameter& macroParameter)
        : AutomationModifierSource (macroAssignment),
          assignment (macroAssignment),
          macro (&macroParameter)
    {
        jassert (state.hasType (IDs::MACRO) && state.hasProperty (IDs::source));
    }

    AutomatableParameter::ModifierSource* getModifierSource() override
    {
        return macro.get();
    }

    float getValueAt (double time) override
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        auto macroValue = macro->getCurve().getValueAt (time);
        return AutomationScaleHelpers::mapValue (macroValue, assignment->offset, assignment->value, assignment->curve);
    }

    bool isEnabledAt (double) override
    {
        return true;
    }

    void setPosition (double time) override
    {
        const ScopedLock sl (streamPositionLock);
        macro->updateFromAutomationSources (time);
        auto macroValue = macro->getCurrentValue();

        currentValue.store (AutomationScaleHelpers::mapValue (macroValue, assignment->offset, assignment->value, assignment->curve),
                            std::memory_order_release);
    }

    bool isEnabled() override
    {
        return true;
    }

    float getCurrentValue() override
    {
        return currentValue.load (std::memory_order_acquire);
    }

    const MacroParameter::Assignment::Ptr assignment;
    const MacroParameter::Ptr macro;

private:
    juce::CriticalSection streamPositionLock;
    std::atomic<float> currentValue { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacroSource)
};

//==============================================================================
struct AutomatableParameter::AutomationSourceList  : private ValueTreeObjectList<AutomationModifierSource, CriticalSection>
{
    AutomationSourceList (const AutomatableParameter& ap)
        : ValueTreeObjectList<AutomationModifierSource, CriticalSection> (ap.modifiersState),
          parameter (ap)
    {
        jassert (! ap.getEdit().isLoading());
        rebuildObjects();
        updateCachedSources();

        if (isActive())
            parameter.curveSource->triggerAsyncCurveUpdate();
    }

    ~AutomationSourceList()
    {
        freeObjects();
    }

    bool isActive() const
    {
        jassert (numSources.load() == objects.size());
        return numSources.load() > 0;
    }

    template<typename Fn>
    void visitSources (Fn f)
    {
        if (auto cs = cachedSources)
            for (auto* as : cs->sources)
                f (*as);
    }

    ReferenceCountedObjectPtr<AutomationModifierSource> getSourceFor (ModifierAssignment& ass)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD

        for (auto o : objects)
            if (o->assignment.get() == &ass)
                return o;

        return {};
    }

    ReferenceCountedObjectPtr<AutomationModifierSource> getSourceFor (ModifierSource& mod)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD

        for (auto o : objects)
            if (o->assignment->isForModifierSource (mod))
                return o;

        return {};
    }

private:
    const AutomatableParameter& parameter;
    std::atomic<int> numSources { 0 };

    // This caching mechanism is to avoid locking on the audio thread and keeps a reference
    // counted copy of the objects for the visit method to use in a lock free way
    struct CachedSources : public ReferenceCountedObject
    {
        ReferenceCountedArray<AutomationModifierSource> sources;
    };

    ReferenceCountedObjectPtr<CachedSources> cachedSources;

    void updateCachedSources()
    {
        auto cs = new CachedSources();

        for (auto o : objects)
            cs->sources.add (o);

        cachedSources = cs;
    }

    bool isSuitableType (const juce::ValueTree& v) const override
    {
        if (v.hasType (IDs::LFO) || v.hasType (IDs::BREAKPOINTOSCILLATOR) || v.hasType (IDs::MACRO)
            || v.hasType (IDs::STEP) || v.hasType (IDs::ENVELOPEFOLLOWER) || v.hasType (IDs::RANDOM)
            || v.hasType (IDs::MIDITRACKER))
        {
            // Old LFOs will have a paramID field that is a name. We'll convert it when we create the ModifierAutomationSource
            const auto isLegacyLFO = [&, this] { return v.hasType (IDs::LFO) && v[IDs::paramID].toString() == parameter.paramName; };

            if ((v[IDs::paramID] == parameter.paramID || isLegacyLFO())
                 && EditItemID::fromProperty (v, IDs::source).isValid())
            {
                return true;
            }
        }

        return false;
    }

    AutomationModifierSource* createNewObject (const juce::ValueTree& v) override
    {
        ReferenceCountedObjectPtr<AutomationModifierSource> as;

        // Convert old LFO name to ID
        if (v.hasType (IDs::LFO) && v[IDs::paramID].toString() == parameter.paramName)
            juce::ValueTree (v).setProperty (IDs::paramID, parameter.paramID, nullptr);

        auto getMacroForID = [this] (const String& id) -> MacroParameter*
        {
            for (auto mpl : getAllMacroParameterLists (parameter.getEdit()))
                for (auto mp : mpl->getMacroParameters())
                    if (mp->paramID == id)
                        return mp;

            return {};
        };

        if (auto mod = findModifierForID (parameter.getEdit(), EditItemID::fromProperty (v, IDs::source)))
            as = new ModifierAutomationSource (mod, v);
        else if (auto macro = getMacroForID (v[IDs::source].toString()))
            as = new MacroSource (new MacroParameter::Assignment (v, *macro), *macro);
        else
            jassertfalse;

        as->incReferenceCount();
        ++numSources;

        return as.get();
    }

    void deleteObject (AutomationModifierSource* as) override
    {
        --numSources;
        as->decReferenceCount();
    }

    void newObjectAdded (AutomationModifierSource* as) override { objectAddedOrRemoved (as); }
    void objectRemoved (AutomationModifierSource* as) override  { objectAddedOrRemoved (as); }
    void objectOrderChanged() override                          { objectAddedOrRemoved (nullptr); }

    void objectAddedOrRemoved (AutomationModifierSource* as)
    {
        updateCachedSources();

        auto notifySource = [] (AutomationModifierSource* source)
        {
            if (auto mas = dynamic_cast<ModifierAutomationSource*> (source))
                mas->modifier->changed();
            else if (auto macro = dynamic_cast<MacroSource*> (source))
                macro->macro->changed();
        };

        for (auto s : objects)
            if (as == nullptr || s != as)
                notifySource (s);

        notifySource (as);

        parameter.curveSource->triggerAsyncCurveUpdate();
    }
};

//==============================================================================
struct AutomatableParameter::AttachedValue  : public AsyncUpdater
{
    AttachedValue (AutomatableParameter& p, CachedValue<float>& v)
        : parameter (p), value (v)
    {
        p.setParameter (value, dontSendNotification);
    }

    ~AttachedValue()
    {
        cancelPendingUpdate();
    }

    void handleAsyncUpdate() override
    {
        value.setValue (parameter.currentValue, nullptr);
    }

    AutomatableParameter& parameter;
    CachedValue<float>& value;
};

//==============================================================================
AutomatableParameter::AutomatableParameter (const String& paramID_,
                                            const String& name_,
                                            AutomatableEditItem& owner,
                                            NormalisableRange<float> vr)
    : paramID (paramID_),
      valueRange (vr),
      automatableEditElement (owner),
      paramName (name_)
{
    if (auto p = dynamic_cast<Plugin*> (&owner))
    {
        plugin = p;
        parentState = plugin->state;
    }
    else if (auto m = dynamic_cast<Modifier*> (&owner))
    {
        modifierOwner = m;
        parentState = modifierOwner->state;
    }
    else if (auto* mpl = dynamic_cast<MacroParameterList*> (&owner))
    {
        macroOwner = mpl;
        parentState = mpl->state;
    }
    else
    {
        jassertfalse; // Unknown AutomatableEditItem type
    }

    editRef = &automatableEditElement.edit;

    modifiersState = parentState.getOrCreateChildWithName (IDs::MODIFIERASSIGNMENTS, &owner.edit.getUndoManager());
    curveSource = std::make_unique<AutomationCurveSource> (*this);

    valueToStringFunction = [] (float value)        { return String (value, 3); };
    stringToValueFunction = [] (const String& s)    { return s.getFloatValue(); };

    parentState.addListener (this);
}

AutomatableParameter::~AutomatableParameter()
{
    if (editRef != nullptr)
        editRef->getAutomationRecordManager().parameterBeingDeleted (this);

    notifyListenersOfDeletion();

    automationSourceList.reset();

    if (attachedValue != nullptr)
        attachedValue->value.getValueTree().removeListener (this);
}

AutomatableParameter::ModifierAssignment::ModifierAssignment (Edit& e, const juce::ValueTree& v)
    : edit (e), state (v)
{
    auto* um = &edit.getUndoManager();
    offset.referTo (state, IDs::offset, um);
    value.referTo (state, IDs::value, um);
    curve.referTo (state, IDs::curve, um);
}

AutomatableParameter::ModifierAssignment::Ptr AutomatableParameter::addModifier (ModifierSource& source, float value, float offset, float curve)
{
    if (auto existing = getAutomationSourceList().getSourceFor (source))
        return existing->assignment;

    ValueTree v;

    if (auto mod = dynamic_cast<Modifier*> (&source))
    {
        v = ValueTree (mod->state.getType());
        mod->itemID.setProperty (v, IDs::source, nullptr);
    }
    else if (auto macro = dynamic_cast<MacroParameter*> (&source))
    {
        v = ValueTree (IDs::MACRO);
        v.setProperty (IDs::source, macro->paramID, nullptr);
    }
    else
    {
        jassertfalse;
        return {};
    }

    jassert (v.isValid());
    v.setProperty (IDs::paramID, paramID, nullptr);
    v.setProperty (IDs::value, value, nullptr);

    if (offset != 0.0f)     v.setProperty (IDs::offset, offset, nullptr);
    if (curve != 0.5f)      v.setProperty (IDs::curve, curve, nullptr);

    modifiersState.addChild (v, -1, &getEdit().getUndoManager());

    auto as = getAutomationSourceList().getSourceFor (source);
    jassert (as != nullptr);

    return as->assignment;
}

void AutomatableParameter::removeModifier (ModifierAssignment& assignment)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    if (auto existing = getAutomationSourceList().getSourceFor (assignment))
        existing->state.getParent().removeChild (existing->state, &getEdit().getUndoManager());
    else
        jassertfalse;
}

void AutomatableParameter::removeModifier (ModifierSource& source)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    if (auto existing = getAutomationSourceList().getSourceFor (source))
        existing->state.getParent().removeChild (existing->state, &getEdit().getUndoManager());
    else
        jassertfalse;
}

juce::ReferenceCountedArray<AutomatableParameter::ModifierAssignment> AutomatableParameter::getAssignments() const
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    juce::ReferenceCountedArray<ModifierAssignment> assignments;

    getAutomationSourceList()
        .visitSources ([&assignments] (AutomationModifierSource& s) { assignments.addIfNotAlreadyThere (s.assignment); });

    return assignments;
}

juce::Array<AutomatableParameter::ModifierSource*> AutomatableParameter::getModifiers() const
{
    juce::Array<ModifierSource*> modifiers;

    getAutomationSourceList()
        .visitSources ([&modifiers] (AutomationModifierSource& s) { modifiers.addIfNotAlreadyThere (s.getModifierSource()); });

    return modifiers;
}

//==============================================================================
bool AutomatableParameter::isAutomationActive() const
{
    return curveSource->isActive() || getAutomationSourceList().isActive();
}

void AutomatableParameter::updateStream()
{
    curveSource->updateInterpolatedPoints();
}

void AutomatableParameter::updateFromAutomationSources (double time)
{
    if (updateParametersRecursionCheck)
        return;

    const ScopedValueSetter<bool> svs (updateParametersRecursionCheck, true);
    float newModifierValue = 0.0f;

    getAutomationSourceList()
        .visitSources ([&newModifierValue, time] (AutomationSource& m) mutable
                       {
                           m.setPosition (time);

                           if (m.isEnabled())
                               newModifierValue += m.getCurrentValue();
                       });

    const float newBaseValue = [this, time]
                               {
                                   if (curveSource->isActive())
                                   {
                                       curveSource->setPosition (time);
                                       return curveSource->getCurrentValue();
                                   }

                                   return currentParameterValue;
                               }();

    if (newModifierValue != 0.0f)
    {
        auto normalisedBase = valueRange.convertTo0to1 (newBaseValue);
        currentModifierValue = valueRange.convertFrom0to1 (jlimit (0.0f, 1.0f, normalisedBase + newModifierValue)) - newBaseValue;
    }
    else
    {
        currentModifierValue = 0.0f;
    }

    setParameterValue (newBaseValue, true);
}

//==============================================================================
void AutomatableParameter::valueTreePropertyChanged (ValueTree& v, const Identifier& i)
{
    if (v == getCurve().state || v.isAChildOf (getCurve().state))
    {
        curveHasChanged();
    }
    else if (attachedValue != nullptr
              && i == attachedValue->value.getPropertyID()
              && v == attachedValue->value.getValueTree())
    {
        attachedValue->value.forceUpdateOfCachedValue();
        currentValue = attachedValue->value.get();
    }
}

void AutomatableParameter::valueTreeChildAdded (ValueTree& parent, ValueTree& newChild)
{
    if (parent == getCurve().state || parent == modifiersState)
        curveHasChanged();
    else if (parent == parentState && newChild[IDs::name] == paramID)
        getCurve().setState (newChild);
}

void AutomatableParameter::valueTreeChildRemoved (ValueTree& parent, ValueTree&, int)
{
    if (parent == getCurve().state || parent == modifiersState)
        curveHasChanged();
}

void AutomatableParameter::valueTreeChildOrderChanged (ValueTree& parent, int, int)
{
    if (parent == getCurve().state || parent == modifiersState)
        curveHasChanged();
}

void AutomatableParameter::valueTreeParentChanged (ValueTree&) {}
void AutomatableParameter::valueTreeRedirected (ValueTree&)    { jassertfalse; } // need to handle this?

//==============================================================================
void AutomatableParameter::attachToCurrentValue (CachedValue<float>& v)
{
    currentValue = v;
    jassert (attachedValue == nullptr);
    attachedValue = std::make_unique<AttachedValue> (*this, v);
    v.getValueTree().addListener (this);
}

void AutomatableParameter::detachFromCurrentValue()
{
    if (attachedValue == nullptr)
        return;

    attachedValue->value.getValueTree().removeListener (this);
    attachedValue.reset();
}

Engine& AutomatableParameter::getEngine() const noexcept
{
    return getEdit().engine;
}

Edit& AutomatableParameter::getEdit() const noexcept
{
    return automatableEditElement.edit;
}

Track* AutomatableParameter::getTrack() const noexcept
{
    return plugin != nullptr ? plugin->getOwnerTrack()
                             : modifierOwner != nullptr ? getTrackContainingModifier (getEdit(), modifierOwner)
                                                        : macroOwner->getTrack();
}

AutomationCurve& AutomatableParameter::getCurve() const noexcept
{
    return curveSource->curve;
}

Selectable* AutomatableParameter::getOwnerSelectable() const
{
    if (macroOwner != nullptr)
    {
        jassertfalse;
        return {};
    }

    if (plugin != nullptr)
        return plugin;

    return modifierOwner;
}

EditItemID AutomatableParameter::getOwnerID() const
{
    // macroOwner is not a Selectable but still has an ID so only do this check for the others
    if (plugin != nullptr || modifierOwner != nullptr)
        if (! Selectable::isSelectableValid (getOwnerSelectable()))
            return {};

    return plugin != nullptr ? plugin->itemID
                             : modifierOwner != nullptr ? modifierOwner->itemID
                                                        : macroOwner->itemID;
}

String AutomatableParameter::getPluginAndParamName() const
{
    String s;

    if (plugin != nullptr)
        s << plugin->getName() + " >> ";
    else  if (modifierOwner != nullptr)
        s << modifierOwner->getName() + " >> ";
    else  if (auto af = getOwnerPlugin (macroOwner))
        s << af->getName() + " >> ";

    return s + getParameterName();
}

String AutomatableParameter::getFullName() const
{
    String s;

    if (auto t = getTrack())
        s << t->getName() << " >> ";

    return s + getPluginAndParamName();
}

//==============================================================================
void AutomatableParameter::resetRecordingStatus()
{
    isRecording = false;
}

//==============================================================================
void AutomatableParameter::setParameterValue (float value, bool isFollowingCurve)
{
    auto& curve = getCurve();
    value = snapToState (getValueRange().clipValue (value));
    currentBaseValue = value;

    if (currentModifierValue != 0.0f)
        value = snapToState (getValueRange().clipValue (value + currentModifierValue));

    if (currentValue != value)
    {
        parameterChanged (value, isFollowingCurve);

        auto& ed = getEdit();

        if (isFollowingCurve)
        {
            ed.getParameterChangeHandler().parameterChanged (*this, true);

            currentValue = value;

            if (attachedValue != nullptr)
                attachedValue->triggerAsyncUpdate();
        }
        else
        {
            if (! getEdit().isLoading())
                jassert (MessageManager::getInstance()->currentThreadHasLockedMessageManager());

            curveHasChanged();

            if (auto playhead = ed.getTransport().getCurrentPlayhead())
            {
                if (! playhead->isUserDragging())
                {
                    auto numPoints = curve.getNumPoints();
                    auto& arm = ed.getAutomationRecordManager();

                    if (playhead->isPlaying() && arm.isWritingAutomation())
                    {
                        auto time = playhead->getPosition();

                        if (! isRecording)
                        {
                            isRecording = true;
                            arm.postFirstAutomationChange (this, currentValue);
                        }

                        arm.postAutomationChange (this, time, value);
                    }
                    else
                    {
                        if (numPoints == 1)
                            curve.movePoint (0, curve.getPointTime (0), value, false);
                    }
                }
            }

            currentValue = value;

            if (attachedValue != nullptr)
            {
                attachedValue->cancelPendingUpdate();
                attachedValue->value = value;
            }
        }

        {
            SCOPED_REALTIME_CHECK
            listeners.call (&Listener::currentValueChanged, *this, currentValue);
        }
    }
}

void AutomatableParameter::setParameter (float value, NotificationType nt)
{
    currentParameterValue = value;
    setParameterValue (value, false);

    if (nt != dontSendNotification)
    {
        jassert (nt != sendNotificationAsync); // Async notifications not yet supported
        listeners.call (&Listener::parameterChanged, *this, currentValue);
    }
}

AutomatableParameter::AutomationSourceList& AutomatableParameter::getAutomationSourceList() const
{
    if (! automationSourceList)
        automationSourceList = std::make_unique<AutomationSourceList> (*this);

    return *automationSourceList;
}

void AutomatableParameter::updateToFollowCurve (double time)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    float newModifierValue = 0.0f;

    getAutomationSourceList()
        .visitSources ([&newModifierValue, time] (AutomationModifierSource& m) mutable
                       {
                           if (m.isEnabledAt (time))
                               newModifierValue += m.getValueAt (time);
                       });

    const float newBaseValue = [this, time]
                               {
                                   if (hasAutomationPoints() && ! isRecording)
                                       return curveSource->getValueAt (time);

                                   return currentParameterValue;
                               }();

    if (newModifierValue != 0.0f)
    {
        auto normalisedBase = valueRange.convertTo0to1 (newBaseValue);
        currentModifierValue = valueRange.convertFrom0to1 (jlimit (0.0f, 1.0f, normalisedBase + newModifierValue)) - newBaseValue;
    }
    else
    {
        currentModifierValue = 0.0f;
    }

    setParameterValue (newBaseValue, true);
}

void AutomatableParameter::parameterChangeGestureBegin()
{
    listeners.call (&Listener::parameterChangeGestureBegin, *this);
}

void AutomatableParameter::parameterChangeGestureEnd()
{
    listeners.call (&Listener::parameterChangeGestureEnd, *this);
}

//==============================================================================
void AutomatableParameter::midiControllerMoved (float newPosition)
{
    setParameter (snapToState (valueRange.convertFrom0to1 (newPosition)), sendNotification);
}

void AutomatableParameter::midiControllerPressed()
{
    if (isDiscrete())
    {
        int state = getStateForValue (getCurrentValue()) + 1;

        if (state >= getNumberOfStates())
            state = 0;

        setParameter (getValueForState (state), sendNotification);
    }
}

//==============================================================================
void AutomatableParameter::curveHasChanged()
{
    CRASH_TRACER
    curveSource->triggerAsyncCurveUpdate();
    getEdit().getParameterChangeHandler().parameterChanged (*this, false);
    listeners.call (&Listener::curveHasChanged, *this);
}

//==============================================================================
Array<AutomatableParameter*> getAllAutomatableParameter (Edit& edit)
{
    return edit.getAllAutomatableParams (true);
}

AutomatableParameter::ModifierSource* getSourceForAssignment (const AutomatableParameter::ModifierAssignment& ass)
{
    for (auto modifier : getAllModifierSources (ass.edit))
        if (ass.isForModifierSource (*modifier))
            return modifier;

    return {};
}

ReferenceCountedArray<AutomatableParameter> getAllParametersBeingModifiedBy (Edit& edit, AutomatableParameter::ModifierSource& m)
{
    juce::ReferenceCountedArray<AutomatableParameter> params;

    for (auto ap : edit.getAllAutomatableParams (true))
    {
        for (auto ass : ap->getAssignments())
        {
            if (ass->isForModifierSource (m))
            {
                jassert (! params.contains (ap)); // Being modified by the same source twice?
                params.add (ap);
                break;
            }
        }
    }

    return params;
}

AutomatableParameter* getParameter (AutomatableParameter::ModifierAssignment& assignment)
{
    for (auto ap : assignment.edit.getAllAutomatableParams (true))
        for (auto ass : ap->getAssignments())
            if (ass == &assignment)
                return ap;

    return {};
}

//==============================================================================
AutomationIterator::AutomationIterator (const AutomatableParameter& p)
{
    const auto& curve = p.getCurve();

    jassert (curve.getNumPoints() > 0);

    const double timeDelta      = 1.0 / 100.0;
    const double minValueDelta  = (p.getValueRange().getLength()) / 256.0;

    int curveIndex = 0;
    double t = 0.0;
    float lastValue = 1.0e10;
    auto lastTime = curve.getPointTime (curve.getNumPoints() - 1) + 1.0;
    double t1 = 0.0;
    double t2 = curve.getPointTime (0);
    float v1 = curve.getValueAt (0.0);
    float v2 = v1;
    float c  = 0;
    CurvePoint bp;
    double x1end = 0;
    double x2end = 0;
    float y1end = 0;
    float y2end = 0;

    while (t < lastTime)
    {
        while (t >= t2)
        {
            if (curveIndex >= curve.getNumPoints() - 1)
            {
                t1 = t2;
                v1 = v2;
                t2 = lastTime;
                break;
            }

            t1 = t2;
            v1 = v2;
            c  = curve.getPointCurve (curveIndex);

            if (c != 0.0f)
            {
                bp = curve.getBezierPoint (curveIndex);

                if (c < -0.5 || c > 0.5)
                    curve.getBezierEnds (curveIndex, x1end, y1end, x2end, y2end);
            }

            t2 = curve.getPointTime (++curveIndex);
            v2 = curve.getPointValue (curveIndex);
        }

        float v = v2;

        if (t2 != t1)
        {
            if (c == 0.0f)
            {
                v = v1 + (v2 - v1) * (float) ((t - t1) / (t2 - t1));
            }
            else if (c >= -0.5 && c <= 0.5)
            {
                v = AutomationCurve::getBezierYFromX (t, t1, v1, bp.time, bp.value, t2, v2);
            }
            else
            {
                if (t >= t1 && t <= x1end)
                    v = v1;
                else if (t >= x2end && t <= t2)
                    v = v2;
                else
                    v = AutomationCurve::getBezierYFromX (t, x1end, y1end, bp.time, bp.value, x2end, y2end);
            }
        }

        if (std::abs (v - lastValue) >= minValueDelta)
        {
            jassert (t >= t1 && t <= t2);

            AutoPoint point;
            point.time = t;
            point.value = v;

            jassert (points.isEmpty() || points.getLast().time <= t);
            points.add (point);

            lastValue = v;
        }

        t += timeDelta;
    }
}

void AutomationIterator::setPosition (double newTime) noexcept
{
    jassert (points.size() > 0);

    auto newIndex = currentIndex;

    if (! isPositiveAndBelow (newIndex, points.size()))
        newIndex = 0;

    if (newIndex > 0 && points.getReference (newIndex).time >= newTime)
    {
        --newIndex;

        while (newIndex > 0 && points.getReference (newIndex).time >= newTime)
            --newIndex;
    }
    else
    {
        while (newIndex < points.size() - 1 && points.getReference (newIndex + 1).time < newTime)
            ++newIndex;
    }

    if (currentIndex != newIndex)
    {
        jassert (isPositiveAndBelow (newIndex, points.size()));
        currentIndex = newIndex;
        currentValue = points.getReference (newIndex).value;
    }
}

//==============================================================================
const char* AutomationDragDropTarget::automatableDragString = "automatableParamDrag";

AutomationDragDropTarget::AutomationDragDropTarget() {}
AutomationDragDropTarget::~AutomationDragDropTarget() {}

bool AutomationDragDropTarget::isAutomatableParameterBeingDraggedOver() const
{
     return isAutoParamCurrentlyOver;
}

AutomatableParameter::Ptr AutomationDragDropTarget::getAssociatedAutomatableParameter (bool* learn)
{
    if (learn != nullptr)
        *learn = false;

    return getAssociatedAutomatableParameter();
}

bool AutomationDragDropTarget::isInterestedInDragSource (const SourceDetails& details)
{
    return details.description == automatableDragString;
}

void AutomationDragDropTarget::itemDragEnter (const SourceDetails&)
{
    isAutoParamCurrentlyOver = hasAnAutomatableParameter();

    if (auto c = dynamic_cast<Component*> (this))
        c->repaint();
}

void AutomationDragDropTarget::itemDragExit (const SourceDetails&)
{
    isAutoParamCurrentlyOver = false;

    if (auto c = dynamic_cast<Component*> (this))
        c->repaint();
}

void AutomationDragDropTarget::itemDropped (const SourceDetails& dragSourceDetails)
{
    isAutoParamCurrentlyOver = false;

    if (auto c = dynamic_cast<Component*> (this))
        c->repaint();

    if (auto source = dynamic_cast<ParameterisableDragDropSource*> (dragSourceDetails.sourceComponent.get()))
    {
        source->draggedOntoAutomatableParameterTargetBeforeParamSelection();

        bool learn = false;

        if (auto param = getAssociatedAutomatableParameter (&learn))
            source->draggedOntoAutomatableParameterTarget (param);
        else if (learn)
            startParameterLearn (source);
    }
}

//==============================================================================
#if TRACKTION_UNIT_TESTS

class AutomationTests : public UnitTest
{
public:
    AutomationTests() : UnitTest ("Automation", "Tracktion") {}

    //==============================================================================
    void runTest() override
    {
        beginTest ("Parameter time atomic tests");
        {
            std::atomic<double> lastTime { -1.0 };
            expectEquals (lastTime.load(), -1.0);
            expect (lastTime.exchange (-1.0) == -1.0);
            expect (lastTime.exchange (0.0) != 0.0);
            expectEquals (lastTime.load(), 0.0);
        }

        beginTest ("Automatable Parameter Update from V8");
        {
            // All plugins on Track 1
            // Time:                    0.0             10.0                20.0
            // FM Synth -> "Vibrato"    v = 0.00        v = 60.40           v = 100.0
            // Reverb -> "Room Size"    v = 4,          v = 7               v = 9
            // VolPan -> "Volume"       v = +0.00 dB    v = -10.43 dB       v = 22.61 dB
            // VolPan -> "Pan"          v = Centre,     v = -0.600 Left     v = -1.000 Left

            auto scopedEdit = getAutomationV8Edit();
            auto& edit = scopedEdit->getEdit();
            auto& transport = edit.getTransport();
            auto plugins = getAudioTracks (edit)[0]->pluginList.getPlugins();

            auto fm = plugins[0];
            auto reverb = plugins[1];
            auto volPan = plugins[2];

            auto setPosition = [&] (double time)
            {
                transport.setCurrentPosition (time);
                auto pos = transport.getCurrentPosition();
                expectEquals (pos, time);

                fm->setAutomatableParamPosition (pos);
                reverb->setAutomatableParamPosition (pos);
                volPan->setAutomatableParamPosition (pos);
            };

            auto vibratoParam   = fm->getAutomatableParameter (12);
            auto roomSizeParam  = reverb->getAutomatableParameterByID ("room size");
            auto volumeParam    = volPan->getAutomatableParameterByID ("volume");
            auto panParam       = volPan->getAutomatableParameterByID ("pan");

            expectEquals (vibratoParam->getParameterName(), String ("Vibrato"));

            setPosition (0.0);
            expectParameterEquals (vibratoParam, 0.0f, "0.0");
            expectParameterEquals (roomSizeParam, 0.3f, "4");
            expectParameterEquals (volumeParam, 0.740818f, "+0.0 dB");
            expectParameterEquals (panParam, 0.0f, "Centre");

            setPosition (10.0);
            expectParameterEquals (vibratoParam, 0.6f, "60.0");
            expectParameterEquals (roomSizeParam, 0.654783f, "7");
            expectParameterEquals (volumeParam, 0.439806f, "-10.43 dB");
            expectParameterEquals (panParam, -0.6f, "-0.6 Left");

            setPosition (20.0);
            expectParameterEquals (vibratoParam, 1.0f, "100.0");
            expectParameterEquals (roomSizeParam, 0.891304f, "9");
            expectParameterEquals (volumeParam, 0.23913f, "-22.61 dB");
            expectParameterEquals (panParam, -1.0f, "-1.0 Left");
        }
    }

private:
    void expectParameterEquals (const AutomatableParameter::Ptr& param, float value, const String& text)
    {
        expectWithinAbsoluteError (param->getCurrentValue(), value, 0.000001f);
        expectEquals (param->getCurrentValueAsString(), text);
    }

    struct ScopedEdit
    {
        ScopedEdit (const MemoryBlock& zippedEditFileData)
        {
            {
                MemoryInputStream mis (zippedEditFileData, false);
                ZipFile zippedFile (&mis, false);

                FileOutputStream fos (editFile.getFile());
                std::unique_ptr<InputStream> is (zippedFile.createStreamForEntry (0));
                fos << *is;
            }

            // Create ProjectDir
            projectDir = File::createTempFile ({});
            projectDir.createDirectory();
            projectDirDeleter = std::make_unique<ScopedDirectoryDeleter> (projectDir);

            // Create a project
            auto& pm = *ProjectManager::getInstance();
            project = pm.createNewProject (projectDir.getChildFile ("project")
                                                     .withFileExtension (projectFileSuffix));
            project->createNewProjectId();
            auto editItem = project->createNewEdit();
            editItem->getSourceFile().replaceWithText (editFile.getFile().loadFileAsString());

            // Load the Edit
            edit = std::make_unique<Edit> (Engine::getInstance(),
                                           loadEditFromProjectManager (pm, editItem->getID()),
                                           Edit::forRendering, nullptr, 0);
        }

        Edit& getEdit() const
        {
            return *edit;
        }

    private:
        TemporaryFile editFile;
        File projectDir;
        std::unique_ptr<ScopedDirectoryDeleter> projectDirDeleter;

        Project::Ptr project;
        std::unique_ptr<Edit> edit;
    };

    std::unique_ptr<ScopedEdit> getAutomationV8Edit()
    {
        static const unsigned char automationUpdateTestEdit[] =
        { 80,75,3,4,20,0,8,0,8,0,17,59,112,76,0,0,0,0,0,0,0,0,0,0,0,0,34,0,16,0,65,117,116,111,109,97,116,105,111,110,85,112,100,97,116,101,84,101,115,116,46,116,114,97,99,107,116,105,111,110,101,100,105,116,85,88,12,0,4,114,171,90,50,113,171,90,245,1,20,0,181,
            88,107,115,162,72,23,254,62,85,243,31,40,62,237,214,214,118,184,169,248,150,201,22,2,70,42,141,18,105,140,51,223,154,155,98,16,44,196,91,126,253,123,0,209,9,206,58,73,237,46,85,198,166,237,243,156,231,92,250,156,238,244,254,58,172,98,102,23,100,155,40,
            77,238,89,30,113,44,19,36,94,234,71,201,252,158,117,200,224,79,153,253,235,225,235,151,175,95,122,186,102,16,134,174,215,211,122,241,11,221,5,97,154,173,24,25,9,168,195,50,94,22,208,28,126,33,209,42,0,168,150,192,243,130,44,203,146,40,180,88,38,166,155,
            220,138,183,243,40,81,23,52,153,95,22,116,57,177,45,181,216,175,95,152,242,89,129,234,48,10,252,254,241,158,213,232,46,242,153,73,186,143,105,226,179,204,42,240,35,106,248,32,25,210,150,204,221,9,84,22,66,191,83,97,219,209,60,1,65,143,38,249,89,65,91,
            16,104,139,250,221,46,207,62,20,248,61,162,155,214,216,214,159,29,125,164,234,15,149,202,106,146,217,228,52,203,251,96,192,61,11,46,112,215,43,144,23,96,228,109,179,93,129,197,222,157,215,27,166,110,27,143,76,178,93,5,25,205,211,236,158,149,88,198,15,
            146,116,21,37,231,247,119,120,149,108,239,238,90,127,207,50,136,58,108,82,42,39,43,136,146,206,58,202,189,197,61,219,190,32,93,139,245,166,134,254,98,19,133,232,204,34,242,129,142,26,71,235,205,61,11,238,73,189,215,192,63,191,6,9,117,227,192,39,25,245,
            94,9,157,151,115,126,180,41,39,237,128,102,222,2,71,110,70,179,40,40,126,170,227,114,122,192,29,89,144,128,183,253,192,162,73,16,223,179,149,72,77,76,113,200,216,84,172,153,137,79,19,42,54,212,39,50,81,212,39,38,14,118,133,64,237,202,158,161,137,211,
            241,164,111,216,166,78,20,77,33,10,147,23,156,70,219,149,27,100,197,58,198,167,57,248,94,224,120,185,150,49,21,155,232,147,233,24,59,230,217,91,3,3,195,28,147,31,215,176,120,151,198,16,23,150,169,190,193,123,168,45,118,218,130,204,139,50,215,110,137,
            34,12,164,14,228,76,4,121,196,221,117,56,190,221,189,120,245,10,189,87,16,175,240,237,119,12,222,207,41,206,172,239,216,35,5,18,227,52,163,233,83,67,213,109,125,86,147,52,70,150,67,170,89,38,161,5,51,67,81,25,45,139,96,239,49,253,237,134,249,33,195,174,
            215,78,163,44,223,210,248,246,34,35,89,111,115,134,103,254,96,132,179,69,239,121,244,202,64,168,99,211,58,51,159,40,218,88,5,115,71,164,14,216,120,68,38,99,140,245,9,196,209,50,70,143,245,210,50,121,171,72,86,250,30,227,212,165,241,245,190,116,249,174,
            39,74,151,136,77,158,244,201,143,114,38,205,94,131,140,173,162,77,202,168,9,215,40,188,236,83,185,237,127,10,134,191,134,17,197,208,247,59,222,167,96,184,159,176,105,203,156,31,116,106,152,10,224,170,32,181,121,94,230,97,243,175,34,63,154,90,89,186,46,
            242,15,114,175,40,128,229,220,56,12,55,65,177,167,145,216,234,138,157,214,143,219,203,75,227,116,11,121,31,194,195,193,195,214,145,174,246,20,49,138,192,252,76,173,232,117,218,109,246,90,190,222,172,202,54,79,45,154,209,213,32,138,243,98,103,129,72,224,
            251,158,79,155,187,187,177,203,207,130,80,38,138,28,116,139,114,119,201,192,95,241,226,189,46,215,226,185,207,49,227,67,65,112,131,79,17,203,210,116,197,108,162,183,224,227,212,4,169,37,185,84,250,28,53,129,74,146,23,248,159,226,118,42,71,31,247,89,224,
            66,219,10,255,123,98,107,154,92,88,141,29,114,46,38,246,67,13,211,123,87,93,126,243,131,144,110,227,156,161,91,63,74,153,116,155,67,181,249,253,130,113,247,51,144,70,109,222,228,44,179,45,43,47,203,132,81,28,84,200,3,147,177,143,73,190,96,153,230,251,
            138,38,219,144,122,249,54,43,12,45,59,86,113,192,104,24,186,143,18,63,221,227,178,203,149,53,160,170,237,117,142,215,13,175,252,105,157,165,115,112,1,244,152,146,4,180,216,162,195,200,34,135,166,230,18,207,201,17,193,51,38,38,103,105,115,7,239,68,117,
            108,25,156,253,104,56,22,84,142,239,249,243,244,155,200,69,195,221,176,63,204,245,41,125,133,113,223,120,249,190,154,15,188,144,104,244,213,28,120,135,87,126,22,34,231,251,92,226,233,162,179,24,119,144,99,77,116,82,244,234,231,73,3,131,215,7,148,115,
            166,227,5,138,134,219,66,151,190,36,10,71,158,137,179,36,225,50,166,75,103,224,203,195,9,94,160,133,55,223,79,189,66,223,78,92,152,111,75,123,36,193,71,152,217,152,11,109,252,54,84,177,184,84,71,111,195,195,83,119,167,18,165,224,235,104,142,110,232,195,
            84,26,124,123,157,191,140,23,195,104,24,206,94,102,153,243,242,77,30,246,113,110,169,230,30,169,120,135,64,158,168,230,145,28,241,17,71,152,27,222,226,114,104,112,233,143,10,221,187,2,199,141,204,195,242,136,37,203,54,143,90,129,115,131,139,213,224,2,
            122,197,147,77,188,166,142,56,172,226,131,123,196,111,88,29,221,242,33,247,222,135,79,7,124,226,130,192,54,55,194,199,161,109,238,177,106,30,194,91,54,45,204,166,77,39,12,188,215,236,209,1,193,39,180,71,28,1,31,223,178,201,109,216,132,108,179,198,217,
            185,170,201,193,187,8,31,105,6,120,183,112,194,6,78,88,219,100,227,253,50,194,187,101,233,43,192,187,237,27,169,225,155,218,191,18,196,105,79,212,209,222,2,44,176,75,180,110,231,222,103,242,116,223,208,121,51,7,32,71,222,251,125,98,222,178,103,143,175,
            243,151,175,115,102,9,254,33,224,95,235,104,238,103,183,253,178,111,228,222,205,181,164,169,19,246,202,41,166,111,36,194,160,219,60,194,184,200,251,18,167,3,199,64,83,123,158,233,42,238,34,212,168,83,46,221,4,109,233,127,49,61,66,241,132,106,196,73,8,
            235,100,58,118,116,7,33,11,245,151,75,219,130,177,173,32,164,104,161,5,135,50,164,12,173,138,140,98,33,93,67,229,147,233,83,127,235,60,122,72,11,251,186,53,253,126,212,99,32,56,248,134,208,206,118,158,117,226,60,115,164,192,83,134,4,176,10,220,42,64,
            72,179,20,165,130,160,243,87,129,114,207,143,72,33,150,130,149,5,172,71,236,165,238,95,154,148,234,76,166,151,51,104,213,255,31,46,86,245,172,177,49,34,76,94,222,120,118,101,69,245,46,247,157,230,26,190,90,196,95,45,234,221,53,52,158,123,74,213,62,126,
            218,76,50,184,77,100,110,93,236,79,199,134,95,217,112,57,42,220,178,2,137,92,249,240,124,87,224,186,112,69,109,181,56,249,35,166,113,72,238,242,34,39,137,29,177,35,241,188,212,237,114,130,40,253,59,246,214,23,155,202,222,186,229,255,202,224,147,212,77,
            107,59,18,39,243,178,192,9,18,24,218,129,63,60,223,249,152,181,130,120,109,238,135,173,253,91,206,197,209,228,159,38,217,159,255,82,150,149,119,214,115,146,5,174,212,165,222,229,58,95,28,225,96,220,187,43,254,55,2,131,255,3,80,75,7,8,177,130,74,216,127,
            6,0,0,78,17,0,0,80,75,3,4,10,0,0,0,0,0,131,59,112,76,0,0,0,0,0,0,0,0,0,0,0,0,9,0,16,0,95,95,77,65,67,79,83,88,47,85,88,12,0,6,114,171,90,6,114,171,90,245,1,20,0,80,75,3,4,20,0,8,0,8,0,17,59,112,76,0,0,0,0,0,0,0,0,0,0,0,0,45,0,16,0,95,95,77,65,67,79,83,
            88,47,46,95,65,117,116,111,109,97,116,105,111,110,85,112,100,97,116,101,84,101,115,116,46,116,114,97,99,107,116,105,111,110,101,100,105,116,85,88,12,0,4,114,171,90,50,113,171,90,245,1,20,0,99,96,21,99,103,96,98,96,240,77,76,86,240,15,86,136,80,128,2,
            144,24,3,39,16,27,1,241,2,32,6,241,47,49,16,5,28,67,66,130,160,76,144,142,21,64,172,133,166,132,17,33,174,146,156,159,171,151,88,80,144,147,170,151,155,90,146,152,146,88,146,104,21,159,237,235,226,89,146,154,27,90,156,90,20,146,152,94,204,192,144,84,
            144,147,89,92,98,96,176,128,3,106,0,35,146,73,200,128,19,0,80,75,7,8,150,222,2,168,109,0,0,0,210,0,0,0,80,75,1,2,21,3,20,0,8,0,8,0,17,59,112,76,177,130,74,216,127,6,0,0,78,17,0,0,34,0,12,0,0,0,0,0,0,0,0,64,164,129,0,0,0,0,65,117,116,111,109,97,116,105,
            111,110,85,112,100,97,116,101,84,101,115,116,46,116,114,97,99,107,116,105,111,110,101,100,105,116,85,88,8,0,4,114,171,90,50,113,171,90,80,75,1,2,21,3,10,0,0,0,0,0,131,59,112,76,0,0,0,0,0,0,0,0,0,0,0,0,9,0,12,0,0,0,0,0,0,0,0,64,253,65,223,6,0,0,95,95,
            77,65,67,79,83,88,47,85,88,8,0,6,114,171,90,6,114,171,90,80,75,1,2,21,3,20,0,8,0,8,0,17,59,112,76,150,222,2,168,109,0,0,0,210,0,0,0,45,0,12,0,0,0,0,0,0,0,0,64,164,129,22,7,0,0,95,95,77,65,67,79,83,88,47,46,95,65,117,116,111,109,97,116,105,111,110,85,
            112,100,97,116,101,84,101,115,116,46,116,114,97,99,107,116,105,111,110,101,100,105,116,85,88,8,0,4,114,171,90,50,113,171,90,80,75,5,6,0,0,0,0,3,0,3,0,6,1,0,0,238,7,0,0,0,0,0,0 };

        return std::make_unique<ScopedEdit> (MemoryBlock (automationUpdateTestEdit, sizeof (automationUpdateTestEdit)));
    }
};

static AutomationTests automationTests;

#endif // TRACKTION_UNIT_TESTS
