/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct RandomModifier::RandomModifierTimer    : public ModifierTimer
{
    RandomModifierTimer (RandomModifier& rm)
        : modifier (rm), tempoSequence (rm.edit.tempoSequence)
    {
    }

    void updateStreamTime (PlayHead& ph, EditTimeRange streamTime, int /*numSamples*/) override
    {
        auto editTime = (float) ph.streamTimeToSourceTime (streamTime.getStart());
        modifier.updateParameterStreams (editTime);

        const auto syncTypeThisBlock = roundToInt (modifier.syncTypeParam->getCurrentValue());
        const auto rateTypeThisBlock = getTypedParamValue<ModifierCommon::RateType> (*modifier.rateTypeParam);

        const float rateThisBlock = modifier.rateParam->getCurrentValue();

        if (rateTypeThisBlock == ModifierCommon::hertz)
        {
            const float durationPerPattern = 1.0f / rateThisBlock;
            ramp.setDuration (durationPerPattern);

            if (syncTypeThisBlock == ModifierCommon::transport)
                ramp.setPosition (std::fmod (editTime, durationPerPattern));

            modifier.setPhase (ramp.getProportion());

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
                    modifier.setPhase ((float) std::fmod (virtualBars, 1.0f));
                }
            }
            else
            {
                const double bpm = (currentTempo.bpm * rateThisBlock) / proportionOfBar;
                const double secondsPerBeat = 60.0 / bpm;
                const float secondsPerStep = static_cast<float> (secondsPerBeat * currentTempo.numerator);
                const float secondsPerPattern = secondsPerStep;
                ramp.setDuration (secondsPerPattern);

                modifier.setPhase (ramp.getProportion());

                // Move the ramp on for the next block
                ramp.process ((float) streamTime.getLength());
            }
        }
    }

    void resync (EditTimeRange streamTime)
    {
        if (roundToInt (modifier.syncTypeParam->getCurrentValue()) == ModifierCommon::note)
        {
            ramp.setPosition (0.0f);
            modifier.setPhase (0.0f);

            // Move the ramp on for the next block
            ramp.process ((float) streamTime.getLength());
        }
    }

    RandomModifier& modifier;
    Ramp ramp;
    TempoSequencePosition tempoSequence;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RandomModifierTimer)
};

//==============================================================================
struct RandomModifier::ModifierAudioNode    : public SingleInputAudioNode
{
    ModifierAudioNode (AudioNode* input, RandomModifier& rm)
        : SingleInputAudioNode (input),
          modifier (&rm)
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
                    modifier->modifierTimer->resync (rc.streamTime);
                    break;
                }
            }
        }
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        callRenderOver (rc);
    }

    RandomModifier::Ptr modifier;
};

//==============================================================================
RandomModifier::RandomModifier (Edit& e, const ValueTree& v)
    : Modifier (e, v)
{
    auto um = &edit.getUndoManager();

    type.referTo (state, IDs::type, um, random);
    shape.referTo (state, IDs::shape, um);
    syncType.referTo (state, IDs::syncType, um, float (ModifierCommon::free));
    rate.referTo (state, IDs::rate, um, 1.0f);
    rateType.referTo (state, IDs::rateType, um, float (ModifierCommon::bar));
    depth.referTo (state, IDs::depth, um, 1.0f);
    stepDepth.referTo (state, IDs::stepDepth, um, 1.0f);
    smooth.referTo (state, IDs::smooth, um);
    bipolar.referTo (state, IDs::bipolar, um);

    auto addDiscreteParam = [this] (const String& paramID, const String& name, Range<float> valueRange, CachedValue<float>& val,
                                    const StringArray& labels) -> AutomatableParameter*
    {
        auto* p = new DiscreteLabelledParameter (paramID, name, *this, valueRange, labels.size(), labels);
        addAutomatableParameter (p);
        p->attachToCurrentValue (val);

        return p;
    };

    auto addParam = [this] (const String& paramID, const String& name, NormalisableRange<float> valueRange,
                            float centreVal, CachedValue<float>& val, const String& suffix) -> AutomatableParameter*
    {
        valueRange.setSkewForCentre (centreVal);
        auto* p = new SuffixedParameter (paramID, name, *this, valueRange, suffix);
        addAutomatableParameter (p);
        p->attachToCurrentValue (val);

        return p;
    };

    using namespace ModifierCommon;
    typeParam       = addDiscreteParam ("type",     TRANS("Type"),      { 0.0f, (float)  getTypeNames().size() - 1 },           type,       getTypeNames());
    shapeParam      = addParam ("shape",            TRANS("Shape"),     { 0.0f, 1.0f }, 0.5f,                                   shape,      {});
    syncTypeParam   = addDiscreteParam ("syncType", TRANS("Sync type"), { 0.0f, (float)  getSyncTypeChoices().size() - 1 },     syncType,   getSyncTypeChoices());
    rateParam       = addParam ("rate",             TRANS("Rate"),      { 0.01f, 50.0f }, 1.0f,                                 rate,       {});
    rateTypeParam   = addDiscreteParam ("rateType", TRANS("Rate Type"), { 0.0f, (float) getRateTypeChoices().size() - 1 },      rateType,   getRateTypeChoices());
    depthParam      = addParam ("depth",            TRANS("Depth"),     { 0.0f, 1.0f }, 0.5f,                                   depth,      {});
    stepDepthParam  = addParam ("stepDepth",        TRANS("Step Depth"),{ 0.0f, 1.0f }, 0.5f,                                   stepDepth,  {});
    smoothParam     = addParam ("smooth",           TRANS("Smooth"),    { 0.0f, 1.0f }, 0.5f,                                   smooth,     {});
    bipolarParam    = addDiscreteParam ("biopolar", TRANS("Bipoloar"),  { 0.0f, 1.0f },                                         bipolar,    { NEEDS_TRANS("Uni-polar"), NEEDS_TRANS("Bi-polar") });

    changedTimer.setCallback ([this]
                              {
                                  changedTimer.stopTimer();
                                  changed();
                              });

    state.addListener (this);
}

RandomModifier::~RandomModifier()
{
    state.removeListener (this);
    notifyListenersOfDeletion();

    edit.removeModifierTimer (*modifierTimer);

    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();

    deleteAutomatableParameters();
}

void RandomModifier::initialise()
{
    // Do this here in case the audio code starts using the parameters before the constructor has finished
    modifierTimer = std::make_unique<RandomModifierTimer> (*this);
    edit.addModifierTimer (*modifierTimer);

    restoreChangedParametersFromState();
}

//==============================================================================
float RandomModifier::getCurrentValue()
{
    return currentValue.load (std::memory_order_acquire);
}

float RandomModifier::getCurrentPhase() const noexcept
{
    return currentPhase.load (std::memory_order_acquire);
}

AutomatableParameter::ModifierAssignment* RandomModifier::createAssignment (const ValueTree& v)
{
    return new Assignment (v, *this);
}

AudioNode* RandomModifier::createPreFXAudioNode (AudioNode* an)
{
    return new ModifierAudioNode (an, *this);
}

//==============================================================================
RandomModifier::Assignment::Assignment (const juce::ValueTree& v, const RandomModifier& rm)
    : AutomatableParameter::ModifierAssignment (rm.edit, v),
      randomModifierID (rm.itemID)
{
}

bool RandomModifier::Assignment::isForModifierSource (const ModifierSource& source) const
{
    if (auto* mod = dynamic_cast<const RandomModifier*> (&source))
        return mod->itemID == randomModifierID;

    return false;
}

RandomModifier::Ptr RandomModifier::Assignment::getRandomModifier() const
{
    if (auto mod = findModifierTypeForID<RandomModifier> (edit, randomModifierID))
        return mod;

    return {};
}

//==============================================================================
StringArray RandomModifier::getTypeNames()
{
    return { NEEDS_TRANS("Random"),
             NEEDS_TRANS("Noise") };
}

//==============================================================================
void RandomModifier::setPhase (float newPhase)
{
    const float s = smoothParam->getCurrentValue();
    newPhase = ((1.0f - s) * newPhase) + (s * AudioFadeCurve::SCurve::get (newPhase));

    while (newPhase >= 1.0f)    newPhase -= 1.0f;
    while (newPhase < 0.0f)     newPhase += 1.0f;

    if (newPhase < getCurrentPhase())
    {
        previousRandom = currentRandom;

        const bool bp = getBoolParamValue (*bipolarParam);
        const float sd = stepDepthParam->getCurrentValue() * (bp ? 1.0f : 0.5f);
        const auto randomRange = Range<float> (currentRandom - sd, currentRandom + sd).getIntersectionWith ({ bp ? -1.0f : 0.0f, 1.0f });

        currentRandom = jmap (rand.nextFloat(), randomRange.getStart(), randomRange.getEnd());
        jassert (randomRange.contains (currentRandom));
        randomDifference = currentRandom - previousRandom;
    }

    jassert (isPositiveAndBelow (newPhase, 1.0f));
    currentPhase.store (newPhase, std::memory_order_release);

    auto newValue = [this, newPhase, s = shapeParam->getCurrentValue()]
    {
        if (newPhase < (1.0f - s))
            return previousRandom;

        jassert (s > 0.0f);
        const float skewedPhase = ((newPhase - 1.0f) / s) + 1.0f;
        return (randomDifference * skewedPhase) + previousRandom;
    }();

    newValue *= depthParam->getCurrentValue();

    currentValue.store (newValue, std::memory_order_release);
}

void RandomModifier::valueTreeChanged()
{
    if (! changedTimer.isTimerRunning())
        changedTimer.startTimerHz (30);
}
