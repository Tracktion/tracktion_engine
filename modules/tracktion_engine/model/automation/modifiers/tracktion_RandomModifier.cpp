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

struct RandomModifier::RandomModifierTimer    : public ModifierTimer
{
    RandomModifierTimer (RandomModifier& rm)
        : modifier (rm)
    {
    }

    void updateStreamTime (TimePosition editTime, int numSamples) override
    {
        const double blockLength = numSamples / modifier.getSampleRate();
        modifier.setEditTime (editTime);
        modifier.updateParameterStreams (editTime);

        const auto syncTypeThisBlock = juce::roundToInt (modifier.syncTypeParam->getCurrentValue());
        const auto rateTypeThisBlock = getTypedParamValue<ModifierCommon::RateType> (*modifier.rateTypeParam);

        const float rateThisBlock = modifier.rateParam->getCurrentValue();

        if (rateTypeThisBlock == ModifierCommon::hertz)
        {
            const float durationPerPattern = 1.0f / rateThisBlock;
            ramp.setDuration (durationPerPattern);

            if (syncTypeThisBlock == ModifierCommon::transport)
                ramp.setPosition (std::fmod ((float) editTime.inSeconds(), durationPerPattern));

            modifier.setPhase (ramp.getProportion());

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
                    modifier.setPhase ((float) std::fmod (virtualBars, 1.0f));
                }
            }
            else
            {
                const double bpm = (currentTempo * rateThisBlock) / proportionOfBar;
                const double secondsPerBeat = 60.0 / bpm;
                const float secondsPerStep = static_cast<float> (secondsPerBeat * currentTimeSig.numerator);
                const float secondsPerPattern = secondsPerStep;
                ramp.setDuration (secondsPerPattern);

                modifier.setPhase (ramp.getProportion());

                // Move the ramp on for the next block
                ramp.process ((float) blockLength);
            }
        }
    }

    void resync (double duration)
    {
        if (juce::roundToInt (modifier.syncTypeParam->getCurrentValue()) == ModifierCommon::note)
        {
            ramp.setPosition (0.0f);
            modifier.setPhase (0.0f);

            // Move the ramp on for the next block
            ramp.process ((float) duration);
        }
    }

    RandomModifier& modifier;
    Ramp ramp;
    tempo::Sequence::Position tempoSequence { createPosition (modifier.edit.tempoSequence) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RandomModifierTimer)
};

//==============================================================================
RandomModifier::RandomModifier (Edit& e, const juce::ValueTree& v)
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
                            juce::NormalisableRange<float> valueRange,
                            float centreVal, juce::CachedValue<float>& val,
                            const juce::String& suffix) -> AutomatableParameter*
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

AutomatableParameter::ModifierAssignment* RandomModifier::createAssignment (const juce::ValueTree& v)
{
    return new Assignment (v, *this);
}

void RandomModifier::applyToBuffer (const PluginRenderContext& prc)
{
    if (prc.bufferForMidiMessages == nullptr)
        return;
    
    for (auto& m : *prc.bufferForMidiMessages)
        if (m.isNoteOn())
            modifierTimer->resync (prc.bufferNumSamples / getSampleRate());
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
juce::StringArray RandomModifier::getTypeNames()
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
        const auto randomRange = juce::Range<float> (currentRandom - sd, currentRandom + sd)
                                    .getIntersectionWith ({ bp ? -1.0f : 0.0f, 1.0f });

        currentRandom = juce::jmap (rand.nextFloat(), randomRange.getStart(), randomRange.getEnd());
        jassert (randomRange.contains (currentRandom));
        randomDifference = currentRandom - previousRandom;
    }

    jassert (juce::isPositiveAndBelow (newPhase, 1.0f));
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

}} // namespace tracktion { inline namespace engine
