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
/**
    Calculates the RMS of a continuous signal.
    N.B. this isn't a mathematically correct RMS but a reasonable estimate.
*/
class RunningRMS
{
public:
    /** Creates an empty RunningRMS. */
    RunningRMS() = default;

    /** Sets the sample rate to use for detection. */
    inline void setSampleRate (float newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        update();
    }

    /** Returns the current RMS for a new input sample. */
    inline float processSingleSample (float in) noexcept
    {
        state += (juce::square (in) - state) * coefficient;
        return std::sqrt (state);
    }

private:
    float state = 0.0f;
    float coefficient = 1.0f;
    float sampleRate = 44100.0f;

    void update() noexcept
    {
        coefficient = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 100.0f / sampleRate);
        state = 0.0f;
    }
};

//==============================================================================
/**
    Envelope follower with adjustable attack/release parameters as well as several
    detection and time constant modes.
*/
class EnvelopeFollowerModifier::EnvelopeFollower
{
public:
    //==============================================================================
    /** The available detection modes. */
    enum DetectionMode
    {
        peakMode = 0,
        msMode,
        rmsMode
    };

    /** The available time constant modes modes. */
    enum TimeConstantMode
    {
        digitalTC = 0,
        slowDigitalTC,
        analogTC
    };

    //==============================================================================
    /** Creates a default EnvelopeFollower. */
    EnvelopeFollower() = default;

    /** Sets the sample rate to use.
        This should be called before any of the parameter methods.
    */
    void setSampleRate (float newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        rms.setSampleRate (newSampleRate);

        attackCoeff     = getDelta (attackTimeMs);
        releaseCoeff    = getDelta (releaseTimeMs);
        holdSamples     = (int) msToSamples (holdTimeMs, sampleRate);

        reset();
    }

    /** Resets the detection and current envelope. */
    void reset() noexcept
    {
        envelope = 0.0f;
    }

    //==============================================================================
    /** Sets the attack time. */
    void setAttackTime (float attackMs)
    {
        attackTimeMs = attackMs;
        attackCoeff = getDelta (attackTimeMs);
    }

    /** Sets the hold time. */
    void setHoldTime (float holdMs)
    {
        holdTimeMs = holdMs;
        holdSamples = (int) msToSamples (holdMs, sampleRate);
    }

    /** Sets the release time. */
    void setReleaseTime (float releaseMs)
    {
        releaseTimeMs = releaseMs;
        releaseCoeff = getDelta (releaseTimeMs);
    }

    /** Sets the detection mode to use. */
    void setDetectMode (DetectionMode newDetectionMode)
    {
        detectMode = newDetectionMode;
    }

    /** Sets the time constant to be used. */
    void setTimeConstantMode (TimeConstantMode newTC)
    {
        if (! setIfDifferent (timeConstantMode, newTC))
            return;

        switch (timeConstantMode)
        {
            case digitalTC:     timeConstant = -2.0f; break; // log (1%)
            case slowDigitalTC: timeConstant = -1.0f; break; // log (10%)
            case analogTC:      timeConstant = -0.43533393574791066201247090699309f; break; // log (36.7%)
        }

        attackCoeff   = getDelta (attackTimeMs);
        releaseCoeff  = getDelta (releaseTimeMs);
    }

    //==============================================================================
    /** Returns the envelope for a new sample. */
    float processSingleSample (float input)
    {
        switch (detectMode)
        {
            case peakMode:
            {
                input = std::abs (input);
                break;
            }
            case msMode:
            {
                input = juce::square (std::abs (input));
                break;
            }
            case rmsMode:
            {
                input = rms.processSingleSample (input);
                break;
            }
        }

        if (input > envelope)
        {
            envelope = attackCoeff * (envelope - input) + input;
            holdSamplesLeft = holdSamples;
        }
        else if (holdSamplesLeft > 0)
        {
            --holdSamplesLeft;
        }
        else
        {
            envelope = releaseCoeff * (envelope - input) + input;
        }

        jassert (! std::isnan (envelope));
        jassert (envelope == 0.0f || std::isnormal (envelope));
        envelope = juce::jlimit (0.0f, 1.0f, envelope);

        return envelope;
    }

protected:
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float envelope = 0.0f;
    float attackTimeMs = 0.0f;
    float holdTimeMs = 0.0f;
    float releaseTimeMs = 0.0f;
    float sampleRate = 44100.0f;
    float timeConstant = -2.0f;
    int holdSamples = 0, holdSamplesLeft = 0;
    DetectionMode detectMode = peakMode;
    TimeConstantMode timeConstantMode = digitalTC;
    RunningRMS rms;

    inline float getDelta (float timeMs)
    {
        return std::exp (timeConstant / (timeMs * sampleRate * 0.001f));
    }

    inline double msToSamples (double numMiliseconds, double sr)
    {
        return numMiliseconds * 0.001 * sr;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopeFollower)
};


//==============================================================================
EnvelopeFollowerModifier::EnvelopeFollowerModifier (Edit& e, const juce::ValueTree& v)
    : Modifier (e, v)
{
    auto um = &edit.getUndoManager();

    gainDb.referTo (state, IDs::gainDb, um, 0.0f);
    attack.referTo (state, IDs::attack, um, 100.0f);
    hold.referTo (state, IDs::hold, um, 0.0f);
    release.referTo (state, IDs::release, um, 500.0f);
    depth.referTo (state, IDs::depth, um, 1.0f);
    offset.referTo (state, IDs::offset, um, 0.0f);
    lowPassEnabled.referTo (state, IDs::lowPassEnabled, um, false);
    highPassEnabled.referTo (state, IDs::highPassEnabled, um, false);
    lowPassFrequency.referTo (state, IDs::lowPassFrequency, um, 2000.0f);
    highPassFrequency.referTo (state, IDs::highPassFrequency, um, 500.0f);

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

    const juce::NormalisableRange<float> freqRange (20.0f, 20000.0f);

    gainDbParam             = addParam          ("gainDb",              TRANS("Gain"),                  { -20.0f, 20.0f, 0.001f }, 0.0f,    gainDb,             "dB");
    attackParam             = addParam          ("attack",              TRANS("Attack"),                { 1.0f, 5000.0f }, 50.0f,           attack,             "ms");
    holdParam               = addParam          ("hold",                TRANS("Hold"),                  { 0.0f, 5000.0f }, 50.0f,           hold,               "ms");
    releaseParam            = addParam          ("release",             TRANS("Release"),               { 1.0f, 5000.0f }, 50.0f,           release,            "ms");
    depthParam              = addParam          ("depth",               TRANS("Depth"),                 { -1.0f, 1.0f }, 0.0f,              depth,              {});
    offsetParam             = addParam          ("offset",              TRANS("Offset"),                { 0.0f, 1.0f }, 0.5f,               offset,             {});
    lowPassEnabledParam     = addDiscreteParam  ("lowPassEnabled",      TRANS("Low-pass Enabled"),      { 0.0f, 1.0f },                     lowPassEnabled,     modifier::getEnabledNames());
    highPassEnabledParam    = addDiscreteParam  ("highPassEnabled",     TRANS("High-pass Enabled"),     { 0.0f, 1.0f },                     highPassEnabled,    modifier::getEnabledNames());
    lowPassFrequencyParam   = addParam          ("lowPassFrequency",    TRANS("Low-pass Frequency"),    freqRange, 700.0f,                  lowPassFrequency,   "Hz");
    highPassFrequencyParam  = addParam          ("highPassFrequency",   TRANS("High-pass Frequency"),   freqRange, 700.0f,                  highPassFrequency,  "Hz");

    changedTimer.setCallback ([this]
                              {
                                  changedTimer.stopTimer();
                                  changed();
                              });

    state.addListener (this);

    envelopeFollower = std::make_unique<EnvelopeFollower>();
}

EnvelopeFollowerModifier::~EnvelopeFollowerModifier()
{
    state.removeListener (this);
    notifyListenersOfDeletion();

    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();

    deleteAutomatableParameters();
}

//==============================================================================
float EnvelopeFollowerModifier::getCurrentValue()
{
    return (getEnvelopeValue() * depthParam->getCurrentValue()) + offsetParam->getCurrentValue();
}

AutomatableParameter::ModifierAssignment* EnvelopeFollowerModifier::createAssignment (const juce::ValueTree& v)
{
    return new Assignment (v, *this);
}

juce::StringArray EnvelopeFollowerModifier::getAudioInputNames()
{
    return { TRANS("Audio input left"),
             TRANS("Audio input right") };
}

void EnvelopeFollowerModifier::initialise (double newSampleRate, int)
{
    prepareToPlay (newSampleRate);
}

void EnvelopeFollowerModifier::deinitialise()
{
    reset();
}

void EnvelopeFollowerModifier::applyToBuffer (const PluginRenderContext& pc)
{
    setEditTime (pc.editTime.getStart());
    
    if (pc.destBuffer == nullptr)
        return;

    updateParameterStreams (pc.editTime.getStart());

    juce::AudioBuffer<float> ab (pc.destBuffer->getArrayOfWritePointers(),
                           pc.destBuffer->getNumChannels(),
                           pc.bufferStartSample,
                           pc.bufferNumSamples);
    processBlock (ab);
}

//==============================================================================
EnvelopeFollowerModifier::Assignment::Assignment (const juce::ValueTree& v, const EnvelopeFollowerModifier& evm)
    : AutomatableParameter::ModifierAssignment (evm.edit, v),
      envelopeFollowerModifierID (evm.itemID)
{
}

bool EnvelopeFollowerModifier::Assignment::isForModifierSource (const ModifierSource& source) const
{
    if (auto* mod = dynamic_cast<const EnvelopeFollowerModifier*> (&source))
        return mod->itemID == envelopeFollowerModifierID;

    return false;
}

EnvelopeFollowerModifier::Ptr EnvelopeFollowerModifier::Assignment::getEnvelopeFollowerModifier() const
{
    if (auto mod = findModifierTypeForID<EnvelopeFollowerModifier> (edit, envelopeFollowerModifierID))
        return mod;

    return {};
}

//==============================================================================
void EnvelopeFollowerModifier::prepareToPlay (double newSampleRate)
{
    envelopeFollower->setSampleRate ((float) newSampleRate);
}

void EnvelopeFollowerModifier::processBlock (const juce::AudioBuffer<float>& ab)
{
    envelopeFollower->setAttackTime (attackParam->getCurrentValue());
    envelopeFollower->setHoldTime (holdParam->getCurrentValue());
    envelopeFollower->setReleaseTime (releaseParam->getCurrentValue());
    auto gainVal = juce::Decibels::decibelsToGain (gainDbParam->getCurrentValue());
    const bool lowPass = getBoolParamValue (*lowPassEnabledParam);
    const bool highPass = getBoolParamValue (*highPassEnabledParam);

    const int numChannels = ab.getNumChannels();
    const int numSamples = ab.getNumSamples();
    const float* const* samples = ab.getArrayOfReadPointers();
    float envelope = 0.0f;

    AudioScratchBuffer scratch (1, numSamples);
    float* scratchData = scratch.buffer.getWritePointer (0);

    // Find max of all channels
    for (int i = 0; i < numSamples; ++i)
    {
        float sample = 0.0f;

        for (int c = 0; c < numChannels; ++c)
            sample = std::max (sample, std::abs (samples[c][i]));

        scratchData[i] = sample * gainVal;
    }

    // Filter if required
    if (setIfDifferent (currentLowPassFrequency, lowPassFrequencyParam->getCurrentValue()))
        lowPassFilter.setCoefficients (juce::IIRCoefficients::makeLowPass (getSampleRate(), currentLowPassFrequency));

    if (setIfDifferent (currentHighPassFrequency, highPassFrequencyParam->getCurrentValue()))
        highPassFilter.setCoefficients (juce::IIRCoefficients::makeHighPass (getSampleRate(), currentHighPassFrequency));

    if (lowPass)
        lowPassFilter.processSamples (scratchData, numSamples);

    if (highPass)
        highPassFilter.processSamples (scratchData, numSamples);

    // Process envelope
    for (int i = 0; i < numSamples; ++i)
        envelope = envelopeFollower->processSingleSample (scratchData[i]);

    jassert (! std::isnan (envelope));
    envelopeValue.store (envelope, std::memory_order_release);
}

void EnvelopeFollowerModifier::reset()
{
    envelopeFollower->reset();
}

void EnvelopeFollowerModifier::valueTreeChanged()
{
    if (! changedTimer.isTimerRunning())
        changedTimer.startTimerHz (30);
}

}} // namespace tracktion { inline namespace engine
