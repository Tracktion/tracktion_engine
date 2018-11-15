/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct LFOModifier::LFOModifierTimer    : public ModifierTimer
{
    LFOModifierTimer (LFOModifier& lfo)
        : modifier (lfo), tempoSequence (lfo.edit.tempoSequence)
    {
    }

    void updateStreamTime (PlayHead& ph, EditTimeRange streamTime, int /*numSamples*/) override
    {
        const float editTime = (float) ph.streamTimeToSourceTime (streamTime.getStart());
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

            setPhase (ramp.getProportion());

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
                    setPhase ((float) std::fmod (virtualBars, 1.0f));
                }
            }
            else
            {
                const double bpm = (currentTempo.bpm * rateThisBlock) / proportionOfBar;
                const double secondsPerBeat = 60.0 / bpm;
                const float secondsPerStep = static_cast<float> (secondsPerBeat * currentTempo.numerator);
                const float secondsPerPattern = secondsPerStep;
                ramp.setDuration (secondsPerPattern);

                setPhase (ramp.getProportion());

                // Move the ramp on for the next block
                ramp.process ((float) streamTime.getLength());
            }
        }
    }

    void setPhase (float newPhase)
    {
        newPhase += modifier.phaseParam->getCurrentValue();

        while (newPhase >= 1.0f)    newPhase -= 1.0f;
        while (newPhase < 0.0f)     newPhase += 1.0f;

        if (newPhase < modifier.getCurrentPhase())
        {
            previousRandom = currentRandom;
            currentRandom = rand.nextFloat();
            randomDifference = currentRandom - previousRandom;
        }

        jassert (isPositiveAndBelow (newPhase, 1.0f));
        modifier.currentPhase.store (newPhase, std::memory_order_release);

        auto getValue = [this, newPhase]
        {
            using namespace PredefinedWavetable;
            switch (getTypedParamValue<LFOModifier::Wave> (*modifier.waveParam))
            {
                case waveSine:          return getSinSample (newPhase);
                case waveTriangle:      return getTriangleSample (newPhase);
                case waveSawUp:         return getSawUpSample (newPhase);
                case waveSawDown:       return getSawDownSample (newPhase);
                case waveSquare:        return getSquareSample (newPhase);
                case fourStepsUp:       return getStepsUpSample (newPhase, 4);
                case fourStepsDown:     return getStepsDownSample (newPhase, 4);
                case eightStepsUp:      return getStepsUpSample (newPhase, 8);
                case eightStepsDown:    return getStepsDownSample (newPhase, 8);
                case random:            return currentRandom;
                case noise:             return (randomDifference * newPhase) + previousRandom;
            }

            return 0.0f;
        };

        float newValue = getValue();
        newValue *= modifier.depthParam->getCurrentValue();
        newValue += modifier.offsetParam->getCurrentValue();

        if (getBoolParamValue (*modifier.bipolarParam))
            newValue = (newValue * 2.0f) - 1.0f;

        modifier.currentValue.store (newValue, std::memory_order_release);
    }

    void resync (EditTimeRange streamTime)
    {
        const auto type = roundToInt (modifier.syncTypeParam->getCurrentValue());

        if (type == ModifierCommon::note)
        {
            ramp.setPosition (0.0f);
            setPhase (0.0f);

            // Move the ramp on for the next block
            ramp.process ((float) streamTime.getLength());
        }
    }

    LFOModifier& modifier;
    Ramp ramp;
    TempoSequencePosition tempoSequence;

    Random rand;
    float previousRandom = 0.0f, currentRandom = 0.0f, randomDifference = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LFOModifierTimer)
};

//==============================================================================
struct LFOModifier::ModifierAudioNode    : public SingleInputAudioNode
{
    ModifierAudioNode (AudioNode* input, LFOModifier& lfo)
        : SingleInputAudioNode (input),
          modifier (&lfo)
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

    LFOModifier::Ptr modifier;
};

//==============================================================================
LFOModifier::LFOModifier (Edit& e, const ValueTree& v)
    : Modifier (e, v)
{
    auto um = &edit.getUndoManager();

    wave.referTo (state, IDs::wave, um, waveSine);
    syncType.referTo (state, IDs::syncType, um, float (ModifierCommon::free));
    rate.referTo (state, IDs::rate, um, 1.0f);
    rateType.referTo (state, IDs::rateType, um, float (ModifierCommon::bar));
    depth.referTo (state, IDs::depth, um, 1.0f);
    bipolar.referTo (state, IDs::bipolar, um);
    phase.referTo (state, IDs::phase, um);
    offset.referTo (state, IDs::offset, um);

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
    waveParam       = addDiscreteParam ("wave",     TRANS("Wave"),      { 0.0f, (float)  getWaveNames().size() - 1 },           wave,       getWaveNames());
    syncTypeParam   = addDiscreteParam ("syncType", TRANS("Sync type"), { 0.0f, (float)  getSyncTypeChoices().size() - 1 },     syncType,   getSyncTypeChoices());
    rateParam       = addParam ("rate",             TRANS("Rate"),      { 0.01f, 50.0f }, 1.0f,                                 rate,       {});
    rateTypeParam   = addDiscreteParam ("rateType", TRANS("Rate Type"), { 0.0f, (float) getRateTypeChoices().size() - 1 },      rateType,   getRateTypeChoices());
    depthParam      = addParam ("depth",            TRANS("Depth"),     { 0.0f, 1.0f }, 0.5f,                                   depth,      {});
    bipolarParam    = addDiscreteParam ("biopolar", TRANS("Bipoloar"),  { 0.0f, 1.0f },                                         bipolar,    { NEEDS_TRANS("Uni-polar"), NEEDS_TRANS("Bi-polar") });
    phaseParam      = addParam ("phase",            TRANS("Phase"),     { 0.0f, 1.0f }, 0.5f,                                   phase,      {});
    offsetParam     = addParam ("offset",           TRANS("Offset"),    { 0.0f, 1.0f }, 0.5f,                                   offset,     {});

    changedTimer.setCallback ([this]
                              {
                                  changedTimer.stopTimer();
                                  changed();
                              });

    state.addListener (this);
}

LFOModifier::~LFOModifier()
{
    state.removeListener (this);
    notifyListenersOfDeletion();

    edit.removeModifierTimer (*modifierTimer);

    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();

    deleteAutomatableParameters();
}

void LFOModifier::initialise()
{
    // Do this here in case the audio code starts using the parameters before the constructor has finished
    modifierTimer = std::make_unique<LFOModifierTimer> (*this);
    edit.addModifierTimer (*modifierTimer);

    restoreChangedParametersFromState();
}

//==============================================================================
float LFOModifier::getCurrentValue()         { return currentValue.load (std::memory_order_acquire); }
float LFOModifier::getCurrentPhase() const   { return currentPhase.load (std::memory_order_acquire); }

AutomatableParameter::ModifierAssignment* LFOModifier::createAssignment (const ValueTree& v)
{
    return new Assignment (v, *this);
}

AudioNode* LFOModifier::createPreFXAudioNode (AudioNode* an)
{
    return new ModifierAudioNode (an, *this);
}

//==============================================================================
LFOModifier::Assignment::Assignment (const juce::ValueTree& v, const LFOModifier& lfo)
    : AutomatableParameter::ModifierAssignment (lfo.edit, v),
      lfoModifierID (lfo.itemID)
{
}

bool LFOModifier::Assignment::isForModifierSource (const ModifierSource& source) const
{
    if (auto* mod = dynamic_cast<const LFOModifier*> (&source))
        return mod->itemID == lfoModifierID;

    return false;
}

LFOModifier::Ptr LFOModifier::Assignment::getLFOModifier() const
{
    if (auto mod = findModifierTypeForID<LFOModifier> (edit, lfoModifierID))
        return mod;

    return {};
}

//==============================================================================
StringArray LFOModifier::getWaveNames()
{
    return
    {
        NEEDS_TRANS("Sine"),
        NEEDS_TRANS("Triangle"),
        NEEDS_TRANS("Saw Up"),
        NEEDS_TRANS("Saw Down"),
        NEEDS_TRANS("Square"),
        NEEDS_TRANS("4 Steps Up"),
        NEEDS_TRANS("4 Steps Down"),
        NEEDS_TRANS("8 Steps Up"),
        NEEDS_TRANS("8 Steps Down"),
        NEEDS_TRANS("Random"),
        NEEDS_TRANS("Noise")
    };
}

//==============================================================================
void LFOModifier::valueTreeChanged()
{
    if (! changedTimer.isTimerRunning())
        changedTimer.startTimerHz (30);
}
