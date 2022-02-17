/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
class Modifier::ValueFifo
{
public:
    ValueFifo (double newSampleRate, TimeDuration maxNumSecondsToStore)
        : sampleRate (newSampleRate)
    {
        jassert (sampleRate > 0.0);
        
        const auto numSamplesToStore = (size_t) tracktion::toSamples (maxNumSecondsToStore, newSampleRate);
        values = std::vector<float> (numSamplesToStore, 0.0f);
    }
    
    void addValue (int numSamplesDelta, float value)
    {
        jassert ((size_t) numSamplesDelta < values.size());
        
        const float delta = (value - lastValue) / numSamplesDelta;
        float alpha = lastValue;
        
        while (--numSamplesDelta >= 0)
        {
            if (++headIndex == values.size())
                headIndex = 0;
                
            alpha += delta;
            values[headIndex] = alpha;
        }
            
        lastValue = value;
    }
    
    [[nodiscard]] float getValueAt (TimeDuration numSeconds) const
    {
        const size_t sampleDelta = (size_t) tracktion::toSamples (numSeconds, sampleRate);
        
        if (sampleDelta > values.size())
            return {};
        
        const int deltaIndex = int (headIndex) - int (sampleDelta);
        const size_t valueIndex = (size_t) juce::negativeAwareModulo (deltaIndex, (int) values.size());
        
        return values[valueIndex];
    }
    
    [[nodiscard]] std::vector<float> getValues (TimeDuration numSecondsBeforeNow) const
    {
        std::vector<float> v;
        
        const auto numValues = std::min (values.size(),
                                         (size_t) tracktion::toSamples (numSecondsBeforeNow, sampleRate));
        v.reserve (numValues);

        int index = (int) headIndex;
        
        for (int i = (int) numValues; --i >= 0;)
        {
            v.push_back (values[(size_t) index]);
            
            if (--index < 0)
                index = (int) values.size() - 1;
        }

        return v;
    }
    
private:
    double sampleRate = 0.0;
    float lastValue = 0.0;
    std::vector<float> values;
    size_t headIndex = 0;
};

//==============================================================================
//==============================================================================
Modifier::Modifier (Edit& e, const juce::ValueTree& v)
    : AutomatableEditItem (e, v),
      state (v)
{
    auto um = &edit.getUndoManager();
    colour.referTo (state, IDs::colour, um, juce::Colours::red.withHue (1.0f / 9.0f));
    enabled.referTo (state, IDs::enabled, um, true);

    enabledParam = new DiscreteLabelledParameter ("enabled", TRANS("Enabled"), *this, { 0.0f, 1.0f },
                                                  modifier::getEnabledNames().size(), modifier::getEnabledNames());
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
void Modifier::baseClassInitialise (double newSampleRate, int blockSizeSamples)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    sampleRate = newSampleRate;

    if (initialiseCount++ == 0)
    {
        CRASH_TRACER
        initialise (sampleRate, blockSizeSamples);

        const auto numSecondsToStore = maxHistoryTime;
        valueFifo = std::make_unique<ValueFifo> (sampleRate, numSecondsToStore);
        messageThreadValueFifo = std::make_unique<ValueFifo> (sampleRate, numSecondsToStore);
        
        const int numSamples = (int) tracktion::toSamples (numSecondsToStore, sampleRate);
        const int numBlocks = (numSamples / blockSizeSamples) * 2;
        valueFifoQueue.reset ((size_t) numBlocks);
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
        valueFifo.reset();
    }
}

void Modifier::baseClassApplyToBuffer (const PluginRenderContext& prc)
{
    applyToBuffer (prc);
    jassert (valueFifo);
    const float v = getCurrentValue();
    valueFifo->addValue (prc.bufferNumSamples, getCurrentValue());
    
    // Queue a value change up to be dispatched on the message thread
    const auto newV = std::make_pair (prc.bufferNumSamples, v);
    
    if (valueFifoQueue.getFreeSlots() == 0)
    {
        std::remove_cv_t<decltype(newV)> temp;
        valueFifoQueue.pop (temp);
    }
    
    valueFifoQueue.push (newV);
}

//==============================================================================
TimePosition Modifier::getCurrentTime() const
{
    return lastEditTime;
}

float Modifier::getValueAt (TimeDuration numSecondsBeforeNow) const
{
    if (valueFifo)
        return valueFifo->getValueAt (numSecondsBeforeNow);
        
    return {};
}

std::vector<float> Modifier::getValues (TimeDuration numSecondsBeforeNow) const
{
    if (! messageThreadValueFifo)
        return {};
    
    // Dispatch pending values
    for (;;)
    {
        std::pair<int, float> tmp;
        
        if (! valueFifoQueue.pop (tmp))
            break;
        
        messageThreadValueFifo->addValue (tmp.first, tmp.second);
    }
    
    // Then return the array of samples
    return messageThreadValueFifo->getValues (numSecondsBeforeNow);
}

//==============================================================================
ModifierList::ModifierList (Edit& e, const juce::ValueTree& parentTree)
    : ValueTreeObjectList<Modifier> (parentTree),
      edit (e), state (parent)
{
    jassert (parent.hasType (IDs::MODIFIERS));
    callBlocking ([this] { rebuildObjects(); });
}

ModifierList::~ModifierList()
{
    freeObjects();
}

bool ModifierList::isModifier (const juce::Identifier& i)
{
    return i == IDs::LFO || i == IDs::BREAKPOINTOSCILLATOR
        || i == IDs::STEP || i == IDs::ENVELOPEFOLLOWER
        || i == IDs::RANDOM || i == IDs::MIDITRACKER;
}

juce::ReferenceCountedArray<Modifier> ModifierList::getModifiers() const
{
    if (! edit.isLoading())
        TRACKTION_ASSERT_MESSAGE_THREAD

    juce::ReferenceCountedArray<Modifier> mods;

    for (auto m : objects)
        mods.add (m);

    return mods;
}

//==============================================================================
juce::ReferenceCountedObjectPtr<Modifier> ModifierList::insertModifier (juce::ValueTree v, int index, SelectionManager* sm)
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

Modifier* ModifierList::createNewObject (const juce::ValueTree& v)
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

}} // namespace tracktion { inline namespace engine
