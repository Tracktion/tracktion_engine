/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct StepModifier::StepModifierTimer : public ModifierTimer
{
    StepModifierTimer (StepModifier& sm)
        : stepModifier (sm), tempoSequence (sm.edit.tempoSequence)
    {
    }

    void updateStreamTime (PlayHead& ph, EditTimeRange streamTime, int /*numSamples*/) override
    {
        auto editTime = (float) ph.streamTimeToSourceTime (streamTime.getStart());
        stepModifier.updateParameterStreams (editTime);

        const auto syncTypeThisBlock = roundToInt (stepModifier.syncTypeParam->getCurrentValue());
        const auto rateTypeThisBlock = getTypedParamValue<ModifierCommon::RateType> (*stepModifier.rateTypeParam);

        const float numStepsThisBlock = stepModifier.numStepsParam->getCurrentValue();
        const float rateThisBlock = stepModifier.rateParam->getCurrentValue();

        if (rateTypeThisBlock == ModifierCommon::hertz)
        {
            const float durationPerPattern = numStepsThisBlock * (1.0f / rateThisBlock);
            ramp.setDuration (durationPerPattern);

            if (syncTypeThisBlock == ModifierCommon::transport)
                ramp.setPosition (std::fmod (editTime, durationPerPattern));

            const int step = static_cast<int> (std::floor (numStepsThisBlock * ramp.getProportion()));
            stepModifier.currentStep.store (step, std::memory_order_release);

            // Move the ramp on for the next block
            ramp.process ((float) streamTime.getLength());
        }
        else
        {
            tempoSequence.setTime (editTime);
            const auto currentTempo = tempoSequence.getCurrentTempo();
            const double proportionOfBar = ModifierCommon::getBarFraction (rateTypeThisBlock);

            if (syncTypeThisBlock == ModifierCommon::transport)
            {
                const float editTimeInBeats = (float) (currentTempo.startBeatInEdit + (editTime - currentTempo.startTime) * currentTempo.beatsPerSecond);
                const double bars = (editTimeInBeats / currentTempo.numerator) * rateThisBlock;

                if (rateTypeThisBlock >= ModifierCommon::fourBars && rateTypeThisBlock <= ModifierCommon::sixtyFourthD)
                {
                    const double virtualBars = bars / proportionOfBar;
                    const int step = static_cast<int> (std::fmod (virtualBars, numStepsThisBlock));
                    stepModifier.currentStep.store (step, std::memory_order_release);
                }
            }
            else
            {
                const double bpm = (currentTempo.bpm * rateThisBlock) / proportionOfBar;
                const double secondsPerBeat = 60.0 / bpm;
                const float secondsPerStep = static_cast<float> (secondsPerBeat * currentTempo.numerator);
                const float secondsPerPattern = (numStepsThisBlock * secondsPerStep);
                ramp.setDuration (secondsPerPattern);

                const int step = static_cast<int> (std::floor (numStepsThisBlock * ramp.getProportion()));
                stepModifier.currentStep.store (step, std::memory_order_release);

                // Move the ramp on for the next block
                ramp.process ((float) streamTime.getLength());
            }
        }
    }

    void resync (EditTimeRange streamTime)
    {
        const auto type = roundToInt (stepModifier.syncTypeParam->getCurrentValue());

        if (type == ModifierCommon::note)
        {
            ramp.setPosition (0.0f);
            stepModifier.currentStep.store (0, std::memory_order_release);

            // Move the ramp on for the next block
            ramp.process ((float) streamTime.getLength());
        }
    }

    StepModifier& stepModifier;
    Ramp ramp;
    TempoSequencePosition tempoSequence;
};

//==============================================================================
struct StepModifier::StepModifierAudioNode    : public SingleInputAudioNode
{
    StepModifierAudioNode (AudioNode* input, StepModifier& sm)
        : SingleInputAudioNode (input),
          stepModifier (&sm)
    {
    }

    void renderOver (const AudioRenderContext& rc) override
    {
        SingleInputAudioNode::renderOver (rc);

        if (rc.bufferForMidiMessages != nullptr)
        {
            for (auto& m : *rc.bufferForMidiMessages)
            {
                if (m.isNoteOn())
                {
                    stepModifier->stepModifierTimer->resync (rc.streamTime);
                    break;
                }
            }
        }
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        callRenderOver (rc);
    }

    StepModifier::Ptr stepModifier;
};

//==============================================================================
StepModifier::StepModifier (Edit& e, const ValueTree& v)
    : Modifier (e, v)
{
    auto um = &edit.getUndoManager();

    restoreStepsFromProperty();

    syncType.referTo (state, IDs::syncType, um, float (ModifierCommon::free));
    numSteps.referTo (state, IDs::numSteps, um, 16.0f);
    rate.referTo (state, IDs::rate, um, 1.0f);
    rateType.referTo (state, IDs::rateType, um, float (ModifierCommon::bar));
    depth.referTo (state, IDs::depth, um, 1.0f);

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
    jassert (isPositiveAndBelow (step, maxNumSteps));
    return jlimit (-1.0f, 1.0f, steps[step]);
}

void StepModifier::setStep (int step, float value)
{
    if (! isPositiveAndBelow (step, maxNumSteps) || steps[step] == value)
        return;

    steps[step] = jlimit (-1.0f, 1.0f, value);
    flushStepsToProperty();
}

int StepModifier::getCurrentStep() const noexcept
{
    return currentStep.load (std::memory_order_acquire);
}

AutomatableParameter::ModifierAssignment* StepModifier::createAssignment (const ValueTree& v)
{
    return new Assignment (v, *this);
}

AudioNode* StepModifier::createPreFXAudioNode (AudioNode* an)
{
    return new StepModifierAudioNode (an, *this);
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
String StepModifier::getSelectableDescription()
{
    return TRANS("Step Modifier");
}

//==============================================================================
void StepModifier::flushStepsToProperty()
{
    MemoryOutputStream stream;

    for (int i = 0; i < maxNumSteps; ++i)
        stream.writeFloat (steps[i]);

    stream.flush();
    state.setProperty (IDs::stepData, stream.getMemoryBlock(), &edit.getUndoManager());
}

void StepModifier::restoreStepsFromProperty()
{
    std::fill_n (steps, numElementsInArray (steps), 0.0f);

    if (auto mb = state[IDs::stepData].getBinaryData())
    {
        MemoryInputStream stream (*mb, false);
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

void StepModifier::valueTreePropertyChanged (ValueTree& v, const Identifier& i)
{
    if (v == state && i == IDs::stepData)
        restoreStepsFromProperty();

    ValueTreeAllEventListener::valueTreePropertyChanged (v, i);
}
