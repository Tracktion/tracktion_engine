/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


Modifier::Modifier (Edit& e, const juce::ValueTree& v)
    : AutomatableEditItem (e, v),
      state (v)
{
    auto um = &edit.getUndoManager();
    colour.referTo (state, IDs::colour, um, Colours::red.withHue (1.0f / 9.0f));
    enabled.referTo (state, IDs::enabled, um, true);

    enabledParam = new DiscreteLabelledParameter ("enabled", TRANS("Enabled"), *this, { 0.0f, 1.0f },
                                                  getEnabledNames().size(), getEnabledNames());
    addAutomatableParameter (enabledParam);
    enabledParam->attachToCurrentValue (enabled);

    if (remapOnTempoChange.isUsingDefault())
        remapOnTempoChange = true;
}

Modifier::~Modifier()
{
    // Must remove parameters in sub-class!
    jassert (getNumAutomatableParameters() == 0);
}

void Modifier::remove()
{
    deselect();

    for (auto p : getAllParametersBeingModifiedBy (edit, *this))
        p->removeModifier (*this);

    for (auto t : getAllTracks (edit))
        t->hideAutomatableParametersForSource (itemID);

    auto um = &edit.getUndoManager();
    auto rack = state.getParent().getParent();

    state.getParent().removeChild (state, um);

    if (rack.hasType (IDs::RACK))
        RackType::removeBrokenConnections (rack, um);
}

//==============================================================================
void Modifier::baseClassInitialise (const PlaybackInitialisationInfo& info)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    if (initialiseCount++ == 0)
    {
        CRASH_TRACER
        initialise (info);
    }

    CRASH_TRACER
    resetRecordingStatus();
}

void Modifier::baseClassDeinitialise()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    jassert (initialiseCount > 0);

    if (--initialiseCount == 0)
    {
        CRASH_TRACER
        deinitialise();
        resetRecordingStatus();
    }
}

StringArray Modifier::getEnabledNames()
{
    return { NEEDS_TRANS("Disabled"),
             NEEDS_TRANS("Enabled") };
}

//==============================================================================
ModifierList::ModifierList (Edit& e, const ValueTree& parent)
    : ValueTreeObjectList<Modifier> (parent),
      edit (e), state (parent)
{
    jassert (parent.hasType (IDs::MODIFIERS));
    callBlocking ([this] { rebuildObjects(); });
}

ModifierList::~ModifierList()
{
    freeObjects();
}

bool ModifierList::isModifier (const Identifier& i)
{
    return i == IDs::LFO || i == IDs::BREAKPOINTOSCILLATOR
        || i == IDs::STEP || i == IDs::ENVELOPEFOLLOWER
        || i == IDs::RANDOM || i == IDs::MIDITRACKER;
}

ReferenceCountedArray<Modifier> ModifierList::getModifiers() const
{
    if (! edit.isLoading())
        TRACKTION_ASSERT_MESSAGE_THREAD

    ReferenceCountedArray<Modifier> mods;

    for (auto m : objects)
        mods.add (m);

    return mods;
}

//==============================================================================
ReferenceCountedObjectPtr<Modifier> ModifierList::insertModifier (ValueTree v, int index, SelectionManager* sm)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    jassert (isSuitableType (v));
    auto um = &edit.getUndoManager();

    if (v.getParent().isValid())
        v.getParent().removeChild (v, um);

    const auto existing = objects[index];
    const int indexBefore = existing == nullptr ? -1 : parent.indexOf (existing->state);
    parent.addChild (v, indexBefore, um);

    auto* modifier = [&]() -> Modifier*
                     {
                         for (auto o : objects)
                             if (o->state == v)
                                 return o;

                         return {};
                     }();

    jassert (modifier != nullptr);

    if (sm != nullptr)
        sm->selectOnly (modifier);

    return modifier;
}

//==============================================================================
bool ModifierList::isSuitableType (const juce::ValueTree& v) const
{
    return isModifier (v.getType());
}

Modifier* ModifierList::createNewObject (const ValueTree& v)
{
    CRASH_TRACER
    auto m = findModifierForID (edit, EditItemID::readOrCreateNewID (edit, v));

    if (m == nullptr)
    {
        if (v.hasType (IDs::LFO))                         m = new LFOModifier (edit, v);
        else if (v.hasType (IDs::BREAKPOINTOSCILLATOR))   m = new BreakpointOscillatorModifier (edit, v);
        else if (v.hasType (IDs::STEP))                   m = new StepModifier (edit, v);
        else if (v.hasType (IDs::ENVELOPEFOLLOWER))       m = new EnvelopeFollowerModifier (edit, v);
        else if (v.hasType (IDs::RANDOM))                 m = new RandomModifier (edit, v);
        else if (v.hasType (IDs::MIDITRACKER))            m = new MIDITrackerModifier (edit, v);
        else                                              jassertfalse;

        m->initialise();
    }

    m->incReferenceCount();

    return m.get();
}

void ModifierList::deleteObject (Modifier* m)
{
    jassert (m != nullptr);
    m->decReferenceCount();
}

void ModifierList::newObjectAdded (Modifier*)   { sendChangeMessage(); }
void ModifierList::objectRemoved (Modifier*)    { sendChangeMessage(); }
void ModifierList::objectOrderChanged()         { sendChangeMessage(); }

//==============================================================================
Modifier::Ptr findModifierForID (ModifierList& ml, EditItemID modifierID)
{
    for (auto mod : ml.getModifiers())
        if (mod->itemID == modifierID)
            return mod;

    return {};
}
