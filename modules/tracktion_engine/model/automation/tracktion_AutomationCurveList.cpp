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
AutomationCurveModifier::AutomationCurveModifier (Edit& e,
                                                  const juce::ValueTree& v,
                                                  AutomatableParameterID destID_,
                                                  std::function<CurvePosition()> getPositionDelegate_)
    : EditItem (e, v),
      state (v),
      destID (std::move (destID_)),
      getPositionDelegate (std::move (getPositionDelegate_))
{
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
                                                               &edit.getUndoManager());
        curve.setState (curveState);

        curve.setParameterID (destID.paramID);
    }

    edit.automationCurveModifierEditItemCache.addItem (*this);
}

AutomationCurveModifier::~AutomationCurveModifier()
{
    Selectable::notifyListenersOfDeletion();
    edit.automationCurveModifierEditItemCache.removeItem (*this);
}

AutomationCurveModifier::CurveInfo AutomationCurveModifier::getCurve (CurveModifierType type)
{
    auto getParameterLimits = [this]
    {
        if (auto param = getParameter (*this))
            return param->getValueRange();

        return juce::Range<float>();
    };

    using enum CurveModifierType;
    switch (type)
    {
        case absolute:  return { type, absoluteCurve, getParameterLimits() };
        case relative:  return { type, relativeCurve, { -0.5f, 0.5f } };
        case scale:     return { type, scaleCurve,    { 0.0f, 1.0f } };
    }

    assert(false);
    unreachable();
}

CurvePosition AutomationCurveModifier::getPosition() const
{
    return getPositionDelegate();
}

void AutomationCurveModifier::remove()
{
    state.getParent().removeChild (state, &edit.getUndoManager());
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
    return TRANS("Automation Curve Modifier");
}

juce::String AutomationCurveModifier::getSelectableDescription()
{
    return getName();
}

//==============================================================================
void AutomationCurveModifier::callPositionChangedListeners()
{
    listeners.call (&Listener::positionChanged);
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

    return true;
}


//==============================================================================
//==============================================================================
namespace
{
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

BaseAndModValue getValuesAtEditPosition (AutomationCurveModifier& acm, AutomatableParameter& param, EditPosition editPos)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    auto& ts = getTempoSequence (acm);
    auto acmPos = acm.getPosition();

    if (! contains (acmPos.clipRange, editPos, ts))
        return {};

    auto curvePos = toPosition (minus (editPos, acmPos.curveStart, ts));
    return getValuesAtCurvePosition (acm, param, curvePos);
}

BaseAndModValue getValuesAtCurvePosition (AutomationCurveModifier& acm, AutomatableParameter& param, EditPosition pos)
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
    if (auto foundItem = acm.edit.automatableEditItemCache.findItem (acm.destID.automatableEditItemID))
        return foundItem->getAutomatableParameterByID (acm.destID.paramID);

    return nullptr;
}

AutomationCurveModifier::Ptr getAutomationCurveModifierForID (Edit& edit, EditItemID automationCurveModifierID)
{
    if (auto found = edit.automationCurveModifierEditItemCache.findItem (automationCurveModifierID))
        return *found;

    return nullptr;
}

//==============================================================================
//==============================================================================
class AutomationCurveList::List : private ValueTreeObjectList<AutomationCurveModifier>
{
public:
    List (AutomationCurveList& o, Edit& e,
          ValueTreePropertyChangedListener positionChangedCallback_,
          std::function<CurvePosition()> getPositionDelegate_,
          const juce::ValueTree& parent_)
        : ValueTreeObjectList<AutomationCurveModifier> (parent_),
          curveList (o), edit (e),
          positionChangedCallback (std::move (positionChangedCallback_)),
          getPositionDelegate (std::move (getPositionDelegate_))
    {
        assert (parent.hasType (IDs::AUTOMATIONCURVES));
        rebuildObjects();

        positionChangedCallback.onPropertyChanged = [this] (auto)
        {
            for (auto object : objects)
                object->callPositionChangedListeners();
        };
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
        juce::ValueTree v (IDs::AUTOMATIONCURVEMODIFIER,
                           {
                               { IDs::source, destParam.automatableEditElement.itemID.toString() },
                               { IDs::paramID, destParam.paramID }
                           } );
        parent.appendChild (v, &edit.getUndoManager());
        auto added = objects.getLast();
        assert (added->state == v);

        return added;
    }

private:
    AutomationCurveList& curveList;
    Edit& edit;
    ValueTreePropertyChangedListener positionChangedCallback;
    std::function<CurvePosition()> getPositionDelegate;

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

        auto newCurveMod = new AutomationCurveModifier (edit, v, destID, getPositionDelegate);
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
        if (auto param = getParameter (*e))
            param->addModifier (*e);

        curveList.listeners.call (&AutomationCurveList::Listener::itemsChanged);
    }

    void objectRemoved (AutomationCurveModifier* e) override
    {
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
                                          ValueTreePropertyChangedListener sourcePropertyChangeListener,
                                          std::function<CurvePosition()> getPositionDelegate)
{
    list = std::make_unique<List> (*this, e,
                                   std::move (sourcePropertyChangeListener),
                                   std::move (getPositionDelegate),
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

void AutomationCurveList::addListener (Listener& l)
{
    listeners.add (&l);
}

void AutomationCurveList::removeListener (Listener& l)
{
    listeners.remove (&l);
}

} // namespace tracktion::inline engine
