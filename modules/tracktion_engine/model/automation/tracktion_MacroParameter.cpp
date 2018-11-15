/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


MacroParameter::Assignment::Assignment (const ValueTree& v, const MacroParameter& mp)
    : AutomatableParameter::ModifierAssignment (mp.edit, v),
      macroParamID (EditItemID::fromVar (mp.paramID))
{
}

bool MacroParameter::Assignment::isForModifierSource (const AutomatableParameter::ModifierSource& source) const
{
    // NB: the paramID is a string that can be any format, but for macro params, we use it to hold an EditItemID
    if (auto* mp = dynamic_cast<const MacroParameter*> (&source))
        return mp->paramID == macroParamID.toString();

    return false;
}

//==============================================================================
MacroParameter::MacroParameter (AutomatableEditItem& automatable, Edit& e, const ValueTree& v)
    : AutomatableParameter (EditItemID::fromID (v).toString(),
                            EditItemID::fromID (v).toString(),
                            automatable, { 0.0f, 1.0f }),
      edit (e), state (v)
{
    jassert (EditItemID::fromID (v).isValid());

    value.referTo (state, IDs::value, &edit.getUndoManager(), 0.5f);
    macroName.referTo (state, IDs::name, &edit.getUndoManager());
    attachToCurrentValue (value);
}

MacroParameter::~MacroParameter()
{
    detachFromCurrentValue();
}

void MacroParameter::initialise()
{
    jassert (! edit.isLoading());
}

void MacroParameter::parameterChanged (float, bool byAutomation)
{
    // Update any non-active parameters
    if (byAutomation)
        return;

    TRACKTION_ASSERT_MESSAGE_THREAD
    auto cursorPos = edit.getTransport().getCurrentPosition();

    for (auto ap : getAllParametersBeingModifiedBy (edit, *this))
        if (! ap->automatableEditElement.isBeingActivelyPlayed())
            ap->updateFromAutomationSources (cursorPos);
}

//==============================================================================
MacroParameter::Ptr getMacroParameterForID (Edit& e, EditItemID pid)
{
    for (auto mpl : getAllMacroParameterLists (e))
        for (auto mp : mpl->getMacroParameters())
            if (EditItemID::fromVar (mp->paramID) == pid)
                return mp;

    return {};
}

//==============================================================================
struct MacroParameterList::List : public ValueTreeObjectList<MacroParameter>
{
    List (MacroParameterList& mpl, const ValueTree& v)
        : ValueTreeObjectList<MacroParameter> (v),
          macroParameterList (mpl), edit (mpl.edit)
    {
        jassert (v.hasType (IDs::MACROPARAMETERS));
        rebuildObjects();
    }

    ~List()
    {
        freeObjects();
    }

    ReferenceCountedArray<MacroParameter> getMacroParameters() const
    {
        return macroParameters;
    }

    MacroParameterList& macroParameterList;
    Edit& edit;

private:
    ReferenceCountedArray<MacroParameter, CriticalSection> macroParameters;

    bool isSuitableType (const juce::ValueTree& v) const override
    {
        return v.hasType (IDs::MACROPARAMETER);
    }

    MacroParameter* createNewObject (const juce::ValueTree& v) override
    {
        auto* mp = new MacroParameter (macroParameterList, edit, v);
        macroParameters.add (mp);
        macroParameterList.addAutomatableParameter (mp);
        return mp;
    }

    void deleteObject (MacroParameter* mp) override
    {
        jassert (mp != nullptr);
        macroParameterList.deleteParameter (mp);
        macroParameters.removeObject (mp);
    }

    void newObjectAdded (MacroParameter*) override  { macroParameterList.sendChangeMessage(); }

    void objectRemoved (MacroParameter* mp) override
    {
        for (auto p : getAllParametersBeingModifiedBy (edit, *mp))
            p->removeModifier (*mp);

        for (auto t : getAllTracks (edit))
            t->hideAutomatableParametersForSource (EditItemID::fromVar (mp->paramID));

        macroParameterList.sendChangeMessage();
    }

    void objectOrderChanged() override              { macroParameterList.sendChangeMessage(); }

    void valueTreePropertyChanged (ValueTree& v, const Identifier& i) override
    {
        if (v.hasType (IDs::MACROPARAMETER) && i == IDs::name)
            macroParameterList.rebuildParameterTree();

        ValueTreeObjectList<MacroParameter>::valueTreePropertyChanged (v, i);
    }
};

//==============================================================================
MacroParameterList::MacroParameterList (Edit& e, const ValueTree& v)
    : AutomatableEditItem (e, v),
      state (v)
{
    jassert (state.hasType (IDs::MACROPARAMETERS));
    jassert (itemID.isValid());
    list = std::make_unique<List> (*this, v);

    restoreChangedParametersFromState();
}

MacroParameterList::~MacroParameterList()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
}

MacroParameter* MacroParameterList::createMacroParameter()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    auto* um = &edit.getUndoManager();

    ValueTree v (IDs::MACROPARAMETER);
    edit.createNewItemID().writeID (v, nullptr);
    state.addChild (v, -1, um);

    auto* mp = list->objects.getLast();
    mp->macroName = TRANS("Macro") + " " + String (list->objects.size());
    jassert (mp != nullptr);
    jassert (mp->state == v);
    sendChangeMessage();

    return mp;
}

void MacroParameterList::removeMacroParameter (MacroParameter& mp)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    jassert (list != nullptr);
    auto* um = &edit.getUndoManager();

    // Remove the child from the parent
    if (mp.state.getParent() == state)
        state.removeChild (mp.state, um);
    else
        jassertfalse;

    sendChangeMessage();
}

void MacroParameterList::hideMacroParametersFromTracks() const
{
    if (! edit.isLoading())
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
    }

    for (auto mp : getMacroParameters())
    {
        auto paramID = EditItemID::fromVar (mp->paramID);

        for (auto t : getAllTracks (edit))
            t->hideAutomatableParametersForSource (paramID);
    }
}

ReferenceCountedArray<MacroParameter> MacroParameterList::getMacroParameters() const
{
    return list->getMacroParameters();
}

Track* MacroParameterList::getTrack() const
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    for (auto p (state.getParent()); p.isValid(); p = p.getParent())
        if (TrackList::isTrack (p))
            return findTrackForState (edit, p);

    return {};
}

Plugin::Ptr getOwnerPlugin (MacroParameterList* mpl)
{
    if (mpl == nullptr)
        return {};

    for (auto af : getAllPlugins (mpl->edit, false))
        if (af->getNumMacroParameters() > 0 && &af->macroParameterList == mpl)
            return af;

    return {};
}

//==============================================================================
MacroParameterElement::MacroParameterElement (Edit& e, const ValueTree& v)
    : macroParameterList (e, ValueTree (v).getOrCreateChildWithName (IDs::MACROPARAMETERS, &e.getUndoManager()))
{
}
