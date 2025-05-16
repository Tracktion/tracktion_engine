/*
,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine
{

namespace detail
{
    inline Clip* getClip (const AutomationCurveModifier& acm)
    {
        for (auto v = acm.state.getParent(); v.isValid(); v = v.getParent())
            if (Clip::isClipState (v))
                return findClipForState (acm.edit, v);

        return {};
    }

    inline juce::String getClipName (const AutomationCurveModifier& acm)
    {
        if (auto clip = getClip (acm))
            return clip->getName();

        return {};
    }
}

//==============================================================================
//==============================================================================
juce::String toString (CurveModifierType cmt)
{
    return std::string (magic_enum::enum_name (cmt));
}

std::optional<CurveModifierType> curveModifierTypeFromString (juce::String s)
{
    return magic_enum::enum_cast<CurveModifierType> (s.toStdString());
}

//==============================================================================
//==============================================================================
std::optional<EditPosition> AutomationCurvePlayhead::getPosition() const
{
    return position.load();
}

//==============================================================================
//==============================================================================
AutomationCurveModifier::AutomationCurveModifier (Edit& e,
                                                  const juce::ValueTree& v,
                                                  AutomatableParameterID destID_,
                                                  std::function<CurvePosition()> getPositionDelegate_,
                                                  std::function<ClipPositionInfo()> getClipPositionDelegate_)
    : EditItem (e, v),
      state (v),
      destID (std::move (destID_)),
      getPositionDelegate (std::move (getPositionDelegate_)),
      getClipPositionDelegate (std::move (getClipPositionDelegate_))
{
    auto um = &edit.getUndoManager();

    using enum CurveModifierType;
    using CAT = std::pair<AutomationCurve*, CurveModifierType>;
    for (auto curveAndType : { CAT{ &absoluteCurve, absolute },
                               CAT{ &relativeCurve, relative},
                               CAT{ &scaleCurve, scale } })
    {
        auto& curve = *curveAndType.first;
        curve.setParentState (state);

        auto curveState = getOrCreateChildWithTypeAndProperty (state,
                                                               IDs::AUTOMATIONCURVE,
                                                               IDs::type,
                                                               toString (curveAndType.second),
                                                               um);
        curve.setState (curveState);

        auto& timing = curveTimings[static_cast<size_t> (curveAndType.second)];
        timing.unlinked.referTo (curveState, IDs::unlinked, um);
        timing.start.referTo (curveState, IDs::start, um);
        timing.length.referTo (curveState, IDs::length, um);

        timing.looping.referTo (curveState, IDs::looping, um);
        timing.loopStart.referTo (curveState, IDs::loopStart, um);
        timing.loopLength.referTo (curveState, IDs::loopLength, um);
    }

    auto limitsString = v[IDs::absoluteLimits].toString();
    absoluteLimits = juce::Range<float> (limitsString.upToFirstOccurrenceOf (" ", false, false).getFloatValue(),
                                         limitsString.fromFirstOccurrenceOf (" ", false, false).getFloatValue());

    stateListener.onValueTreeChanged = [this]
    {
        changed();
        listeners.call (&Listener::curveChanged);
    };

    stateListener.onPropertyChanged = [this] (auto& changedTree, auto& id)
    {
        if (id == IDs::unlinked)
        {
            curveUnlinkedStateChanged (changedTree);
        }
        else if (id == IDs::source)
        {
            if (! getUndoManager (edit).isPerformingUndoRedo())
                if (auto oldParam = getParameter (*this))
                    oldParam->removeModifier (*this);

            destID.automatableEditItemID = EditItemID::fromProperty (changedTree, IDs::source);

            if (! getUndoManager (edit).isPerformingUndoRedo())
                if (auto oldParam = getParameter (*this))
                    oldParam->addModifier (*this);
        }
        else if (id == IDs::paramID)
        {
            destID.paramID = changedTree[IDs::paramID].toString();
        }
    };

    stateListener.onChildAdded = [this] (auto&, auto& child)
    {
        if (child.hasType (IDs::POINT) || child.hasType (IDs::AUTOMATIONCURVE))
            updateModifierAssignment();
    };

    stateListener.onChildRemoved = [this] (auto&, auto& child, int)
    {
        if (child.hasType (IDs::POINT) || child.hasType (IDs::AUTOMATIONCURVE))
            updateModifierAssignment();
    };

    edit.automationCurveModifierEditItemCache.addItem (*this);
}

AutomationCurveModifier::~AutomationCurveModifier()
{
    Selectable::notifyListenersOfDeletion();
    edit.automationCurveModifierEditItemCache.removeItem (*this);
}

AutomatableParameterID AutomationCurveModifier::getDestID() const
{
    return destID;
}

bool AutomationCurveModifier::setDestination (AutomatableEditItem& newDest)
{
    if (! newDest.getAutomatableParameterByID (destID.paramID))
        return false;

    auto um = getUndoManager_p (edit);
    newDest.itemID.setProperty (state, IDs::source, um);

    // If the destination has been set but not added by the property change listener,
    // it's probably from a copy/paste or move operation where the destination
    // hasn't changed so add the modifier here
    if (! getUndoManager (edit).isPerformingUndoRedo())
    {
        if (auto param = getParameter (*this))
        {
            for (auto mod : param->getModifiers())
                if (mod == this)
                    return true;

            param->addModifier (*this);
        }
    }

    return true;
}

AutomationCurveModifier::CurveTiming& AutomationCurveModifier::getCurveTiming (CurveModifierType type)
{
    return curveTimings[static_cast<size_t> (type)];
}

AutomationCurveModifier::CurveInfo AutomationCurveModifier::getCurve (CurveModifierType type)
{
    // Update the limits if this is the first time we've seen the parameter
    if (std::exchange (updateLimits, false))
        if (auto param = getParameter (*this))
            absoluteLimits = param->getValueRange();

    using enum CurveModifierType;
    switch (type)
    {
        case absolute:  return { type, absoluteCurve, absoluteLimits };
        case relative:  return { type, relativeCurve, { -0.5f, 0.5f } };
        case scale:     return { type, scaleCurve,    { 0.0f, 1.0f } };
    }

    assert(false);
    unreachable();
}

CurvePosition AutomationCurveModifier::getPosition() const
{
    const std::scoped_lock sl (positionDelegateMutex);
    return getPositionDelegate();
}

ClipPositionInfo AutomationCurveModifier::getClipPositionInfo() const
{
    return getClipPositionDelegate();
}

void AutomationCurveModifier::remove()
{
    state.getParent().removeChild (state, &edit.getUndoManager());
}

//==============================================================================
std::shared_ptr<AutomationCurvePlayhead> AutomationCurveModifier::getPlayhead (CurveModifierType type)
{
    auto& playhead = playheads[static_cast<size_t> (type)];

    if (! playhead)
        playhead = std::make_shared<AutomationCurvePlayhead>();

    return playhead;
}

//==============================================================================
void AutomationCurveModifier::addListener (Listener& l)
{
    listeners.add (&l);
}

void AutomationCurveModifier::removeListener (Listener& l)
{
    listeners.remove (&l);
}

//==============================================================================
juce::String AutomationCurveModifier::getName() const
{
    return TRANS("Clip Automation") + ": " + detail::getClipName (*this);
}

juce::String AutomationCurveModifier::getSelectableDescription()
{
    return getName();
}

//==============================================================================
void AutomationCurveModifier::setPositionDelegate (std::function<CurvePosition()> newDelegate)
{
    assert (newDelegate);
    const std::scoped_lock sl (positionDelegateMutex);
    getPositionDelegate = std::move (newDelegate);
}

//==============================================================================
bool hasAnyPoints (AutomationCurveModifier& curve)
{
    for (auto c : { &curve.getCurve (CurveModifierType::absolute).curve,
                    &curve.getCurve (CurveModifierType::relative).curve,
                    &curve.getCurve (CurveModifierType::scale).curve })
    {
        if (c->getNumPoints() > 0)
            return true;
    }

    return false;
}

void AutomationCurveModifier::curveUnlinkedStateChanged (juce::ValueTree& v)
{
    using enum CurveModifierType;

    for (auto type : { absolute, relative, scale })
    {
        auto& timing = getCurveTiming (type);

        if (v != timing.unlinked.getValueTree())
            continue;

        timing.unlinked.forceUpdateOfCachedValue();
        juce::ErasedScopeGuard listenerCaller ([this, type] { listeners.call (&Listener::unlinkedStateChanged, type); });

        if (timing.unlinked.get())
            return;

        // Copy properties from clip
        auto clipInfo = getClipPositionDelegate();

        timing.start = toPosition (clipInfo.position.offset);
        timing.length = clipInfo.position.position.getLength();

        if (clipInfo.loopRange)
        {
            timing.looping = true;
            timing.loopStart = clipInfo.loopRange->getStart();
            timing.loopLength = clipInfo.loopRange->getLength();
        }
        else
        {
            timing.looping = false;
        }
    }
}

void AutomationCurveModifier::updateModifierAssignment()
{
    if (edit.getUndoManager().isPerformingUndoRedo())
        return;

    if (auto param = getParameter (*this))
    {
        if (hasAnyPoints (*this))
            param->addModifier (*this);
        else
            param->removeModifier (*this);
    }
}


//==============================================================================
AutomationCurveModifier::Assignment::Assignment (AutomationCurveModifier& acm, const juce::ValueTree& v)
    : ModifierAssignment (acm.edit, v),
      automationCurveModifierID (acm.itemID)
{
}

bool AutomationCurveModifier::Assignment::isForModifierSource (const ModifierSource& ms) const
{
    if (auto curveModifier = dynamic_cast<const AutomationCurveModifier*> (&ms))
        return curveModifier->itemID == automationCurveModifierID;

    return false;
}


//==============================================================================
//==============================================================================
namespace
{
    // Returns the base value
    [[nodiscard]] float processAbsoluteValue ([[maybe_unused]] AutomatableParameter& param, float curveValue)
    {
        jassert (! std::isnan (curveValue));
        assert (curveValue >= param.valueRange.start);
        assert (curveValue <= param.valueRange.end);

        return curveValue;
    }

    // Returns the mod value
    [[nodiscard]] float processRelativeValue (float curveValue, std::optional<float> modValue)
    {
        assert (curveValue >= -0.5f);
        assert (curveValue <= 0.5f);

        if (! modValue)
            modValue = 0.0f;

        return *modValue + curveValue;
    }

    [[nodiscard]] BaseAndModValue processScaleValue (AutomatableParameter& param, float curveValue, BaseAndModValue values)
    {
        assert (curveValue >= 0.0f);
        assert (curveValue <= 1.0f);

        if (! values.baseValue)
            values.baseValue = getDefaultValue (param, CurveModifierType::absolute);

        if (! values.modValue)
            values.modValue = 0.0f;

        auto normalisedBase = param.valueRange.convertTo0to1 (values.baseValue.value());
        auto targetNormalisedValue = (normalisedBase + values.modValue.value()) * curveValue;
        values.modValue = targetNormalisedValue - normalisedBase;

        return values;
    }
}

float getDefaultValue (AutomatableParameter& param, CurveModifierType type)
{
    using enum CurveModifierType;
    switch (type)
    {
        case absolute:  return param.getCurrentBaseValue();
        case relative:  return 0.0f;
        case scale:     return 1.0f;
    };

    unreachable();
}

BaseAndModValue getValuesAtEditPosition (AutomationCurveModifier& acm, AutomatableParameter& param, EditPosition editPos)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    BaseAndModValue values;

    auto& ts = getTempoSequence (acm);
    auto acmPos = acm.getPosition();

    using enum CurveModifierType;

    for (auto type : { absolute, relative, scale })
    {
        auto curveInfo = acm.getCurve (type);

        if (curveInfo.curve.getNumPoints() == 0)
            continue;

        EditPosition curvePos = editPos;
        auto& curveTiming = acm.getCurveTiming (type);

        if (! contains (acmPos.clipRange, editPos, ts))
            continue;

        if (curveTiming.unlinked.get())
            curvePos = minus (editPos, toDuration (acmPos.clipRange.getStart()), ts);
        else
            curvePos = minus (editPos, toDuration (acmPos.curveStart), ts);

        if (auto adjustedPos = applyTimingToCurvePosition (acm, type, curvePos))
            curvePos = *adjustedPos;
        else
            continue;

        auto curveValue = curveInfo.curve.getValueAt (curvePos, getDefaultValue (param, curveInfo.type));

        if (type == absolute)
        {
            values.baseValue = processAbsoluteValue (param, curveValue);
        }
        else if (type == relative)
        {
            values.modValue = processRelativeValue (curveValue, values.modValue);
        }
        else if (type == scale)
        {
            values = processScaleValue (param, curveValue, values);
        }
    }

    return values;
}

BaseAndModValue getValuesAtCurvePosition (AutomationCurveModifier& acm, AutomatableParameter& param, EditPosition curvePos)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    BaseAndModValue values;

    using enum CurveModifierType;

    for (auto type : { absolute, relative, scale })
    {
        auto curveInfo = acm.getCurve (type);

        if (curveInfo.curve.getNumPoints() == 0)
            continue;

        if (auto adjustedPos = applyTimingToCurvePosition (acm, type, curvePos))
            curvePos = *adjustedPos;
        else
            continue;

        auto curveValue = curveInfo.curve.getValueAt (curvePos, getDefaultValue (param, curveInfo.type));

        if (type == absolute)
        {
            values.baseValue = processAbsoluteValue (param, curveValue);
        }
        else if (type == relative)
        {
            values.modValue = processRelativeValue (curveValue, values.modValue);
        }
        else if (type == scale)
        {
            values = processScaleValue (param, curveValue, values);
        }
    }

    return values;
}

BaseAndModValue getValuesAtCurvePositionUnlooped (AutomationCurveModifier& acm, AutomatableParameter& param, EditPosition pos)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    BaseAndModValue values;

    using enum CurveModifierType;

    for (auto type : { absolute, relative, scale })
    {
        auto curveInfo = acm.getCurve (type);

        if (curveInfo.curve.getNumPoints() == 0)
            continue;

        auto curveValue = curveInfo.curve.getValueAt (pos, getDefaultValue (param, curveInfo.type));

        if (type == absolute)
        {
            values.baseValue = processAbsoluteValue (param, curveValue);
        }
        else if (type == relative)
        {
            values.modValue = processRelativeValue (curveValue, values.modValue);
        }
        else if (type == scale)
        {
            values = processScaleValue (param, curveValue, values);
        }
    }

    return values;
}

std::optional<EditPosition> applyTimingToCurvePosition (AutomationCurveModifier& acm, CurveModifierType type, EditPosition curvePos)
{
    auto& timing = acm.getCurveTiming (type);
    auto& ts = getTempoSequence (acm).getInternalSequence();

    // Unlinked means timing starts from the start of the curve, ignoring the clip
    if (timing.unlinked.get())
    {
        // Apply start offset first
        auto posBeats = toBeats (curvePos, ts)
                            + toDuration (timing.start.get());

        if (timing.looping.get())
        {
            auto loopStart = timing.loopStart.get();

            if (posBeats > loopStart)
            {
                auto posRelToLoopStart = posBeats - loopStart;
                auto moduloPosRelToLoopStart = std::fmod (toUnderlying (posRelToLoopStart),
                                                          toUnderlying (timing.loopLength.get()));

                posBeats = BeatPosition::fromBeats (moduloPosRelToLoopStart)
                            + toDuration (loopStart);
            }
        }
        else
        {
            // Stop after end
            auto end = timing.start.get() + timing.length.get();

            if (posBeats >= end)
                return std::nullopt;
        }

        curvePos = posBeats;
    }
    else if (auto clipPosInfo = acm.getClipPositionInfo(); clipPosInfo.loopRange)
    {
        // Use clip looping
        auto loopStart = clipPosInfo.loopRange->getStart();

        auto posBeats = toBeats (curvePos, ts);

        if (posBeats > loopStart)
        {
            auto posRelToLoopStart = posBeats - loopStart;
            auto moduloPosRelToLoopStart = std::fmod (toUnderlying (posRelToLoopStart),
                                                      toUnderlying (clipPosInfo.loopRange->getLength()));

            posBeats = BeatPosition::fromBeats (moduloPosRelToLoopStart)
                        + toDuration (loopStart);
        }

        curvePos = posBeats;
    }

    return curvePos;
}

std::optional<EditPosition> editPositionToCurvePosition (AutomationCurveModifier& acm, CurveModifierType type, EditPosition editPos)
{
    auto& ts = getTempoSequence (acm).getInternalSequence();
    auto curvePosition = acm.getPosition();
    auto editBeatPos = toBeats (editPos, ts);

    if (! toBeats (curvePosition.clipRange, ts).contains (editBeatPos))
        return std::nullopt;

    auto posRelativeToCurvePosition = acm.getCurveTiming (type).unlinked.get()
            ? editBeatPos - toDuration (toBeats (curvePosition.clipRange.getStart(), ts))
            : editBeatPos - toDuration (toBeats (curvePosition.curveStart, ts));

    return applyTimingToCurvePosition (acm, type, posRelativeToCurvePosition);
}

namespace detail
{
    void processValue (AutomatableParameter& param, CurveModifierType modType, float curveValue, float& baseValue, float& modValue)
    {
        using enum CurveModifierType;
        jassert (! std::isnan (curveValue));

        switch (modType)
        {
            case absolute:
                assert (curveValue >= param.valueRange.start);
                assert (curveValue <= param.valueRange.end);
                baseValue = curveValue;
            break;
            case relative:
                assert (curveValue >= -0.5f);
                assert (curveValue <= 0.5f);
                modValue += curveValue;
            break;
            case scale:
                assert (curveValue >= 0.0f);
                assert (curveValue <= 1.0f);
                auto normalisedBase = param.valueRange.convertTo0to1 (baseValue);
                auto targetNormalisedValue = (normalisedBase + modValue) * curveValue;
                modValue = targetNormalisedValue - normalisedBase;
            break;
        }
    }
}


//==============================================================================
//==============================================================================
AutomatableParameter::Ptr getParameter (const AutomationCurveModifier& acm)
{
    auto destID = acm.getDestID();

    if (auto foundItem = acm.edit.automatableEditItemCache.findItem (destID.automatableEditItemID))
        return foundItem->getAutomatableParameterByID (destID.paramID);

    return nullptr;
}

AutomationCurveModifier::Ptr getAutomationCurveModifierForID (Edit& edit, EditItemID automationCurveModifierID)
{
    if (auto found = edit.automationCurveModifierEditItemCache.findItem (automationCurveModifierID))
        return *found;

    return nullptr;
}

//==============================================================================
namespace detail
{
    inline bool areEquivalentPlugins (Plugin& p1, Plugin& p2)
    {
        auto et1 = dynamic_cast<ExternalPlugin*> (&p1);
        auto et2 = dynamic_cast<ExternalPlugin*> (&p2);

        if (et1 && et2)
            return et1->desc.isDuplicateOf (et2->desc);

        return typeid (p1) == typeid (p2);
    }
}

void updateRelativeDestinationOrRemove ([[maybe_unused]] AutomationCurveList& list, AutomationCurveModifier& curve, Clip& clip)
{
    if (getUndoManager (curve).isPerformingUndoRedo())
        return;

    assert (contains_v (list.getItems(), &curve));
    auto oldParam = getParameter (curve);

    if (! oldParam)
        return;

    // Clip
    // Rack
    // Master
    // Folder
    // Track
    auto oldPlugin = oldParam->getPlugin();

    if (! oldPlugin)
    {
        // Old plugin doesn't exist anymore so we can't copy from it, so remove the curve
        curve.remove();
        return;
    }

    // If the curve is targeted to a plugin on a different clip,
    // update it to the new one if possible
    if (oldPlugin->getOwnerClip() != nullptr)
        if (auto pluginList = clip.getPluginList())
            for (auto newPlugin : *pluginList)
                if (detail::areEquivalentPlugins (*oldPlugin, *newPlugin))
                    if (curve.setDestination (*newPlugin))
                        return;

    // If the plugin is on the same clip or on a RackType, keep it
    if (oldPlugin->getOwnerClip() == &clip
        || oldPlugin->getOwnerRackType())
    {
        return;
    }

    auto oldTrack = oldPlugin->getOwnerTrack();

    // Keep connection to master plugins or folder tracks
    if (dynamic_cast<MasterTrack*> (oldTrack) != nullptr
        || dynamic_cast<FolderTrack*> (oldTrack) != nullptr)
    {
        jassert (&dynamic_cast<MasterTrack*> (oldTrack)->pluginList == oldPlugin->getOwnerList());
        return;
    }

    // Curve was on a plugin on a track so look for an appropriate replacement
    if (auto newTrack = clip.getTrack(); newTrack)
        for (auto newPlugin : newTrack->pluginList)
            if (detail::areEquivalentPlugins (*oldPlugin, *newPlugin))
                if (curve.setDestination (*newPlugin))
                    return;

    // No corresponding parameter so remove the curve
    curve.remove();
}

void assignNewIDsToAutomationCurveModifiers (Edit& edit, juce::ValueTree& state)
{
    if (state.hasType (IDs::AUTOMATIONCURVE))
        return;

    for (auto v : state)
    {
        assignNewIDsToAutomationCurveModifiers (edit, v);

        if (v.hasType (IDs::AUTOMATIONCURVEMODIFIER))
            edit.createNewItemID().writeID (v, nullptr);
    }
}

void removeInvalidAutomationCurveModifiers (juce::ValueTree& state, const AutomatableParameter& parameter)
{
    assert (state.hasType (IDs::MODIFIERASSIGNMENTS));

    for (int i = state.getNumChildren(); --i >= 0;)
    {
        auto v = state.getChild (i);

        if (auto curveModifier = getAutomationCurveModifierForID (parameter.getEdit(), EditItemID::fromProperty (v, IDs::source)))
        {
            // Only remove curves relevant to this parameter or the below code won't be valid
            if (curveModifier->getDestID().paramID != parameter.paramID)
                continue;

            auto assignedParam = getParameter (*curveModifier);

            // Could be an ExternalPlugin that failed to load?
            // Leave alone for now
            if (! assignedParam)
                continue;

            if (&parameter == assignedParam)
                continue;

            state.removeChild (v, nullptr);
        }
    }
}

//==============================================================================
//==============================================================================
class AutomationCurveList::List : private ValueTreeObjectList<AutomationCurveModifier>
{
public:
    List (AutomationCurveList& o, Edit& e,
          std::function<CurvePosition()> getPositionDelegate_,
          std::function<ClipPositionInfo()> getClipPositionDelegate_,
          const juce::ValueTree& parent_)
        : ValueTreeObjectList<AutomationCurveModifier> (parent_),
          curveList (o), edit (e),
          getPositionDelegate (std::move (getPositionDelegate_)),
          getClipPositionDelegate (std::move (getClipPositionDelegate_))
    {
        assert (parent.hasType (IDs::AUTOMATIONCURVES));
        assert (getPositionDelegate);
        assert (getClipPositionDelegate);

        rebuildObjects();
    }

    ~List() override
    {
        freeObjects();
    }

    std::vector<AutomationCurveModifier::Ptr> getItems()
    {
        return { objects.begin(), objects.end() };
    }

    AutomationCurveModifier::Ptr addCurve (const AutomatableParameter& destParam)
    {
        auto limits = destParam.getValueRange();
        auto limitsString  = juce::String (limits.getStart()) + " " + juce::String (limits.getEnd());

        juce::ValueTree v (IDs::AUTOMATIONCURVEMODIFIER,
                           {
                               { IDs::source, destParam.automatableEditElement.itemID.toString() },
                               { IDs::paramID, destParam.paramID },
                               { IDs::absoluteLimits, limitsString }
                           } );
        parent.appendChild (v, &edit.getUndoManager());
        auto added = objects.getLast();
        assert (added->state == v);

        return added;
    }

    void removeCurve (int idx)
    {
        parent.removeChild (idx, &edit.getUndoManager());
    }

private:
    AutomationCurveList& curveList;
    Edit& edit;
    std::function<CurvePosition()> getPositionDelegate;
    std::function<ClipPositionInfo()> getClipPositionDelegate;

    //==============================================================================
    bool isSuitableType (const juce::ValueTree& v) const override
    {
        return v.hasType (IDs::AUTOMATIONCURVEMODIFIER);
    }

    AutomationCurveModifier* createNewObject (const juce::ValueTree& v) override
    {
        assert (edit.automationCurveModifierEditItemCache.findItem (EditItemID::fromID (v)) == nullptr);

        auto automatableEditItemID = EditItemID::fromProperty (v, IDs::source);
        auto paramID = v[IDs::paramID].toString();
        auto destID = AutomatableParameterID { automatableEditItemID, paramID };

        auto newCurveMod = new AutomationCurveModifier (edit, v, destID, getPositionDelegate, getClipPositionDelegate);
        newCurveMod->incReferenceCount();

        return newCurveMod;
    }

    void deleteObject (AutomationCurveModifier* c) override
    {
        assert (c != nullptr);
        c->decReferenceCount();
    }

    void newObjectAdded (AutomationCurveModifier* e) override
    {
        if (! edit.getUndoManager().isPerformingUndoRedo())
            if (auto param = getParameter (*e))
                param->addModifier (*e);

        curveList.listeners.call (&AutomationCurveList::Listener::itemsChanged);
    }

    void objectRemoved (AutomationCurveModifier* e) override
    {
        if (! edit.getUndoManager().isPerformingUndoRedo())
            if (auto param = getParameter (*e))
                param->removeModifier (*e);

        curveList.listeners.call (&AutomationCurveList::Listener::itemsChanged);
    }

    void objectOrderChanged() override
    {
        curveList.listeners.call (&AutomationCurveList::Listener::itemsChanged);
    }
};


//==============================================================================
//==============================================================================
AutomationCurveList::AutomationCurveList (Edit& e, const juce::ValueTree& parentTree,
                                          std::function<CurvePosition()> getPositionDelegate,
                                          std::function<ClipPositionInfo()> getClipPositionDelegate)
{
    list = std::make_unique<List> (*this, e,
                                   std::move (getPositionDelegate),
                                   std::move (getClipPositionDelegate),
                                   parentTree);
}

AutomationCurveList::~AutomationCurveList()
{
}

std::vector<AutomationCurveModifier::Ptr> AutomationCurveList::getItems()
{
    return list->getItems();
}

AutomationCurveModifier::Ptr AutomationCurveList::addCurve (const AutomatableParameter& destParam)
{
    return list->addCurve (destParam);
}

void AutomationCurveList::removeCurve (int idx)
{
    list->removeCurve (idx);
}

void AutomationCurveList::addListener (Listener& l)
{
    listeners.add (&l);
}

void AutomationCurveList::removeListener (Listener& l)
{
    listeners.remove (&l);
}

} // namespace tracktion::inline engine
