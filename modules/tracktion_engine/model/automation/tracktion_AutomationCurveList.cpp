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

AutomationCurveModifier::AutomationCurveModifier (Edit& e,
                                                  const juce::ValueTree& v,
                                                  AutomatableParameterID destID_,
                                                  std::function<CurvePosition()> getPositionDelegate_)
    : EditItem (e, v),
      state (v),
      destID (std::move (destID_)),
      curve (edit, AutomationCurve::TimeBase::beats),
      getPositionDelegate (std::move (getPositionDelegate_))
{
    curve.setParentState (v);
    curve.setState (state.getOrCreateChildWithName (IDs::AUTOMATIONCURVE, &edit.getUndoManager()));
    curve.setParameterID (destID.paramID);

    edit.automationCurveModifierEditItemCache.addItem (*this);
}

AutomationCurveModifier::~AutomationCurveModifier()
{
    edit.automationCurveModifierEditItemCache.removeItem (*this);
}

AutomationCurve& AutomationCurveModifier::getCurve()
{
    return curve;
}

CurvePosition AutomationCurveModifier::getPosition() const
{
    return getPositionDelegate();
}

void AutomationCurveModifier::remove()
{
    state.getParent().removeChild (state, &edit.getUndoManager());
}

juce::String AutomationCurveModifier::getName() const
{
    return TRANS("Automation Curve Modifier");
}

juce::String AutomationCurveModifier::getSelectableDescription()
{
    return getName();
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
                         std::function<CurvePosition()> getPositionDelegate_,
                         const juce::ValueTree& parent_)
        : ValueTreeObjectList<AutomationCurveModifier> (parent_),
          curveList (o), edit (e),
          getPositionDelegate (std::move (getPositionDelegate_))
    {
        assert (parent.hasType (IDs::AUTOMATIONCURVES));
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
                                          std::function<CurvePosition()> getPositionDelegate)
{
    list = std::make_unique<List> (*this, e, std::move (getPositionDelegate), parentTree);
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
