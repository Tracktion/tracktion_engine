/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct MIDITrackerModifier::ModifierAudioNode    : public SingleInputAudioNode
{
    ModifierAudioNode (AudioNode* input, MIDITrackerModifier& mtm)
        : SingleInputAudioNode (input),
          modifier (&mtm)
    {
    }

    void renderOver (const AudioRenderContext& rc) override
    {
        SingleInputAudioNode::renderOver (rc);
        modifier->applyToBuffer (rc);
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        callRenderOver (rc);
    }

    MIDITrackerModifier::Ptr modifier;
};

//==============================================================================
MIDITrackerModifier::MIDITrackerModifier (Edit& e, const ValueTree& v)
    : Modifier (e, v)
{
    auto um = &edit.getUndoManager();

    nodeState = state.getOrCreateChildWithName (IDs::NODES, um);

    type.referTo (state, IDs::wave, um, pitch);
    mode.referTo (state, IDs::mode, um, absolute);
    relativeRoot.referTo (state, IDs::root, um, 60);
    relativeSpread.referTo (state, IDs::spread, um, 12);

    auto addDiscreteParam = [this] (const String& paramID, const String& name, Range<float> valueRange, CachedValue<float>& val,
                                    const StringArray& labels) -> AutomatableParameter*
    {
        auto* p = new DiscreteLabelledParameter (paramID, name, *this, valueRange, labels.size(), labels);
        addAutomatableParameter (p);
        p->attachToCurrentValue (val);

        return p;
    };

    auto addParam = [this] (const String& paramID, const String& name, NormalisableRange<float> valueRange, float centreVal, CachedValue<float>& val, const String& suffix) -> AutomatableParameter*
    {
        valueRange.setSkewForCentre (centreVal);
        auto* p = new SuffixedParameter (paramID, name, *this, valueRange, suffix);
        addAutomatableParameter (p);
        p->attachToCurrentValue (val);

        return p;
    };

    typeParam           = addDiscreteParam ("type",     TRANS("MIDI Type"), { 0.0f, (float)  getTypeNames().size() - 1 },   type,           getTypeNames());
    modeParam           = addDiscreteParam ("mode",     TRANS("Mode"),      { 0.0f, (float)  getModeNames().size() - 1 },   mode,           getModeNames());
    relativeRootParam   = addParam ("relativeRoot",     TRANS("Root"),      { 1.0f, 127.0f, 1.0f }, 63.5f,                  relativeRoot,   {});
    relativeSpreadParam = addParam ("relativeSpread",   TRANS("Spread"),    { 1.0f, 64.0f, 1.0f }, 32.0f,                   relativeSpread, {});

    updateMapFromTree();

    changedTimer.setCallback ([this]
                              {
                                  changedTimer.stopTimer();
                                  changed();
                              });

    state.addListener (this);
}

MIDITrackerModifier::~MIDITrackerModifier()
{
    state.removeListener (this);
    notifyListenersOfDeletion();

    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();

    deleteAutomatableParameters();
}

void MIDITrackerModifier::initialise()
{
    restoreChangedParametersFromState();
}

//==============================================================================
float MIDITrackerModifier::getCurrentValue()
{
    return currentModifierValue.load (std::memory_order_acquire);
}

int MIDITrackerModifier::getCurrentMIDIValue() const noexcept
{
    return currentMIDIValue.load (std::memory_order_acquire);
}

AutomatableParameter::ModifierAssignment* MIDITrackerModifier::createAssignment (const ValueTree& v)
{
    return new Assignment (v, *this);
}

StringArray MIDITrackerModifier::getMidiInputNames()
{
    return { TRANS("MIDI input") };
}

AudioNode* MIDITrackerModifier::createPreFXAudioNode (AudioNode* an)
{
    return new ModifierAudioNode (an, *this);
}

void MIDITrackerModifier::applyToBuffer (const AudioRenderContext& rc)
{
    if (rc.bufferForMidiMessages != nullptr)
        for (auto& m : *rc.bufferForMidiMessages)
            if (m.isNoteOn())
                midiEvent (m);
}

//==============================================================================
MIDITrackerModifier::Assignment::Assignment (const juce::ValueTree& v, const MIDITrackerModifier& mtm)
    : AutomatableParameter::ModifierAssignment (mtm.edit, v),
      midiTrackerModifierID (mtm.itemID)
{
}

bool MIDITrackerModifier::Assignment::isForModifierSource (const ModifierSource& source) const
{
    if (auto* mod = dynamic_cast<const MIDITrackerModifier*> (&source))
        return mod->itemID == midiTrackerModifierID;

    return false;
}

MIDITrackerModifier::Ptr MIDITrackerModifier::Assignment::getMIDITrackerModifier() const
{
    if (auto mod = findModifierTypeForID<MIDITrackerModifier> (edit, midiTrackerModifierID))
        return mod;

    return {};
}

//==============================================================================
StringArray MIDITrackerModifier::getTypeNames()
{
    return { NEEDS_TRANS("Pitch"),
             NEEDS_TRANS("Velocity") };
}

StringArray MIDITrackerModifier::getModeNames()
{
    return { NEEDS_TRANS("Absolute"),
             NEEDS_TRANS("Relative") };
}

//==============================================================================
void MIDITrackerModifier::refreshCurrentValue()
{
    // CAS to ensure the modifier value is up to date with the new map and current note
    for (;;)
    {
        auto midiVal = getCurrentMIDIValue();
        updateValueFromMap (midiVal);

        if (midiVal == getCurrentMIDIValue())
            break;
    }
}

void MIDITrackerModifier::updateMapFromTree()
{
    if (! edit.isLoading())
        TRACKTION_ASSERT_MESSAGE_THREAD

    Map newMap;

    for (int i = 0; i < 5; ++i)
    {
        auto v = nodeState.getChild (i);

        if (! v.isValid())
        {
            v = ValueTree (IDs::NODE);
            v.setProperty (IDs::midi, jmap (i, 0, 4, 0, 127), nullptr);
            v.setProperty (IDs::value, jmap ((float) i, 0.0f, 4.0f, -1.0f, 1.0f), nullptr);
            nodeState.addChild (v, -1, nullptr);
        }

        newMap.node[i] = std::pair<int, float> ( v[IDs::midi], v[IDs::value]);
    }

    {
        const SpinLock::ScopedLockType sl (mapLock);
        currentMap = newMap;
    }

    refreshCurrentValue();
}

void MIDITrackerModifier::updateValueFromMap (int midiVal)
{
    jassert (isPositiveAndBelow (midiVal, 128));
    auto linearInterpolate = [] (float x1, float y1, float x2, float y2, float x)
    {
        return (y1 * (x2 - x) + y2 * (x - x1)) / (x2 - x1);
    };

    if (getTypedParamValue<Mode> (*modeParam) == relative)
    {
        const float root = relativeRootParam->getCurrentValue();
        const float spread = relativeSpreadParam->getCurrentValue();

        currentMIDIValue.store (midiVal, std::memory_order_release);
        auto val = linearInterpolate (root, 0.0f, root + spread, 1.0f, (float) midiVal);
        currentModifierValue.store (val, std::memory_order_release);
        return;
    }
    else
    {
        Map map;

        {
            const SpinLock::ScopedLockType sl (mapLock);
            map = currentMap;
        }

        auto lastNode = map.node[4];

        for (int i = 4; --i >= 0;)
        {
            const auto node = map.node[i];
            const int nodeMidiVal = std::get<int> (node);

            if (nodeMidiVal <= midiVal)
            {
                currentMIDIValue.store (midiVal, std::memory_order_release);

                auto val = linearInterpolate ((float) nodeMidiVal, std::get<float> (node),
                                              (float) std::get<int> (lastNode), std::get<float> (lastNode),
                                              (float) midiVal);
                currentModifierValue.store (val, std::memory_order_release);
                return;
            }

            lastNode = node;
        }
    }

    jassertfalse;
}

void MIDITrackerModifier::midiEvent (const MidiMessage& m)
{
    if (! m.isNoteOn())
        return;

    if (getTypedParamValue<Type> (*typeParam) == pitch)
        updateValueFromMap (m.getNoteNumber());
    else
        updateValueFromMap (m.getVelocity());
}

void MIDITrackerModifier::valueTreeChanged()
{
    if (! changedTimer.isTimerRunning())
        changedTimer.startTimerHz (30);
}

void MIDITrackerModifier::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
{
    if (v == state)
    {
        if (i == IDs::mode)
        {
            updateMapFromTree();
            propertiesChanged();
        }
        else if (i == IDs::root || i == IDs::spread)
        {
            refreshCurrentValue();
        }
    }
    else if (v.hasType (IDs::NODE))
    {
        if (i == IDs::midi || i == IDs::value)
            updateMapFromTree();
    }

    ValueTreeAllEventListener::valueTreePropertyChanged (v, i);
}
