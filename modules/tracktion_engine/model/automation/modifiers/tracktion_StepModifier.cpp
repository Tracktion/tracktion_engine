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

struct StepModifier::StepModifierTimer : public ModifierTimer
{
    StepModifierTimer (StepModifier& sm)
        : modifier (sm)
    {
    }

    void updateStreamTime (TimePosition editTime, int numSamples) override
    {
        const double blockLength = numSamples / modifier.getSampleRate();
        modifier.setEditTime (editTime);
        modifier.updateParameterStreams (editTime);

        const auto syncTypeThisBlock = juce::roundToInt (modifier.syncTypeParam->getCurrentValue());
        const auto rateTypeThisBlock = getTypedParamValue<ModifierCommon::RateType> (*modifier.rateTypeParam);

        const float numStepsThisBlock = modifier.numStepsParam->getCurrentValue();
        const float rateThisBlock = modifier.rateParam->getCurrentValue();

        if (rateTypeThisBlock == ModifierCommon::hertz)
        {
            const float durationPerPattern = numStepsThisBlock * (1.0f / rateThisBlock);
            ramp.setDuration (durationPerPattern);

            if (syncTypeThisBlock == ModifierCommon::transport)
                ramp.setPosition (std::fmod ((float) editTime.inSeconds(), durationPerPattern));

            const int step = static_cast<int> (std::floor (numStepsThisBlock * ramp.getProportion()));
            modifier.currentStep.store (step, std::memory_order_release);

            // Move the ramp on for the next block
            ramp.process ((float) blockLength);
        }
        else
        {
            tempoSequence.set (editTime);
            const auto currentTempo = tempoSequence.getTempo();
            const auto currentTimeSig = tempoSequence.getTimeSignature();
            const auto proportionOfBar = ModifierCommon::getBarFraction (rateTypeThisBlock);

            if (syncTypeThisBlock == ModifierCommon::transport)
            {
                const auto editTimeInBeats = tempoSequence.getBeats().inBeats();
                const auto bars = (editTimeInBeats / currentTimeSig.numerator) * rateThisBlock;

                if (rateTypeThisBlock >= ModifierCommon::fourBars && rateTypeThisBlock <= ModifierCommon::sixtyFourthD)
                {
                    const double virtualBars = bars / proportionOfBar;
                    const int step = static_cast<int> (std::fmod (virtualBars, numStepsThisBlock));
                    modifier.currentStep.store (step, std::memory_order_release);
                }
            }
            else
            {
                const double bpm = (currentTempo * rateThisBlock) / proportionOfBar;
                const double secondsPerBeat = 60.0 / bpm;
                const float secondsPerStep = static_cast<float> (secondsPerBeat * currentTimeSig.numerator);
                const float secondsPerPattern = (numStepsThisBlock * secondsPerStep);
                ramp.setDuration (secondsPerPattern);

                const int step = static_cast<int> (std::floor (numStepsThisBlock * ramp.getProportion()));
                modifier.currentStep.store (step, std::memory_order_release);

                // Move the ramp on for the next block
                ramp.process ((float) blockLength);
            }
        }
    }

    void resync (double duration)
    {
        const auto type = juce::roundToInt (modifier.syncTypeParam->getCurrentValue());

        if (type == ModifierCommon::note)
        {
            ramp.setPosition (0.0f);
            modifier.currentStep.store (0, std::memory_order_release);

            // Move the ramp on for the next block
            ramp.process ((float) duration);
        }
    }

    StepModifier& modifier;
    Ramp ramp;
    tempo::Sequence::Position tempoSequence { createPosition (modifier.edit.tempoSequence) };
};

//==============================================================================
StepModifier::StepModifier (Edit& e, const juce::ValueTree& v)
    : Modifier (e, v)
{
    auto um = &edit.getUndoManager();

    restoreStepsFromProperty();

    syncType.referTo (state, IDs::syncType, um, float (ModifierCommon::free));
    numSteps.referTo (state, IDs::numSteps, um, 16.0f);
    rate.referTo (state, IDs::rate, um, 1.0f);
    rateType.referTo (state, IDs::rateType, um, float (ModifierCommon::bar));
    depth.referTo (state, IDs::depth, um, 1.0f);

    auto addDiscreteParam = [this] (const juce::String& paramID, const juce::String& name,
                                    juce::Range<float> valueRange, juce::CachedValue<float>& val,
                                    const juce::StringArray& labels) -> AutomatableParameter*
    {
        auto* p = new DiscreteLabelledParameter (paramID, name, *this, valueRange, labels.size(), labels);
        addAutomatableParameter (p);
        p->attachToCurrentValue (val);

        return p;
    };

    auto addParam = [this] (const juce::String& paramID, const juce::String& name,
                            juce::NormalisableRange<float> valueRange, float centreVal,
                            juce::CachedValue<float>& val, const juce::String& suffix) -> AutomatableParameter*
    {
        valueRange.setSkewForCentre (centreVal);
        auto* p = new SuffixedParameter (paramID, name, *this, valueRange, suffix);
        addAutomatableParameter (p);
        p->attachToCurrentValue (val);

        return p;
    };

    using namespace ModifierCommon;
    syncTypeParam   = addDiscreteParam ("syncType", TRANS("Sync type"), { 0.0f, (float)  note },        syncType,   getSyncTypeChoices());
    numStepsParam   = addParam ("numSteps",         TRANS("Steps"),     { 2.0f, 64.0f, 1.0f }, 31.0f,   numSteps,   {});
    rateParam       = addParam ("rate",             TRANS("Rate"),      { 0.01f, 50.0f }, 1.0f,         rate,       {});
    rateTypeParam   = addDiscreteParam ("rateType", TRANS("Rate Type"), { 0.0f, (float) sixtyFourthD }, rateType,   getRateTypeChoices());
    depthParam      = addParam ("depth",            TRANS("Depth"),     { -1.0f, 1.0f }, 0.0f,          depth,      {});

    changedTimer.setCallback ([this]
                              {
                                  changedTimer.stopTimer();
                                  changed();
                              });

    state.addListener (this);
}

StepModifier::~StepModifier()
{
    state.removeListener (this);
    notifyListenersOfDeletion();

    edit.removeModifierTimer (*stepModifierTimer);

    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();

    deleteAutomatableParameters();
}

void StepModifier::initialise()
{
    // Do this here in case the audio code starts using the parameters before the constructor has finished
    stepModifierTimer = std::make_unique<StepModifierTimer> (*this);
    edit.addModifierTimer (*stepModifierTimer);

    restoreChangedParametersFromState();
}

float StepModifier::getCurrentValue()
{
    return getStep (getCurrentStep()) * depthParam->getCurrentValue();
}

//==============================================================================
float StepModifier::getStep (int step) const
{
    jassert (juce::isPositiveAndBelow (step, maxNumSteps));
    return juce::jlimit (-1.0f, 1.0f, steps[step]);
}

void StepModifier::setStep (int step, float value)
{
    if (! juce::isPositiveAndBelow (step, maxNumSteps) || steps[step] == value)
        return;

    steps[step] = juce::jlimit (-1.0f, 1.0f, value);
    flushStepsToProperty();
}

int StepModifier::getCurrentStep() const noexcept
{
    return currentStep.load (std::memory_order_acquire);
}

AutomatableParameter::ModifierAssignment* StepModifier::createAssignment (const juce::ValueTree& v)
{
    return new Assignment (v, *this);
}

void StepModifier::applyToBuffer (const PluginRenderContext& prc)
{
    if (prc.bufferForMidiMessages == nullptr)
        return;
    
    for (auto& m : *prc.bufferForMidiMessages)
        if (m.isNoteOn())
            stepModifierTimer->resync (prc.bufferNumSamples / getSampleRate());
}

//==============================================================================
StepModifier::Assignment::Assignment (const juce::ValueTree& v, const StepModifier& sm)
    : AutomatableParameter::ModifierAssignment (sm.edit, v),
      stepModifierID (sm.itemID)
{
}

bool StepModifier::Assignment::isForModifierSource (const ModifierSource& source) const
{
    if (auto* mod = dynamic_cast<const StepModifier*> (&source))
        return mod->itemID == stepModifierID;

    return false;
}

StepModifier::Ptr StepModifier::Assignment::getStepModifier() const
{
    if (auto mod = findModifierTypeForID<StepModifier> (edit, stepModifierID))
        return mod;

    return {};
}

//==============================================================================
juce::String StepModifier::getSelectableDescription()
{
    return TRANS("Step Modifier");
}

//==============================================================================
void StepModifier::flushStepsToProperty()
{
    juce::MemoryOutputStream stream;

    for (int i = 0; i < maxNumSteps; ++i)
        stream.writeFloat (steps[i]);

    stream.flush();
    state.setProperty (IDs::stepData, stream.getMemoryBlock(), &edit.getUndoManager());
}

void StepModifier::restoreStepsFromProperty()
{
    std::fill_n (steps, juce::numElementsInArray (steps), 0.0f);

    if (auto mb = state[IDs::stepData].getBinaryData())
    {
        juce::MemoryInputStream stream (*mb, false);
        int index = 0;

        while (! stream.isExhausted())
            steps[index++] = stream.readFloat();
    }
}

//==============================================================================
void StepModifier::valueTreeChanged()
{
    if (! changedTimer.isTimerRunning())
        changedTimer.startTimerHz (30);
}

void StepModifier::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
{
    if (v == state && i == IDs::stepData)
        restoreStepsFromProperty();

    ValueTreeAllEventListener::valueTreePropertyChanged (v, i);
}

}} // namespace tracktion { inline namespace engine
