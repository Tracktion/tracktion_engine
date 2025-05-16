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

namespace AutomationScaleHelpers
{
    inline float getQuadraticBezierControlPoint (float y1, float y2, float curve) noexcept
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

    inline float getCurvedValue (float value, float start, float end, float curve) noexcept
    {
        if (curve == 0.0f)
            return ((end - start) * value) + start;

        auto control = getQuadraticBezierControlPoint (start, end, curve);
        return (float) getBezierXfromT (value, start, control, end);
    }

    inline float mapValue (float inputVal, float offset, float value, float curve) noexcept
    {
        return inputVal < 0.0 ? offset - getCurvedValue (-inputVal, 0.0f, value, curve)
                              : offset + getCurvedValue (inputVal, 0.0f, value, curve);
    }

    /** Remaps an input value from a given input range to 0-1. */
    inline float remapInputValue (float inputVal, juce::Range<float> inputRange)
    {
        jassert (juce::isPositiveAndNotGreaterThan (inputVal, 1.0f));
        auto remappedValue = juce::jmap (inputRange.clipValue (inputVal),
                                         inputRange.getStart(), inputRange.getEnd(),
                                         0.0f, 1.0f);
        jassert (juce::isPositiveAndNotGreaterThan (remappedValue, 1.0f));
        return remappedValue;
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
    virtual float getValueAt (TimePosition) = 0;

    /** Must return if the source is enabled at the given time.
        This is called from the message thread and can be used for drawing etc. so
        shouldn't reposition the streams.
    */
    virtual bool isEnabledAt (TimePosition) = 0;

    /** Should set the position of the source to a specific time in the Edit.
        This must be thread safe as it could be called from multiple threads.
    */
    virtual void setPosition (TimePosition) = 0;

    /** Should return true if the source is enabled at the current position. */
    virtual bool isEnabled() = 0;

    /** Should return the current value of the source. */
    virtual float getCurrentValue() = 0;

    /** Is called by the message thread to modify the automation value.
        By default, most modifier types just add to the modValue but some types
        can overwrite the base or scale the mod etc.
    */
    virtual void processValueAt (TimePosition t, [[maybe_unused]] float& baseValue, float& modValue)
    {
        modValue += getValueAt (t);
    }

    /** Is called during processing to modify the automation value.
        By default, most modifier types just add to the modValue but some types
        can overwrite the base or scale the mod etc.
    */
    virtual void processValue ([[maybe_unused]] float& baseValue, float& modValue)
    {
        float currentModValue = getCurrentValue();
        jassert (! std::isnan (currentModValue));
        modValue += currentModValue;
    }

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
    float getValueAt (TimePosition) override          { return getCurrentValue(); }
    bool isEnabledAt (TimePosition) override          { return true; }

    void setPosition (TimePosition newEditTime) override
    {
        editTimeToReturn = newEditTime;
    }

    bool isEnabled() override
    {
        return getBoolParamValue (*modifier->enabledParam);
    }

    float getCurrentValue() override
    {
        float baseValue = modifier->getCurrentValue();

        const auto currentTime = modifier->getCurrentTime();
        const auto deltaTime = currentTime - editTimeToReturn;

        if (deltaTime > 0s && deltaTime < Modifier::maxHistoryTime)
            baseValue = modifier->getValueAt (deltaTime);

        return AutomationScaleHelpers::mapValue (baseValue, assignment->offset, assignment->value, assignment->curve);
    }

    const Modifier::Ptr modifier;
    TimePosition editTimeToReturn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModifierAutomationSource)
};

//==============================================================================
class AutomationCurveSource : public AutomationSource
{
public:
    AutomationCurveSource (AutomatableParameter& ap)
        : AutomationSource (getState (ap)),
          parameter (ap),
          curve (ap.getEdit(), AutomationCurve::TimeBase::time,
                 ap.parentState, state)
    {
        deferredUpdateTimer.setCallback ([this]
                                         {
                                             deferredUpdateTimer.stopTimer();
                                             updateIterator();
                                         });

        curve.setParameterID (ap.paramID);

        if (curve.getNumPoints() > 0)
            triggerAsyncIteratorUpdate();
    }

    void triggerAsyncIteratorUpdate()
    {
        if (int numPoints = curve.getNumPoints();
            numPoints > 0 && ! scopedActiveParameter)
        {
            scopedActiveParameter = std::make_unique<AutomatableParameter::ScopedActiveParameter> (parameter);
        }
        else if (numPoints == 0 && scopedActiveParameter)
        {
            scopedActiveParameter.reset();
        }

        deferredUpdateTimer.startTimer (10);
    }

    void updateIteratorIfNeeded()
    {
        if (deferredUpdateTimer.isTimerRunning())
            deferredUpdateTimer.timerCallback();
    }

    bool isActive() const noexcept
    {
        return automationActive.load (std::memory_order_relaxed);
    }

    float getValueAt (TimePosition time) override
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        return curve.getValueAt (time, parameter.getCurrentBaseValue());
    }

    bool isEnabledAt (TimePosition) override
    {
        return isEnabled();
    }

    void setPosition (TimePosition time) override
    {
        if (! parameter.getEdit().getAutomationRecordManager().isReadingAutomation())
            if (auto plugin = parameter.getPlugin())
                if (! plugin->isClipEffectPlugin())
                    return;

        const juce::ScopedLock sl (parameterStreamLock);

        if (lastTime.exchange (time) != time)
            parameterStream->setPosition (time);
    }

    bool isEnabled() override
    {
        return ! curve.bypass.get();
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
    std::atomic<TimePosition> lastTime { TimePosition::fromSeconds (-1.0) };
    std::unique_ptr<AutomatableParameter::ScopedActiveParameter> scopedActiveParameter;

    void updateIterator()
    {
        auto editLoading = parameter.getEdit().isLoading();

        CRASH_TRACER

        if (! editLoading)
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

            if (parameterStream)
            {
                auto activeParam = std::make_unique<AutomatableParameter::ScopedActiveParameter> (parameter);
                std::swap (scopedActiveParameter, activeParam);
            }
            else
            {
                scopedActiveParameter.reset();

                if (! editLoading)
                    parameter.updateToFollowCurve (lastTime);
            }

            lastTime = -1.0s;
        }
    }

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
          macro (&macroParameter)
    {
        jassert (state.hasType (IDs::MACRO) && state.hasProperty (IDs::source));

        // This is called here to ensure the AutomationSourceList isn't created
        // on the audio thread during a later setPosition call
        macro->hasActiveModifierAssignments();
    }

    AutomatableParameter::ModifierSource* getModifierSource() override
    {
        return macro.get();
    }

    float getValueAt (TimePosition time) override
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        auto macroValue = macro->getCurve().getValueAt (time, macro->getCurrentBaseValue());
        const auto range = juce::Range<float>::between (assignment->inputStart.get(), assignment->inputEnd.get());
        return AutomationScaleHelpers::mapValue (AutomationScaleHelpers::remapInputValue (macroValue, range),
                                                 assignment->offset, assignment->value, assignment->curve);
    }

    bool isEnabledAt (TimePosition) override
    {
        return true;
    }

    void setPosition (TimePosition time) override
    {
        const juce::ScopedLock sl (streamPositionLock);
        macro->updateFromAutomationSources (time);
        auto macroValue = macro->getCurrentValue();

        const auto range = juce::Range<float>::between (assignment->inputStart.get(), assignment->inputEnd.get());
        currentValue.store (AutomationScaleHelpers::mapValue (AutomationScaleHelpers::remapInputValue (macroValue, range),
                                                              assignment->offset, assignment->value, assignment->curve),
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

    const MacroParameter::Ptr macro;

private:
    juce::CriticalSection streamPositionLock;
    std::atomic<float> currentValue { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacroSource)
};

//==============================================================================
class AutomationCurveModifierSource : public AutomationModifierSource,
                                      public AutomationCurveModifier::Listener,
                                      public SelectableListener
{
public:
    AutomationCurveModifierSource (AutomatableParameter& parameter_,
                                   AutomationCurveModifier::Assignment::Ptr curveAssignment,
                                   AutomationCurveModifier& acm)
        : AutomationModifierSource (std::move (curveAssignment)),
          curveModifier (acm),
          parameter (parameter_)
    {
        deferredUpdateTimer.setCallback ([this]
                                         {
                                             deferredUpdateTimer.stopTimer();
                                             updateIterator();
                                         });
        triggerAsyncIteratorUpdate();
    }

    void triggerAsyncIteratorUpdate()
    {
        if (! curveModifier.edit.isLoading())
            deferredUpdateTimer.startTimer (10);
    }

    void updateIteratorIfNeeded()
    {
        if (deferredUpdateTimer.isTimerRunning())
            deferredUpdateTimer.timerCallback();
    }

    bool isActive() const noexcept
    {
        for (auto& curve : curves)
            if (curve.isActive())
                return true;

        return false;
    }

    float getValueAt (TimePosition) override
    {
        assert(false && "Shouldn't be called for a modifier type");
        return 0.0f;
    }

    bool isEnabledAt (TimePosition t) override
    {
        auto& ts = getTempoSequence (curveModifier);
        auto curveClipRange = toTime (curveModifier.getPosition().clipRange, ts);
        return curveClipRange.contains (t);
    }

    void setPosition (TimePosition editTime) override
    {
        if (lastTime.exchange (editTime) == editTime)
            return;

        bool anyEnabled = false;

        for (auto& curve : curves)
            if (curve.setPosition (editTime))
                anyEnabled = true;

        enabledAtCurrentStreamTime.store (anyEnabled);
    }

    bool isEnabled() override
    {
        if (! enabledAtCurrentStreamTime.load (std::memory_order_acquire))
            return false;

        for (auto& curve : curves)
            if (! curve.curveInfo.curve.bypass.get())
                return true;

        return false;
    }

    float getCurrentValue() override
    {
        assert(false && "Shouldn't be called for a modifier type");
        return 0.0f;
    }

    void processValueAt (TimePosition t, float& baseValue, float& modValue) override
    {
        auto values = getValuesAtEditPosition (curveModifier, parameter, t);

        if (values.baseValue)
            baseValue = *values.baseValue;

        if (values.modValue)
            modValue = *values.modValue;
    }

    void processValue (float& baseValue, float& modValue) override
    {
        for (auto& curve : curves)
            curve.processValue (baseValue, modValue);
    }

    AutomatableParameter::ModifierSource* getModifierSource() override
    {
        return &curveModifier;
    }

    AutomationCurveModifier& curveModifier;
    AutomatableParameter& parameter;

private:
    SafeScopedListener curveModifierListener { makeSafeRef (curveModifier), *this };
    SafeScopedListener curveSelectableListener { makeSafeRef<Selectable> (curveModifier), *this };
    LambdaTimer deferredUpdateTimer;
    std::atomic<bool> enabledAtCurrentStreamTime { false };
    std::atomic<TimePosition> lastTime { -1.0s };

    struct CurveWrapper
    {
        CurveWrapper (AutomationCurveModifier& curveModifier_,
                      AutomatableParameter& parameter_,
                      AutomationCurveModifier::CurveInfo info)
            : curveModifier (curveModifier_),
              parameter (parameter_),
              curveInfo (info)
        {
        }

        AutomationCurveModifier& curveModifier;
        AutomatableParameter& parameter;
        AutomationCurveModifier::CurveInfo curveInfo;
        std::shared_ptr<AutomationCurvePlayhead> playhead { curveModifier.getPlayhead (curveInfo.type) };
        std::unique_ptr<AutomationIterator> parameterStream;
        juce::CriticalSection parameterStreamLock;
        std::atomic<bool> automationActive { false };

        bool isActive() const
        {
            return automationActive.load (std::memory_order_relaxed);
        }

        bool setPosition (TimePosition editTime)
        {
            const juce::ScopedLock sl (parameterStreamLock);

            if (parameterStream)
            {
                auto modifiedPos = editPositionToCurvePosition (curveModifier, curveInfo.type, editTime);
                playhead->position.store (modifiedPos);

                if (modifiedPos)
                {
                    parameterStream->setPosition (toTime (*modifiedPos,
                                                  getTempoSequence (curveModifier).getInternalSequence()));
                    return true;
                }
            }

            return false;
        }

        float getCurrentValue()
        {
            const juce::ScopedLock sl (parameterStreamLock);

            if (parameterStream)
                return parameterStream->getCurrentValue();

            return 0.0f;
        }

        void updateCachedIterator()
        {
            CRASH_TRACER
            TRACKTION_ASSERT_MESSAGE_THREAD

            std::unique_ptr<AutomationIterator> newStream;

            if (curveInfo.curve.getNumPoints() > 0)
            {
                auto s = std::make_unique<AutomationIterator> (curveInfo.curve.edit, curveInfo.curve);

                if (! s->isEmpty())
                    newStream = std::move (s);
            }

            {
                const juce::ScopedLock sl (parameterStreamLock);
                automationActive.store (newStream != nullptr, std::memory_order_relaxed);
                parameterStream = std::move (newStream);
            }
        }

        void processValue (float& baseValue, float& modValue)
        {
            if (isActive())
                detail::processValue (parameter, curveInfo.type, getCurrentValue(), baseValue, modValue);
        }
    };

    std::array<CurveWrapper, 3> curves { CurveWrapper { curveModifier, parameter, curveModifier.getCurve (CurveModifierType::absolute) },
                                         CurveWrapper { curveModifier, parameter, curveModifier.getCurve (CurveModifierType::relative) },
                                         CurveWrapper { curveModifier, parameter, curveModifier.getCurve (CurveModifierType::scale) } };

    void updateIterator()
    {
        jassert (! curveModifier.edit.isLoading());
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        for (auto& curve : curves)
            curve.updateCachedIterator();

        if (! isActive())
            parameter.updateToFollowCurve (lastTime);

        lastTime = -1.0s;
    }

    void curveChanged() override
    {
        triggerAsyncIteratorUpdate();
    }

    void selectableObjectChanged (Selectable*) override
    {
        deferredUpdateTimer.timerCallback();
    }

    void selectableObjectAboutToBeDeleted (Selectable*) override
    {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomationCurveModifierSource)
};


//==============================================================================
struct AutomatableParameter::AutomationSourceList  : private ValueTreeObjectList<AutomationModifierSource, juce::CriticalSection>
{
    AutomationSourceList (const AutomatableParameter& ap)
        : ValueTreeObjectList<AutomationModifierSource, juce::CriticalSection> (ap.modifiersState),
          parameter (ap)
    {
        jassert (! ap.getEdit().isLoading()); // This can't be created before the Edit has loaded
                                              // or it won't be able to find the sources

        if (! getUndoManager (parameter).isPerformingUndoRedo())
            removeInvalidAutomationCurveModifiers (parent, parameter);

        rebuildObjects();
        updateCachedSources();

        if (isActive())
            parameter.curveSource->triggerAsyncIteratorUpdate();
    }

    ~AutomationSourceList() override
    {
        freeObjects();
    }

    static bool hasAutomationSources (const juce::ValueTree& stateToCheck)
    {
        for (auto v : stateToCheck)
            if (isAutomationSourceType (v))
                return true;

        return false;
    }

    bool isActive() const
    {
        auto num = numSources.load (std::memory_order_acquire);
        jassert (num == objects.size());
        return num > 0;
    }

    template<typename Fn>
    void visitSources (Fn&& f)
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
    std::stack<AutomatableParameter::ScopedActiveParameter> activeParameters;

    // This caching mechanism is to avoid locking on the audio thread and keeps a reference
    // counted copy of the objects for the visit method to use in a lock free way
    struct CachedSources : public ReferenceCountedObject
    {
        juce::ReferenceCountedArray<AutomationModifierSource> sources;
    };

    juce::ReferenceCountedObjectPtr<CachedSources> cachedSources;

    void updateCachedSources()
    {
        if (objects.isEmpty())
        {
            cachedSources.reset();
        }
        else
        {
            auto cs = new CachedSources();

            for (auto o : objects)
                cs->sources.add (o);

            cachedSources = cs;
        }
    }

    static bool isAutomationSourceType (const juce::ValueTree& v)
    {
        return v.hasType (IDs::LFO) || v.hasType (IDs::BREAKPOINTOSCILLATOR) || v.hasType (IDs::MACRO)
            || v.hasType (IDs::STEP) || v.hasType (IDs::ENVELOPEFOLLOWER) || v.hasType (IDs::RANDOM)
            || v.hasType (IDs::MIDITRACKER) || v.hasType (IDs::AUTOMATIONCURVE);
    }

    bool isSuitableType (const juce::ValueTree& v) const override
    {
        if (isAutomationSourceType (v))
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
        {
            if (v.isAChildOf (mod->state))
                return nullptr;

            as = new ModifierAutomationSource (mod, v);
        }
        else if (auto macro = getMacroForID (v[IDs::source].toString()))
        {
            if (v.isAChildOf (macro->state))
                return nullptr;

            as = new MacroSource (new MacroParameter::Assignment (v, *macro), *macro);
        }
        else if (auto curveModifier = getAutomationCurveModifierForID (parameter.getEdit(), EditItemID::fromProperty (v, IDs::source)))
        {
            auto assignedParam = getParameter (*curveModifier);
            assert (assignedParam);
            assert (&parameter == assignedParam);
            as = new AutomationCurveModifierSource (*assignedParam,
                                                    new AutomationCurveModifier::Assignment (*curveModifier, v),
                                                    *curveModifier);
        }
        else
        {
            return nullptr;
        }

        as->incReferenceCount();
        activeParameters.emplace (parameter);
        ++numSources;

        return as.get();
    }

    void deleteObject (AutomationModifierSource* as) override
    {
        activeParameters.pop();
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

        // Force an update of the base/modifier values for the current time for this parameter
        // as modifiers have been added.
        // Do this synchronously as this is after Edit construction
        parameter.curveSource->triggerAsyncIteratorUpdate();
        parameter.curveSource->updateIteratorIfNeeded();
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
    virtual void detach (juce::ValueTree::Listener*) = 0;
    virtual bool updateIfMatches (juce::ValueTree&, const juce::Identifier&) = 0;
    virtual void updateParameterFromValue() = 0;

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
    void detach (juce::ValueTree::Listener* l) override   { value.getValueTree().removeListener (l); }

    bool updateIfMatches (juce::ValueTree& v, const juce::Identifier& i) override
    {
        if (i == value.getPropertyID() && v == value.getValueTree())
        {
            value.forceUpdateOfCachedValue();
            return true;
        }
        return false;
    }

    void updateParameterFromValue() override
    {
        parameter.setParameter (value, juce::dontSendNotification);
    }

    juce::CachedValue<float>& value;
};

struct AutomatableParameter::AttachedIntValue : public AutomatableParameter::AttachedValue
{
    AttachedIntValue (AutomatableParameter& p, juce::CachedValue<int>& v)
        : AttachedValue (p), value (v)
    {
        parameter.setParameter ((float) value.get(), juce::dontSendNotification);
    }

    void handleAsyncUpdate() override               { value.setValue (juce::roundToInt (parameter.getCurrentValue()), nullptr); }
    float getValue() override                       { return (float) value.get(); }
    void setValue (float v) override                { value = juce::roundToInt (v); }
    float getDefault() override                     { return (float) value.getDefault(); }
    void detach (juce::ValueTree::Listener* l) override   { value.getValueTree().removeListener (l); }

    bool updateIfMatches (juce::ValueTree& v, const juce::Identifier& i) override
    {
        if (i == value.getPropertyID() && v == value.getValueTree())
        {
            value.forceUpdateOfCachedValue();
            return true;
        }
        return false;
    }

    void updateParameterFromValue() override
    {
        parameter.setParameter ((float) value.get(), juce::dontSendNotification);
    }

    juce::CachedValue<int>& value;
};

struct AutomatableParameter::AttachedBoolValue : public AutomatableParameter::AttachedValue
{
    AttachedBoolValue (AutomatableParameter& p, juce::CachedValue<bool>& v)
        : AttachedValue (p), value (v)
    {
        parameter.setParameter (value.get() ? 1.0f : 0.0f, juce::dontSendNotification);
    }

    void handleAsyncUpdate() override                     { value.setValue (parameter.currentValue != 0.0f, nullptr); }
    float getValue() override                             { return value; }
    void setValue (float v) override                      { value = v != 0 ? true : false; }
    float getDefault() override                           { return value.getDefault() ? 1.0f : 0.0f; }
    void detach (juce::ValueTree::Listener* l) override   { value.getValueTree().removeListener (l); }

    bool updateIfMatches (juce::ValueTree& v, const juce::Identifier& i) override
    {
        if (i == value.getPropertyID() && v == value.getValueTree())
        {
            value.forceUpdateOfCachedValue();
            return true;
        }
        return false;
    }

    void updateParameterFromValue() override
    {
        parameter.setParameter (value.get() ? 1.0f : 0.0f, juce::dontSendNotification);
    }

    juce::CachedValue<bool>& value;
};


//==============================================================================
AutomatableParameter::AutomatableParameter (const juce::String& paramID_,
                                            const juce::String& name_,
                                            AutomatableEditItem& owner,
                                            juce::NormalisableRange<float> vr)
    : paramID (paramID_),
      valueRange (vr),
      automatableEditElement (owner),
      paramName (name_),
      editRef (&automatableEditElement.edit)
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

    modifiersState = parentState.getOrCreateChildWithName (IDs::MODIFIERASSIGNMENTS, &owner.edit.getUndoManager());

    curveSource = std::make_unique<AutomationCurveSource> (*this);

    valueToStringFunction = [] (float value)              { return juce::String (value, 3); };
    stringToValueFunction = [] (const juce::String& s)    { return s.getFloatValue(); };

    parentState.addListener (this);
}

AutomatableParameter::~AutomatableParameter()
{
    if (auto edit = editRef.get())
        edit->getAutomationRecordManager().parameterBeingDeleted (*this);

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

    inputStart.referTo (state, IDs::start, um, 0.0f);
    inputEnd.referTo (state, IDs::end, um, 1.0f);
}

AutomatableParameter::ModifierAssignment::Ptr AutomatableParameter::addModifier (ModifierSource& source, float value, float offset, float curve)
{
    if (auto existing = getAutomationSourceList().getSourceFor (source))
        return existing->assignment;

    juce::ValueTree v;

    if (auto mod = dynamic_cast<Modifier*> (&source))
    {
        if (mod == &automatableEditElement)
            return {};

        v = createValueTree (mod->state.getType(),
                             IDs::source, mod->itemID);
    }
    else if (auto macro = dynamic_cast<MacroParameter*> (&source))
    {
        v = createValueTree (IDs::MACRO,
                             IDs::source, macro->paramID);
    }
    else if (auto automationCurveModifier = dynamic_cast<AutomationCurveModifier*> (&source))
    {
        v = createValueTree (IDs::AUTOMATIONCURVE,
                             IDs::source, automationCurveModifier->itemID);
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

bool AutomatableParameter::removeModifier (ModifierAssignment& assignment)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (auto existing = getAutomationSourceList().getSourceFor (assignment))
    {
        existing->state.getParent().removeChild (existing->state, &getEdit().getUndoManager());
        return true;
    }

    return false;
}

bool AutomatableParameter::removeModifier (ModifierSource& source)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (auto existing = getAutomationSourceList().getSourceFor (source))
    {
        existing->state.getParent().removeChild (existing->state, &getEdit().getUndoManager());
        return true;
    }

    return false;
}

bool AutomatableParameter::hasActiveModifierAssignments() const
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    return getAutomationSourceList().isActive();
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
    return numActiveAutomationSources.load (std::memory_order_acquire) > 0;
}

std::optional<float> AutomatableParameter::getDefaultValue() const
{
    if (attachedValue != nullptr)
        return attachedValue->getDefault();

    return {};
}

void AutomatableParameter::updateStream()
{
    curveSource->updateIteratorIfNeeded();

    if (automationSourceList || AutomationSourceList::hasAutomationSources (modifiersState))
        getAutomationSourceList()
            .visitSources ([] (AutomationSource& m)
                           {
                               if (auto curveModSource = dynamic_cast<AutomationCurveModifierSource*> (&m))
                                   curveModSource->updateIteratorIfNeeded();
                           });
}

void AutomatableParameter::updateFromAutomationSources (TimePosition time)
{
    if (updateParametersRecursionCheck)
        return;

    const juce::ScopedValueSetter<bool> svs (updateParametersRecursionCheck, true);

    float newModifierValue = 0.0f;
    float newBaseValue = [this, time]
                         {
                             if (curveSource->isActive()
                                 && curveSource->isEnabledAt (time)
                                 && ! isCurrentlyRecording())
                             {
                                 curveSource->setPosition (time);
                                 return curveSource->getCurrentValue();
                             }

                             return currentParameterValue.load();
                         }();

    getAutomationSourceList()
        .visitSources ([&newBaseValue, &newModifierValue, time] (AutomationSource& m) mutable
                       {
                           m.setPosition (time);

                           if (m.isEnabled())
                               m.processValue (newBaseValue, newModifierValue);
                       });

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
        if (i == IDs::bypass)
            changed();

        curveHasChanged();
    }
    else if (attachedValue != nullptr && attachedValue->updateIfMatches (v, i))
    {
        // N.B.You shouldn't be directly setting the value of an attachedValue managed parameter.
        // To avoid feedback loops of sync issues, always go via setParameter

        TRACKTION_ASSERT_MESSAGE_THREAD
        SCOPED_REALTIME_CHECK
        // N.B. we shouldn't call attachedValue->updateParameterFromValue here as this
        // will set the base value of the parameter. The change in property could be due
        // to a Modifier or automation change so we don't want to force that to be the base value
        listeners.call (&Listener::currentValueChanged, *this);
    }
}

void AutomatableParameter::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& newChild)
{
    if (parent == getCurve().state || parent == modifiersState)
        curveHasChanged();
    else if (parent == getCurve().parentState)
        curveHasChanged();
    else if (parent == parentState && newChild[IDs::name] == paramID)
        getCurve().setState (newChild);
}

void AutomatableParameter::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (parent == getCurve().state || parent == modifiersState)
        curveHasChanged();
    else if (parent == getCurve().parentState)
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
    currentParameterValue = currentValue = v;
    jassert (attachedValue == nullptr);
    attachedValue = std::make_unique<AttachedFloatValue> (*this, v);
    v.getValueTree().addListener (this);
}

void AutomatableParameter::attachToCurrentValue (juce::CachedValue<int>& v)
{
    currentParameterValue = currentValue = (float) v.get();
    jassert (attachedValue == nullptr);
    attachedValue = std::make_unique<AttachedIntValue> (*this, v);
    v.getValueTree().addListener (this);
}

void AutomatableParameter::attachToCurrentValue (juce::CachedValue<bool>& v)
{
    currentParameterValue = currentValue = v;
    jassert (attachedValue == nullptr);
    attachedValue = std::make_unique<AttachedBoolValue> (*this, v);
    v.getValueTree().addListener (this);
}

void AutomatableParameter::updateFromAttachedValue()
{
    if (attachedValue)
        attachedValue->updateParameterFromValue();
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
        return {};

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
    if (! isRecording)
        return;

    isRecording = false;
    listeners.call (&Listener::recordingStatusChanged, *this);
}

//==============================================================================
void AutomatableParameter::setParameterValue (float value, bool isFollowingCurve)
{
    auto& curve = getCurve();
    value = snapToState (getValueRange().clipValue (value));
    currentBaseValue = value;

    if (currentModifierValue != 0.0f)
        value = snapToState (getValueRange().clipValue (value + currentModifierValue));

    auto oldValue = currentValue.load();

    if (oldValue != value)
    {
        currentValue = value;

        parameterChanged (value, isFollowingCurve);

        auto& ed = getEdit();

        if (isFollowingCurve)
        {
            ed.getParameterChangeHandler().parameterChanged (*this, true);

            if (attachedValue != nullptr)
                attachedValue->triggerAsyncUpdate();
        }
        else
        {
            if (! ed.isLoading())
                jassert (juce::MessageManager::getInstance()->currentThreadHasLockedMessageManager());

            curveHasChanged();

            if (auto epc = ed.getTransport().getCurrentPlaybackContext())
            {
                if (! epc->isDragging())
                {
                    auto numPoints = curve.getNumPoints();
                    auto& arm = ed.getAutomationRecordManager();

                    if (epc->isPlaying() && arm.isWritingAutomation())
                    {
                        auto time = epc->getPosition();

                        if (! isRecording)
                        {
                            isRecording = true;
                            arm.postFirstAutomationChange (*this, oldValue, AutomationTrigger::value);
                            listeners.call (&Listener::recordingStatusChanged, *this);
                        }

                        arm.postAutomationChange (*this, time, value);
                    }
                    else
                    {
                        if (numPoints == 1)
                            curve.movePoint (*this, 0, curve.getPointTime (0), value, false, &ed.getUndoManager());
                    }
                }
            }

            if (attachedValue != nullptr)
            {
                attachedValue->cancelPendingUpdate();
                attachedValue->setValue (value);
            }
        }

        {
            SCOPED_REALTIME_CHECK
            parameterChangedCaller.triggerAsyncUpdate();
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
        TRACKTION_ASSERT_MESSAGE_THREAD
        listeners.call (&Listener::parameterChanged, *this, currentValue);
        getEdit().getParameterChangeHandler().parameterChanged (*this, false);

        if (attachedValue != nullptr)
        {
            // Updates the ValueTree via the CachedValue to the current parameter value synchronously
            attachedValue->handleAsyncUpdate();
        }
    }
}

void AutomatableParameter::setNormalisedParameter (float value, juce::NotificationType nt)
{
    setParameter (valueRange.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, value)), nt);
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

void AutomatableParameter::updateToFollowCurve (TimePosition time)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    float newModifierValue = 0.0f;
    float newBaseValue = [this, time]
                         {
                             if (hasAutomationPoints() && ! isRecording && curveSource->isEnabledAt (time))
                                 return curveSource->getValueAt (time);

                             return currentParameterValue.load();
                         }();

    getAutomationSourceList()
        .visitSources ([&newBaseValue, &newModifierValue, time] (AutomationModifierSource& m) mutable
                       {
                           if (! m.isEnabledAt (time))
                               return;

                           m.processValueAt (time, newBaseValue, newModifierValue);
                           jassert (! std::isnan (newModifierValue));
                       });

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
    jassert(gestureCount == 0);
    gestureCount++;

    TRACKTION_ASSERT_MESSAGE_THREAD
    listeners.call (&Listener::parameterChangeGestureBegin, *this);

    auto& ed = getEdit();

    if (auto epc = ed.getTransport().getCurrentPlaybackContext())
    {
        if (! epc->isDragging())
        {
            auto& arm = ed.getAutomationRecordManager();

            if (epc->isPlaying() && arm.isWritingAutomation())
            {
                auto time = epc->getPosition();
                auto value = currentValue.load();

                if (! isRecording)
                {
                    isRecording = true;
                    arm.postFirstAutomationChange (*this, value, AutomationTrigger::touch);
                    listeners.call (&Listener::recordingStatusChanged, *this);
                }

                arm.postAutomationChange (*this, time, value);
            }
        }
    }
}

void AutomatableParameter::parameterChangeGestureEnd()
{
    gestureCount--;
    jassert(gestureCount == 0);

    TRACKTION_ASSERT_MESSAGE_THREAD
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
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER
    curveSource->triggerAsyncIteratorUpdate();
    listeners.call (&Listener::curveHasChanged, *this);
}

//==============================================================================
AutomatableParameter::ScopedActiveParameter::ScopedActiveParameter (const AutomatableParameter& p)
    : parameter (p)
{
    // Must increment this before the AutomatableEditElement
    assert (parameter.numActiveAutomationSources >= 0);

    // First increment so add the parameter to the active list
    if (parameter.numActiveAutomationSources.fetch_add (1) == 0)
        parameter.automatableEditElement.addActiveParameter (parameter);

    assert (parameter.automatableEditElement.numActiveParameters >= 0);
    ++parameter.automatableEditElement.numActiveParameters;
}

AutomatableParameter::ScopedActiveParameter::~ScopedActiveParameter()
{
    assert (parameter.numActiveAutomationSources >= 0);

    // Must decrement this before the AutomatableEditElement
    // Last decrement so update the active list
    if (parameter.numActiveAutomationSources.fetch_sub (1) == 1)
        parameter.automatableEditElement.removeActiveParameter (parameter);

    --parameter.automatableEditElement.numActiveParameters;
    assert (parameter.automatableEditElement.numActiveParameters >= 0);
}


//==============================================================================
AutomationMode getAutomationMode (const AutomatableParameter& ap)
{
    if (auto t = ap.getTrack())
        return t->automationMode;

    auto& e = ap.getEdit();
    if (&ap == e.getMasterSliderPosParameter().get() || &ap == e.getMasterPanParameter().get())
        if (auto t = e.getMasterTrack())
            return t->automationMode;

    return AutomationMode::read;
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

    edit.visitAllAutomatableParams (true, [&] (AutomatableParameter& param)
    {
        for (auto ass : param.getAssignments())
        {
            if (ass->isForModifierSource (m))
            {
                jassert (! params.contains (param)); // Being modified by the same source twice?
                params.add (param);
                break;
            }
        }
    });

    return params;
}

AutomatableParameter* getParameter (AutomatableParameter::ModifierAssignment& assignment)
{
    AutomatableParameter* result = nullptr;

    assignment.edit.visitAllAutomatableParams (true, [&] (AutomatableParameter& param)
    {
        for (auto ass : param.getAssignments())
            if (ass == &assignment)
                result = &param;
    });

    return result;
}

//==============================================================================
AutomationIterator::AutomationIterator (Edit& edit, const AutomationCurve& curve)
    : tempoSequence (edit.tempoSequence.getInternalSequence()),
      timeBase (curve.timeBase)
{
    const int numPoints = curve.getNumPoints();
    jassert (numPoints > 0);

    for (int i = 0; i < numPoints; i++)
    {
        auto src = curve.getPoint (i);
        assert (src.time.isBeats() == (timeBase == AutomationCurve::TimeBase::beats));

        AutoPoint dst;
        dst.time = toUnderlying (src.time);
        dst.value = src.value;
        dst.curve = src.curve;

        points.add (dst);
    }
}

AutomationIterator::AutomationIterator (const AutomatableParameter& param)
    : AutomationIterator (param.getEdit(), param.getCurve())
{
}

void AutomationIterator::setPosition (EditPosition newTime) noexcept
{
    jassert (points.size() > 0);

    const double newPostion = [this, &newTime]
                              {
                                  if (timeBase == AutomationCurve::TimeBase::time)
                                      return toTime (newTime, tempoSequence).inSeconds();

                                  return toBeats (newTime, tempoSequence).inBeats();
                              }();

    auto newIndex = updateIndex (newPostion);

    if (newPostion < points[0].time)
    {
        currentIndex = newIndex;
        currentValue = points.getReference (0).value;
        return;
    }

    if (newIndex == points.size() - 1)
    {
        currentIndex = newIndex;
        currentValue = points.getReference (newIndex).value;
        return;
    }

    const auto& p1 = points.getReference (newIndex);
    const auto& p2 = points.getReference (newIndex + 1);

    const auto t = newPostion;

    const auto t1 = p1.time;
    const auto t2 = p2.time;

    const auto v1 = p1.value;
    const auto v2 = p2.value;

    const auto c = p1.curve;

    float v = p2.value;

    if (t2 != t1)
    {
        if (c == 0.0f)
        {
            v = v1 + (v2 - v1) * (float) ((t - t1) / (t2 - t1));
        }
        else if (c >= -0.5 && c <= 0.5)
        {
            auto bp = getBezierPoint (p1.time, p1.value, p2.time, p2.value, p1.curve);
            v = float (getBezierYFromX (t, t1, v1, bp.first, bp.second, t2, v2));
        }
        else
        {
            double x1end = 0, x2end = 0;
            double y1end = 0, y2end = 0;

            auto bp = getBezierPoint (p1.time, p1.value, p2.time, p2.value, p1.curve);
            getBezierEnds (p1.time, p1.value,
                           p2.time, p2.value,
                           p1.curve,
                           x1end, y1end, x2end, y2end);

            if (t >= t1 && t <= x1end)
                v = v1;
            else if (t >= x2end && t <= t2)
                v = v2;
            else
                v = float (getBezierYFromX (t, x1end, y1end, bp.first, bp.second, x2end, y2end));
        }
    }

    currentIndex = newIndex;
    currentValue = v;
}

int AutomationIterator::updateIndex (double newPosition)
{
    auto newIndex = currentIndex;

    if (! juce::isPositiveAndBelow (newIndex, points.size()))
        newIndex = 0;

    if (newIndex > 0 && points.getReference (newIndex).time >= newPosition)
    {
        --newIndex;

        while (newIndex > 0 && points.getReference (newIndex).time >= newPosition)
            --newIndex;
    }
    else
    {
        while (newIndex < points.size() - 1 && points.getReference (newIndex + 1).time < newPosition)
            ++newIndex;
    }

    return newIndex;
}


//==============================================================================
const char* AutomationDragDropTarget::automatableDragString = "automatableParamDrag";

AutomationDragDropTarget::AutomationDragDropTarget() {}
AutomationDragDropTarget::~AutomationDragDropTarget() {}

bool AutomationDragDropTarget::isAutomatableParameterBeingDraggedOver() const
{
     return isAutoParamCurrentlyOver;
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

    auto sourceCompRef = dragSourceDetails.sourceComponent;
    auto thisRef = makeWeakRef (dynamic_cast<juce::Component*> (this));

    if (auto source = dynamic_cast<ParameterisableDragDropSource*> (sourceCompRef.get()))
    {
        source->draggedOntoAutomatableParameterTargetBeforeParamSelection();

        auto handleChosenParam = [sourceCompRef] (AutomatableParameter::Ptr param)
        {
            if (auto src = dynamic_cast<ParameterisableDragDropSource*> (sourceCompRef.get()))
                src->draggedOntoAutomatableParameterTarget (param);
        };

        chooseAutomatableParameter (handleChosenParam,
                                    [thisRef, handleChosenParam]
                                    {
                                        if (auto t = dynamic_cast<AutomationDragDropTarget*> (thisRef.get()))
                                            t->startParameterLearn (handleChosenParam);
                                    });
    }
}

}} // namespace tracktion { inline namespace engine
