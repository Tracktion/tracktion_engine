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

namespace AutomationScaleHelpers
{
    float getQuadraticBezierControlPoint (float y1, float y2, float curve) noexcept
    {
        jassert (curve >= -0.5f && curve <= 0.5f);

        auto c = juce::jlimit (-1.0f, 1.0f, curve * 2.0f);

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
struct AutomationSource  : public juce::ReferenceCountedObject
{
    AutomationSource (const juce::ValueTree& v) : state (v) {}
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

    juce::ValueTree state;
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
    ModifierAutomationSource (Modifier::Ptr mod, const juce::ValueTree& assignmentState)
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
            const juce::ScopedLock sl (parameterStreamLock);
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

        const juce::ScopedLock sl (parameterStreamLock);

        if (lastTime.exchange (time) != time)
            parameterStream->setPosition (time);
    }

    bool isEnabled() override
    {
        return true;
    }

    float getCurrentValue() override
    {
        const juce::ScopedLock sl (parameterStreamLock);
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

    static juce::ValueTree getState (AutomatableParameter& ap)
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
        const juce::ScopedLock sl (streamPositionLock);
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
struct AutomatableParameter::AutomationSourceList  : private ValueTreeObjectList<AutomationModifierSource, juce::CriticalSection>
{
    AutomationSourceList (const AutomatableParameter& ap)
        : ValueTreeObjectList<AutomationModifierSource, juce::CriticalSection> (ap.modifiersState),
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

    juce::ReferenceCountedObjectPtr<AutomationModifierSource> getSourceFor (ModifierAssignment& ass)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD

        for (auto o : objects)
            if (o->assignment.get() == &ass)
                return o;

        return {};
    }

    juce::ReferenceCountedObjectPtr<AutomationModifierSource> getSourceFor (ModifierSource& mod)
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
        juce::ReferenceCountedArray<AutomationModifierSource> sources;
    };

    juce::ReferenceCountedObjectPtr<CachedSources> cachedSources;

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
        juce::ReferenceCountedObjectPtr<AutomationModifierSource> as;

        // Convert old LFO name to ID
        if (v.hasType (IDs::LFO) && v[IDs::paramID].toString() == parameter.paramName)
            juce::ValueTree (v).setProperty (IDs::paramID, parameter.paramID, nullptr);

        auto getMacroForID = [this] (const juce::String& id) -> MacroParameter*
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
            return nullptr;

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
struct AutomatableParameter::AttachedValue  : public juce::AsyncUpdater
{
    AttachedValue (AutomatableParameter& p)
        : parameter (p) {}

    virtual ~AttachedValue() { cancelPendingUpdate(); }

    virtual void setValue (float v) = 0;
    virtual float getValue() = 0;
    virtual float getDefault() = 0;
    virtual void detach (ValueTree::Listener* l) = 0;
    virtual bool updateIfMatches (ValueTree& v, const Identifier& i) = 0;

    AutomatableParameter& parameter;
};

struct AutomatableParameter::AttachedFloatValue : public AutomatableParameter::AttachedValue
{
    AttachedFloatValue (AutomatableParameter& p, juce::CachedValue<float>& v)
        : AttachedValue (p), value (v)
    {
        parameter.setParameter (value, juce::dontSendNotification);
    }

    void handleAsyncUpdate() override               { value.setValue (parameter.currentValue, nullptr); }
    float getValue() override                       { return value; }
    void setValue (float v) override                { value = v; }
    float getDefault() override                     { return value.getDefault(); }
    void detach (ValueTree::Listener* l) override   { value.getValueTree().removeListener (l); }

    bool updateIfMatches (ValueTree& v, const Identifier& i) override
    {
        if (i == value.getPropertyID() && v == value.getValueTree())
        {
            value.forceUpdateOfCachedValue();
            return true;
        }
        return false;
    }

    CachedValue<float>& value;
};

struct AutomatableParameter::AttachedIntValue : public AutomatableParameter::AttachedValue
{
    AttachedIntValue (AutomatableParameter& p, juce::CachedValue<int>& v)
        : AttachedValue (p), value (v)
    {
        parameter.setParameter ((float) value.get(), juce::dontSendNotification);
    }

    void handleAsyncUpdate() override               { value.setValue (roundToInt (parameter.currentValue), nullptr); }
    float getValue() override                       { return (float) value.get(); }
    void setValue (float v) override                { value = roundToInt (v); }
    float getDefault() override                     { return (float) value.getDefault(); }
    void detach (ValueTree::Listener* l) override   { value.getValueTree().removeListener (l); }

    bool updateIfMatches (ValueTree& v, const Identifier& i) override
    {
        if (i == value.getPropertyID() && v == value.getValueTree())
        {
            value.forceUpdateOfCachedValue();
            return true;
        }
        return false;
    }

    CachedValue<int>& value;
};

struct AutomatableParameter::AttachedBoolValue : public AutomatableParameter::AttachedValue
{
    AttachedBoolValue (AutomatableParameter& p, juce::CachedValue<bool>& v)
        : AttachedValue (p), value (v)
    {
        parameter.setParameter (value.get() ? 1.0f : 0.0f, juce::dontSendNotification);
    }

    void handleAsyncUpdate() override               { value.setValue (parameter.currentValue != 0.0f, nullptr); }
    float getValue() override                       { return value; }
    void setValue (float v) override                { value = v != 0 ? true : false; }
    float getDefault() override                     { return value.getDefault() ? 1.0f : 0.0f; }
    void detach (ValueTree::Listener* l) override   { value.getValueTree().removeListener (l); }

    bool updateIfMatches (ValueTree& v, const Identifier& i) override
    {
        if (i == value.getPropertyID() && v == value.getValueTree())
        {
            value.forceUpdateOfCachedValue();
            return true;
        }
        return false;
    }

    CachedValue<bool>& value;
};


//==============================================================================
AutomatableParameter::AutomatableParameter (const juce::String& paramID_,
                                            const juce::String& name_,
                                            AutomatableEditItem& owner,
                                            juce::NormalisableRange<float> vr)
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

    valueToStringFunction = [] (float value)              { return juce::String (value, 3); };
    stringToValueFunction = [] (const juce::String& s)    { return s.getFloatValue(); };

    parentState.addListener (this);
}

AutomatableParameter::~AutomatableParameter()
{
    if (editRef != nullptr)
        editRef->getAutomationRecordManager().parameterBeingDeleted (*this);

    notifyListenersOfDeletion();

    automationSourceList.reset();

    if (attachedValue != nullptr)
        attachedValue->detach (this);
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

    juce::ValueTree v;

    if (auto mod = dynamic_cast<Modifier*> (&source))
    {
        v = juce::ValueTree (mod->state.getType());
        mod->itemID.setProperty (v, IDs::source, nullptr);
    }
    else if (auto macro = dynamic_cast<MacroParameter*> (&source))
    {
        v = juce::ValueTree (IDs::MACRO);
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

float AutomatableParameter::getDefaultValue() const
{
    if (attachedValue != nullptr)
        return attachedValue->getDefault();

    return 0.0f;
}

void AutomatableParameter::updateStream()
{
    curveSource->updateInterpolatedPoints();
}

void AutomatableParameter::updateFromAutomationSources (double time)
{
    if (updateParametersRecursionCheck)
        return;

    const juce::ScopedValueSetter<bool> svs (updateParametersRecursionCheck, true);
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
        currentModifierValue = valueRange.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, normalisedBase + newModifierValue)) - newBaseValue;
    }
    else
    {
        currentModifierValue = 0.0f;
    }

    setParameterValue (newBaseValue, true);
}

//==============================================================================
void AutomatableParameter::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
{
    if (v == getCurve().state || v.isAChildOf (getCurve().state))
    {
        curveHasChanged();
    }
    else if (attachedValue != nullptr && attachedValue->updateIfMatches (v, i))
    {
        currentValue = attachedValue->getValue();

        SCOPED_REALTIME_CHECK
        listeners.call (&Listener::currentValueChanged, *this, currentValue);
    }
}

void AutomatableParameter::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& newChild)
{
    if (parent == getCurve().state || parent == modifiersState)
        curveHasChanged();
    else if (parent == parentState && newChild[IDs::name] == paramID)
        getCurve().setState (newChild);
}

void AutomatableParameter::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (parent == getCurve().state || parent == modifiersState)
        curveHasChanged();
}

void AutomatableParameter::valueTreeChildOrderChanged (juce::ValueTree& parent, int, int)
{
    if (parent == getCurve().state || parent == modifiersState)
        curveHasChanged();
}

void AutomatableParameter::valueTreeParentChanged (juce::ValueTree&) {}
void AutomatableParameter::valueTreeRedirected (juce::ValueTree&)    { jassertfalse; } // need to handle this?

//==============================================================================
void AutomatableParameter::attachToCurrentValue (juce::CachedValue<float>& v)
{
    currentValue = v;
    jassert (attachedValue == nullptr);
    attachedValue.reset (new AttachedFloatValue (*this, v));
    v.getValueTree().addListener (this);
}

void AutomatableParameter::attachToCurrentValue (juce::CachedValue<int>& v)
{
    currentValue = (float) v.get();
    jassert (attachedValue == nullptr);
    attachedValue.reset (new AttachedIntValue (*this, v));
    v.getValueTree().addListener (this);
}

void AutomatableParameter::attachToCurrentValue (juce::CachedValue<bool>& v)
{
    currentValue = v;
    jassert (attachedValue == nullptr);
    attachedValue.reset (new AttachedBoolValue (*this, v));
    v.getValueTree().addListener (this);
}

void AutomatableParameter::detachFromCurrentValue()
{
    if (attachedValue == nullptr)
        return;

    attachedValue->detach (this);
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
    if (plugin != nullptr)
    {
        jassert (Selectable::isSelectableValid (plugin));
        return plugin->itemID;
    }

    if (modifierOwner != nullptr)
    {
        jassert (Selectable::isSelectableValid (modifierOwner));
        return modifierOwner->itemID;
    }

    jassert (macroOwner != nullptr);
    return macroOwner->itemID;
}

juce::String AutomatableParameter::getPluginAndParamName() const
{
    juce::String s;

    if (plugin != nullptr)
        s << plugin->getName() + " >> ";
    else  if (modifierOwner != nullptr)
        s << modifierOwner->getName() + " >> ";
    else  if (auto af = getOwnerPlugin (macroOwner))
        s << af->getName() + " >> ";

    return s + getParameterName();
}

juce::String AutomatableParameter::getFullName() const
{
    juce::String s;

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
                jassert (juce::MessageManager::getInstance()->currentThreadHasLockedMessageManager());

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
                            arm.postFirstAutomationChange (*this, currentValue);
                        }

                        arm.postAutomationChange (*this, time, value);
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
                attachedValue->setValue (value);
            }
        }

        {
            SCOPED_REALTIME_CHECK
            listeners.call (&Listener::currentValueChanged, *this, currentValue);
        }
    }
}

void AutomatableParameter::setParameter (float value, juce::NotificationType nt)
{
    currentParameterValue = value;
    setParameterValue (value, false);

    if (nt != juce::dontSendNotification)
    {
        jassert (nt != juce::sendNotificationAsync); // Async notifications not yet supported
        listeners.call (&Listener::parameterChanged, *this, currentValue);
    }
}

void AutomatableParameter::setNormalisedParameter (float value, juce::NotificationType nt)
{
    setParameter (valueRange.convertFrom0to1 (jlimit (0.0f, 1.0f, value)), nt);
}

juce::String AutomatableParameter::getCurrentValueAsStringWithLabel()
{
    auto text = getCurrentValueAsString();
    auto label = getLabel();

    if (! (label.isEmpty() || text.endsWith (label)))
        return text + ' ' + label;

    return text;
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
        currentModifierValue = valueRange.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, normalisedBase + newModifierValue)) - newBaseValue;
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
    setParameter (snapToState (valueRange.convertFrom0to1 (newPosition)), juce::sendNotification);
}

void AutomatableParameter::midiControllerPressed()
{
    if (isDiscrete())
    {
        int state = getStateForValue (getCurrentValue()) + 1;

        if (state >= getNumberOfStates())
            state = 0;

        setParameter (getValueForState (state), juce::sendNotification);
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
juce::Array<AutomatableParameter*> getAllAutomatableParameter (Edit& edit)
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

juce::ReferenceCountedArray<AutomatableParameter> getAllParametersBeingModifiedBy (Edit& edit, AutomatableParameter::ModifierSource& m)
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

    if (! juce::isPositiveAndBelow (newIndex, points.size()))
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
        jassert (juce::isPositiveAndBelow (newIndex, points.size()));
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

    if (auto c = dynamic_cast<juce::Component*> (this))
        c->repaint();
}

void AutomationDragDropTarget::itemDragExit (const SourceDetails&)
{
    isAutoParamCurrentlyOver = false;

    if (auto c = dynamic_cast<juce::Component*> (this))
        c->repaint();
}

void AutomationDragDropTarget::itemDropped (const SourceDetails& dragSourceDetails)
{
    isAutoParamCurrentlyOver = false;

    if (auto c = dynamic_cast<juce::Component*> (this))
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

}
